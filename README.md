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

## License

GNU AGPL v3, the same as mod-accountwide, which this is based on. See `LICENSE`.
