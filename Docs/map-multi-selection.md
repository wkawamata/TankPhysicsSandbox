# Map Editor multiple selection

- Left-click in the viewport or Map Tree selects one element. Shift + left-click
  toggles an element in the selection. A plain click on empty viewport space
  clears it; Shift-clicking empty space leaves it unchanged.
- All selected elements are highlighted. The last selected element is active and
  hosts the single Move/Rotate/Scale gizmo.
- Move applies the same world displacement to each selected element. Rotate and
  Scale keep each selected element's world position and use its individual
  origin. Selected parents are processed before selected children, so a selected
  child is not moved twice. Unselected descendants inherit their parent's edit.
- Local rotates each element about its own local axes. Scale multiplies each
  element's local-axis dimensions. Existing hierarchy shear checks still apply:
  an unrepresentable edit fails atomically with an explanatory status message.
- Enemy and StartPoint sizes retain their existing fixed-size restriction. Scale
  is disabled when either is selected. Models, Empty nodes and Clear Areas can
  be scaled together.
- Duplicate Selected / Ctrl+D duplicates all selected subtrees. A child whose
  parent is also selected is copied once as part of that subtree.
- Remove Selected / Delete removes the explicit selection, children first. As
  with single deletion, include or move all child nodes before removing a parent.
  Root and the last StartPoint remain protected. A failed batch changes nothing.
- One continuous gizmo drag is one Undo/Redo command. A batch duplication or
  deletion is also one command and restores its selection on Undo/Redo.
- Rectangle selection is not implemented. Left drag retains the camera controls.
