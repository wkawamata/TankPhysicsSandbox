# Editor Undo

The Map Editor has an Undo button beside Back to Menu and a Ctrl+Z shortcut.
The Tracked Vehicle panel has an Undo button under Tank Settings and the same
shortcut. Text inputs retain their normal text-editing shortcut while active.

## Supported edits

- Map hierarchy: local transforms, parenting and sibling order, active/visible
  states, names, subtree duplication, and node creation/removal. Parent removal
  retains the hierarchy editor's rule to move or remove its children first.
- Map objects: models, player starts, enemy placements and clear areas, including
  transform gizmos and supported/breakable object parameters.
- Enemy Unit Editor in Map Editor: movement settings, attack definitions, unit
  names and HP, and attack mounts. These share the map's command history.
- Enemy Unit Editor in Tracked Vehicle: movement, attacks, unit types and mounts.
- Tank appearance: materials, colors, model scale and dummy visibility; restored
  appearance refreshes materials and the scene.
- Input Mapping: device axis/button assignments, inversion, neutral/tolerance and
  deadzone settings, plus complete profile replacement via Load Mapping. The
  profile collection is restored safely across device switches and reloads.
  Keyboard bindings currently have no editing UI. Save Mapping still writes disk
  explicitly and is not undone.
- Ground/environment: floor, friction, grid, colors and obstacle generation settings.
  Apply & Reset remains explicit, as for physics and enemy simulation changes.
- MyTank in Tracked Vehicle: editable TankSettings, including chassis, engine,
  transmission, suspension, rolling, mortar and assault projectile parameters.
  Profile loads and completed rolling optimization results are batch edits.

A continuous edit of one control is one command. Switching controls ends the
previous command. Returning a control to its initial value adds no command.

## Sessions and saving

History has no fixed count limit and stays in process memory. Each command owns
its required snapshots. Memory usage therefore grows with the number and size
of edits until the session is cleared or the process exits.

A successful map open, Discard, or Back to Menu clears the map editing history.
Leaving Tracked Vehicle clears its separate MyTank history. Camera and other preview preferences are outside the current Undo scope.

Save does not clear history. Undo after saving changes the in-memory document
and updates its dirty state; it does not overwrite the saved file. Material
asset writes, JSON saves, exports and file deletion are not undoable operations.
Enemy catalog edits must still be saved using Save enemy types JSON.

MyTank Undo restores editable values. Settings that require Apply & Reset keep
that behavior: Undo does not rewind simulation, restore a prior physics world,
or automatically reset the tank. Live projectile settings continue to use the
restored edited values through the existing live-application path.

## Extension points

IUndoCommand provides named Undo and Redo actions. UndoHistory can execute a new
command or record an immediate-mode UI edit that has already been applied.
Adding a new command after Undo discards its redo branch. Redo is available in
the history API; this UI increment exposes Undo only.

SnapshotUndoEdit groups widget interactions. ManifestUndoEdit compares validated
manifest serialization, while enemy and tank settings compare their values,
so temporary invalid text such as an empty enemy name remains undoable.

## Verification

The TankApp.UndoHistory, TankApp.TankSettingsUndo and TankApp.MapUndoHistory
CTest cases cover command branching, 10,000 edits, interaction grouping,
field switching, nested settings, hierarchy snapshots, saved-file preservation
and session cleanup. Representative Map Editor, enemy HP and MyTank edits are
also checked through the Windows UI with the Undo button and Ctrl+Z.
