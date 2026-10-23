# Map names

Manifest maps have a `name` in `Manifest.json`. The name is the public reference
key for loading maps; the folder is a storage location. Editor and game map
lists use an ImGui table with Name, Directory and Playable columns.
Playable ON is shown with a green checkmark. Runtime telemetry uses the map name.

In Map Editor, open a map, edit **Map Name**, press Enter or **Apply Name**, then
**Save**. Applying is one Undo/Redo command. Undo also restores the name field
and the list label. Save writes the manifest; it does not rename the folder.
The editor rejects a name already used by another Manifest map in Assets/Map,
including names inherited from legacy folders. Saving checks again, including
after Undo/Redo or changes to other maps on disk.

Names are exact, case-sensitive UTF-8 strings (1-256 bytes), with no surrounding
ASCII whitespace or control characters. Japanese names and spaces inside names
are supported. Missing `name` is accepted for old manifests and uses the folder
name. Opening an old map gives it that default in memory; saving makes the name
explicit, so future folder renames do not affect references. Existing map assets
are not rewritten automatically. An explicit empty or invalid name is rejected.

## Playable

`playable` is a JSON boolean. Missing values default to true for existing maps;
new maps also start with Playable ON. In Map Editor, the open map's **Playable**
checkbox changes it as one Undo/Redo command. Save writes the new value.
The editor table reflects unsaved changes immediately; the game table refreshes
from saved maps when returning to the menu or reloading the map list.

OFF maps remain listed in the editor and can be edited, but are omitted from
the game start menu. Both
`--map-name` and `--map` also reject loading an OFF Manifest map into the game.
Name resolution itself remains available for editor/tool use. Malformed values
such as strings or numbers are rejected instead of interpreted as booleans.

Load a Manifest map by its saved name:

```bat
scripts\run.bat --scene tracked-vehicle --map-name "Training Ground"
```

`--map` remains available for explicit file/folder paths. Do not combine it with
`--map-name`. Name lookup searches Manifest maps immediately under Assets/Map;
it does not include built-in procedural scenes or legacy standalone physics JSON
files. Lookup reports missing and duplicate names instead of choosing silently.
A malformed/unreadable manifest aborts catalog loading with its path and error.

`Tank::Map::LoadMapCatalog` and `ResolveMapByName` provide the same resolution for
future map transitions and scripting. Rebuild the catalog after saved changes.
References should store the name and ask the catalog for the folder/document;
they should not construct paths from the name. Renaming changes the reference
key: update authored references to the new name. No transition/script system or
automatic reference migration is introduced in this change.

## Playable verification

MapManifestTests, MapUndoHistoryTests, MapCatalogTests and MapFolderTests passed.
Tests cover default/legacy ON, strict boolean validation, OFF save/reload,
editor catalog access and Undo/Redo without overwriting disk.

Windows GUI checks on a temporary map confirmed the three-column tables,
ON/OFF checkmarks, Undo/Redo, Save, OFF hidden from the game menu, ON appearing
after Save and returning to the menu, and OFF rejection through both --map-name
and --map. The temporary map was removed after verification.

Debug compile/link succeeded in build/PlayableValidation. The normal Debug app
was running, so its locked runtime files prevented standard post-build copying.
Validation used existing runtime assets copied to the separate output and a
compile/link pass with PostBuildEventUseInBuild=false; no deployment rules changed.
