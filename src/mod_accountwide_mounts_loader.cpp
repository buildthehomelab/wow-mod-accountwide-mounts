/*
 * mod-accountwide-mounts loader.
 *
 * AzerothCore looks up a loader symbol derived from the module's folder name: for folder
 * "mod-accountwide-mounts" that symbol is exactly "Addmod_accountwide_mountsScripts". If you clone the
 * repo under a different folder name, rename this function to match.
 *
 * Released under the GNU AGPL v3, like mod-accountwide.
 */

void AddAccountWideMountsScripts();

void Addmod_accountwide_mountsScripts()
{
    AddAccountWideMountsScripts();
}
