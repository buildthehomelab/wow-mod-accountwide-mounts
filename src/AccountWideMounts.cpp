/*
 * mod-accountwide-mounts
 *
 * A mount learned by one character on an account is taught to the account's other characters the
 * next time they log in. Mounts only: this is the mount part of warblups/mod-accountwide, split out
 * so it can run without that module's achievement, reputation, currency and PvP sharing. It uses
 * the same `accountwide_mounts` table, so mounts that module already recorded carry over.
 *
 * Released under the GNU AGPL v3, like mod-accountwide.
 */

#include "Config.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldSession.h"

#include <type_traits>
#include <unordered_map>
#include <utility>

namespace
{
    struct Config
    {
        bool enabled = true;
        bool respectItemRestrictions = true;
    };

    Config config;

    // Item spells that teach the spell in the item's second spell slot. Mount and companion items
    // use one of these two.
    constexpr int32 SPELL_LEARNING = 483;
    constexpr int32 SPELL_LEARNING_COMPANION = 55884;

    // Races and classes allowed to use the items that teach each mount, OR-ed together when more
    // than one item teaches the same mount. Mounts no item teaches aren't in here.
    struct ItemRestriction
    {
        uint32 raceMask = 0;
        uint32 classMask = 0;
    };

    std::unordered_map<uint32, ItemRestriction> itemRestrictions;

    // Bots are sessions without a socket. AzerothCore marks them with WorldSession::IsHeadless();
    // older playerbots core forks have WorldSession::IsBot() instead, and older stock cores have
    // neither. Looking for both at compile time lets the module build on all of them.
    template <typename Session, typename = void>
    struct HasIsHeadless : std::false_type { };

    template <typename Session>
    struct HasIsHeadless<Session, std::void_t<decltype(std::declval<Session&>().IsHeadless())>> : std::true_type { };

    template <typename Session, typename = void>
    struct HasIsBot : std::false_type { };

    template <typename Session>
    struct HasIsBot<Session, std::void_t<decltype(std::declval<Session&>().IsBot())>> : std::true_type { };

    template <typename Session>
    bool IsBotSession(Session* session)
    {
        if constexpr (HasIsHeadless<Session>::value)
            return session->IsHeadless();
        else if constexpr (HasIsBot<Session>::value)
            return session->IsBot();
        else
            return false;
    }

    // Bots are skipped both ways: they don't add mounts to their account and don't get taught any.
    // With thousands of random bots, teaching them mounts on every login would only cost time.
    bool IsRealPlayer(Player* player)
    {
        WorldSession* session = player ? player->GetSession() : nullptr;
        return session && !IsBotSession(session);
    }

    // A mount is a spell on the Mounts skill line, which is also what puts it on the client's
    // Mounts tab. Checking for the mounted aura instead would miss the mounts that pick a
    // ground or flying version by riding skill (Big Blizzard Bear, the Headless Horseman's
    // mount), and catch quest and vehicle spells that aren't mounts at all.
    bool IsMountSpell(uint32 spellId)
    {
        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
            if (itr->second->SkillLine == SKILL_MOUNTS)
                return true;

        return false;
    }

    // Whether a mount learned on another character may be taught to this one.
    bool CanShareWith(Player* player, uint32 spellId)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || !IsMountSpell(spellId))
            return false;

        // Class mounts (paladin chargers, warlock steeds, the death knight's deathcharger) belong
        // to their class's spell family and are trained, not collected.
        if (spellInfo->SpellFamilyName != SPELLFAMILY_GENERIC)
            return false;

        SkillLineAbilityMapBounds bounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        for (auto itr = bounds.first; itr != bounds.second; ++itr)
        {
            SkillLineAbilityEntry const* ability = itr->second;
            if (ability->SkillLine != SKILL_MOUNTS)
                continue;

            if (ability->RaceMask && !(ability->RaceMask & player->getRaceMask()))
                return false;

            if (ability->ClassMask && !(ability->ClassMask & player->getClassMask()))
                return false;
        }

        // Faction mounts: a Horde character can't use the reins of an Alliance mount, so it
        // doesn't get taught one either.
        if (config.respectItemRestrictions)
        {
            auto itr = itemRestrictions.find(spellId);
            if (itr != itemRestrictions.end())
            {
                ItemRestriction const& restriction = itr->second;
                if (restriction.raceMask && !(restriction.raceMask & player->getRaceMask()))
                    return false;

                if (restriction.classMask && !(restriction.classMask & player->getClassMask()))
                    return false;
            }
        }

        return true;
    }

    void BuildItemRestrictions()
    {
        itemRestrictions.clear();

        for (auto const& [entry, proto] : *sObjectMgr->GetItemTemplateStore())
        {
            int32 learnSpell = proto.Spells[0].SpellId;
            int32 taughtSpell = proto.Spells[1].SpellId;

            if (learnSpell != SPELL_LEARNING && learnSpell != SPELL_LEARNING_COMPANION)
                continue;

            if (taughtSpell <= 0 || !IsMountSpell(taughtSpell))
                continue;

            ItemRestriction& restriction = itemRestrictions[taughtSpell];
            restriction.raceMask |= proto.AllowableRace;
            restriction.classMask |= proto.AllowableClass;
        }

        LOG_INFO("module", "mod-accountwide-mounts: {} mounts are taught by items", itemRestrictions.size());
    }

    void SaveMount(uint32 accountId, uint32 spellId)
    {
        CharacterDatabase.Execute(
            "INSERT IGNORE INTO `accountwide_mounts` (`account_id`, `spell`) VALUES ({}, {})",
            accountId, spellId);
    }

    // Records every mount this character knows, class and faction mounts included. Those are
    // filtered out when teaching instead, per character.
    void SaveMounts(Player* player)
    {
        uint32 accountId = player->GetSession()->GetAccountId();
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        for (auto const& [spellId, spell] : player->GetSpellMap())
        {
            if (!spell || spell->State == PLAYERSPELL_REMOVED || !spell->Active)
                continue;

            if (!IsMountSpell(spellId))
                continue;

            trans->Append(
                "INSERT IGNORE INTO `accountwide_mounts` (`account_id`, `spell`) VALUES ({}, {})",
                accountId, spellId);
        }

        CharacterDatabase.CommitTransaction(trans);
    }

    void TeachMounts(Player* player)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT `spell` FROM `accountwide_mounts` WHERE `account_id` = {}",
            player->GetSession()->GetAccountId());
        if (!result)
            return;

        do
        {
            uint32 spellId = result->Fetch()[0].Get<uint32>();
            if (!player->HasSpell(spellId) && CanShareWith(player, spellId))
                player->learnSpell(spellId);
        }
        while (result->NextRow());
    }
}

class AccountWideMountsWorldScript : public WorldScript
{
public:
    AccountWideMountsWorldScript() : WorldScript("AccountWideMountsWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled = sConfigMgr->GetOption<bool>("AccountWideMounts.Enable", true);
        config.respectItemRestrictions = sConfigMgr->GetOption<bool>("AccountWideMounts.RespectItemRestrictions", true);
    }

    // Items are loaded by now.
    void OnStartup() override
    {
        BuildItemRestrictions();
    }
};

class AccountWideMountsPlayerScript : public PlayerScript
{
public:
    AccountWideMountsPlayerScript() : PlayerScript("AccountWideMountsPlayerScript") { }

    // Save first, so a character that already knows mounts adds them to the account right away
    // instead of on its first logout.
    void OnPlayerLogin(Player* player) override
    {
        if (!config.enabled || !IsRealPlayer(player))
            return;

        SaveMounts(player);
        TeachMounts(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!config.enabled || !IsRealPlayer(player))
            return;

        SaveMounts(player);
    }

    // Covers mounts learned mid-session, so another character logging in meanwhile gets them too.
    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        if (!config.enabled || !IsRealPlayer(player) || !IsMountSpell(spellId))
            return;

        SaveMount(player->GetSession()->GetAccountId(), spellId);
    }

    // Deleting the last character on an account forgets the account's mounts too.
    void OnPlayerDelete(ObjectGuid guid, uint32 accountId) override
    {
        if (!config.enabled)
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT 1 FROM `characters` WHERE `account` = {} AND `guid` <> {} AND `deleteDate` IS NULL LIMIT 1",
            accountId, guid.GetCounter());
        if (result)
            return;

        CharacterDatabase.Execute("DELETE FROM `accountwide_mounts` WHERE `account_id` = {}", accountId);
    }
};

void AddAccountWideMountsScripts()
{
    new AccountWideMountsWorldScript();
    new AccountWideMountsPlayerScript();
}
