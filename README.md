# Account-wide Mounts

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that shares mounts between
the characters on an account. Learn a mount on one character, and your other characters know it
the next time they log in.

It only shares mounts. It's the mount part of
[warblups/mod-accountwide](https://github.com/warblups/mod-accountwide), split out so you can
have it without that module's achievement, reputation, currency and PvP sharing. Those clash
with [mod-individual-progression](https://github.com/Grimfeather/mod-individual-progression):
a shared raid-boss achievement moves an alt's progression up to your main's. Companion pets are
in a separate module, [mod-accountwide-pets](https://github.com/buildthehomelab/wow-mod-accountwide-pets).

## What gets shared

- Every mount on the client's Mounts tab, including the ones that switch between ground and
  flying by riding skill (Big Blizzard Bear, the Headless Horseman's Mount).
- Not class mounts. Paladin chargers, warlock steeds and the death knight's deathcharger stay
  with their class.
- Not the other faction's mounts, as long as `AccountWideMounts.RespectItemRestrictions` is on
  (the default). A mount only goes to characters that could use the item it's learned from, so
  your Horde characters don't get the Alliance rams.

Mounts are never taken away. A character that already knows a mount keeps it whatever the
settings say.

Bots from mod-playerbots are skipped both ways: they don't add mounts to their account and don't
get taught any.

## Requirements

- AzerothCore wotlk (master) with the WotLK 3.3.5a (12340) client.
- No client patch or addon. The table is created in the characters database automatically.
- Works with [mod-individual-progression](https://github.com/ZhengPeiRu21/mod-individual-progression),
  which is the reason this is split out of mod-accountwide.
- Optional: [mod-playerbots](https://github.com/mod-playerbots/mod-playerbots). Bots are skipped
  whether or not it is installed; the module detects them with `WorldSession::IsHeadless()` when
  the core has it.

## Install

Clone it into your AzerothCore `modules` folder **as `mod-accountwide-mounts`**, without the
repo's `wow-` prefix. AzerothCore finds the module's entry point from the folder name.

```bash
cd <azerothcore>/modules
git clone https://github.com/buildthehomelab/wow-mod-accountwide-mounts.git mod-accountwide-mounts
```

Rebuild the worldserver, then copy `conf/mod_accountwide_mounts.conf.dist` to your config folder
as `mod_accountwide_mounts.conf`. The table is created in the characters database on the next
start, as long as `Updates.EnableDatabases` still includes the characters database (it does by
default).

## Coming from mod-accountwide

This module uses mod-accountwide's `accountwide_mounts` table, so every mount it already recorded
is kept. Either remove mod-accountwide, or keep it for the other features and set
`AccountWide.Mounts = 0` so the two don't both do the work.

## Settings

| Setting | Default | What it does |
|---------|---------|--------------|
| `AccountWideMounts.Enable` | `1` | Master switch. |
| `AccountWideMounts.RespectItemRestrictions` | `1` | Only teach a mount to characters that could use the item it's learned from. `0` shares every mount with every character. |

## How it works

- **Login:** the character's own mounts are added to the account first. Then it's taught every
  account mount it doesn't know yet and is allowed to have.
- **Learning a mount:** it's added to the account right away, so a character that logs in
  meanwhile gets it too.
- **Logout:** saves once more.
- **Deleting the account's last character:** the account's mount list is cleared.

## Troubleshooting

- **The table was not created.** It is added by an update file in the characters database, which
  only runs while `Updates.EnableDatabases` includes the characters database (it does by default).
- **An alt did not get a mount.** Class mounts stay with their class, and mounts for the other
  faction are skipped while the restriction setting is on. Each character adds its own mounts to the
  account when it logs in, so log in on the character that has them first, then on your alts.
- **Two modules are doing the same job.** If mod-accountwide is still installed, remove it or set
  `AccountWide.Mounts = 0` in its config.
- **To share everything regardless of faction or class.** Set `AccountWideMounts.RespectItemRestrictions = 0`.

## Credits

Based on [warblups/mod-accountwide](https://github.com/warblups/mod-accountwide) by warblups, which
this is split out of.

Author: [buildthehomelab](https://github.com/buildthehomelab)

## License

GNU AGPL v3, the same as mod-accountwide, which this is based on. See [LICENSE](LICENSE).
