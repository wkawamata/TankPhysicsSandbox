# Map names

Manifest maps have a `name` in `Manifest.json`. The name is the public reference
key for loading maps; the folder is a storage location. Editor and game map
lists show both as `folder  -  name`. Runtime telemetry uses the map name.

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
