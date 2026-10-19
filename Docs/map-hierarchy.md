# Map hierarchy

Map Editor's Map Tree contains one Empty Root node. Empty nodes, glTF instances,
PlayerStartPoint, enemies and clear areas can be children of any existing node.
All saved positions and rotations are relative to the parent. The hierarchy describes
initial map placement; runtime tank movement does not animate authored child nodes. Mesh and Empty
nodes also have local XYZ scale. Vehicle dimensions remain defined by the tank
configuration; parent scale changes spawn positions, not vehicle dimensions.

Select a node to edit its local transform. Add Empty Node creates a child of the
selection. New meshes, enemies and clear areas are also added below the selection.
Change Parent or use the shared SceneTreeUI drag-and-drop rows. The top quarter
inserts before the target, the middle half appends as its last child, and the bottom
quarter inserts after the target. Sibling insertion shows a line; parenting highlights
the target row. Invalid targets show a red outline and cannot accept the drop.
Sibling order is stored in treeOrder across all element types. Reparenting keeps
local values, so the world position can change. Root cannot be reparented or
removed. Move children elsewhere before deleting their parent.

Viewport gizmos operate in world space and convert the result back into local
coordinates. A manipulation that would require local shear is rejected. Mesh
rendering and collision retain exact composed affine matrices, including shear
from rotated children of nonuniformly scaled parents. Clear areas preserve the
same affine transform for their unit-box boundary and point containment test.

Manifest version 2 stores Empty nodes in `nodes` and parent references in
`parentId` on placed elements and `playerSpawn`. Internal tree IDs are `root`,
`player-start`, `mesh/<instance-id>`, `enemy/<enemy-id>`, `area/<clear-area-id>`,
and the IDs of Empty nodes. Version 1 remains readable: existing elements become
children of an identity Root. Saving writes version 2. Cycles, missing parents,
duplicate node IDs and hierarchies deeper than 128 ancestors are rejected.

## Common node information

Map Tree owns Selected, Active, Visible in editor, Duplicate and Remove Selected.
Active controls gameplay inclusion, including collision meshes and enemy spawning.
Visible in editor controls only the editor preview. Both flags are saved in
nodeInfo and inherited from parents. Inactive nodes can still be edited when visible.
Root cannot be removed, duplicated or deactivated.

StartPoints can be duplicated. Copies are initially inactive; activating another
StartPoint automatically deactivates the previous one. Exactly one StartPoint must
remain active, and its ancestors must be active. Gameplay spawns the player at that
point's world transform. The last StartPoint cannot be removed. Removing an active
StartPoint selects another eligible start. Additional start poses are saved in
startPoints; the original playerSpawn fields remain readable for legacy maps.

Duplicate copies a subtree with fresh IDs and preserved local poses/basic flags.
Cloned StartPoints are always inactive. Duplicated enemies do not automatically
join the source enemy's command group. Move/remove children before removing a parent.
## Enemy Unit Editor window

Use the Enemy Unit Editor checkbox in Map Editor or the Tracked Vehicle play
panel to show or hide the independent window. Both modes share the same editor implementation and JSON
format. Map Editor edits the loaded enemy catalog and updates its placement-type
choices while the window is open. Save enemy types JSON before starting play;
loading gameplay reads the map's enemy_types.json, or Config/enemy_types.json
when no map-local catalog exists. Save the file inside the map for map-specific types.
During play, simulation settings apply live. Apply & Reset applies attack/unit edits
and resets the tank/world. Runtime movement status is shown only during play.
## Viewport selection

Left-click visible mesh surfaces, enemies, StartPoint markers or ClearArea wireframes
in the 3D view to select their Tree node. The nearest triangle hit wins; hidden
objects, grid lines and selection outlines are excluded. The Tree opens ancestors
and scrolls to the selected row. Left-drag still orbits the camera, and UI/Gizmo
gestures do not trigger viewport selection. Clicking empty space keeps selection.
Empty nodes have no pickable geometry and are selected in the Tree.

Hold Shift while translating with the Gizmo to enable grid snapping temporarily,
using Grid Spacing. The Grid Snap checkbox also enables snapping without Shift.

Right-click a Tree row for Delete, Duplicate and Rename. Root cannot be deleted
or duplicated; nodes with children cannot be deleted. Rename changes the display
name only, preserving IDs and references; names are stored in nodeInfo.name.
The Rename dialog accepts Enter to apply and Escape/Cancel to discard edits.

## Map Editor Camera

Map Editor has its own Map Editor Camera window, shown when a map is open.
Use Camera Window in the editor to reopen it after closing. It controls only the
Debug orbit camera: position, look-at point, Perspective/Orthographic projection,
FOV/orthographic height, lens shift, Near Z/Far Z clipping distances and mouse speed. Reset View restores the
initial editor view. Tank camera slots and Follow Tank controls are excluded.
These controls do not alter the tank camera slot selection or stored presets.

## Breakable meshes

Select a Mesh in the Tree and enable Breakable, then set HP (default 60) and Save.
This is a placement setting, independent of the glTF file name or color. Existing
maps default to unbreakable. Duplicate preserves the setting and HP; the manifest
stores instances[].breakable and instances[].hitPoints.

During play, the transformed Hit triangle mesh is registered as a static combat
target, separate from enemy AI. Ordinary rounds, mortar blasts and enemy bullets
can damage it. At zero HP its collider, Visual geometry and Hit overlay disappear.
Reset restores the authored geometry and HP. The initial implementation removes
the whole placement; it does not generate fragments or simulate a collapse.
Mortar blast distance uses the placement's world AABB, as for existing box targets.

Enemy Unit Editor also exposes HP under each Enemy unit type (default 60).
It is stored as unitTypes[].hitPoints; old catalogs without the field retain 60.
Save enemy types JSON to persist it. During play use Apply & Reset to respawn
enemies with the new type HP; Map Editor changes apply after saving and starting play.

## Parent support for enemies

Select an Enemy in the Tree and enable Supported by parent. Parent it underneath
a Breakable Mesh (intermediate Empty groups are allowed). The nearest Breakable
ancestor becomes its support; the link survives flattening for gameplay. With no
Breakable ancestor the flag has no effect. Existing placements default to OFF.

When the support is destroyed, the enemy keeps its world pose and becomes a
Jolt dynamic body, falling and colliding under gravity. Navigation no longer moves
it after release; HP, identity and attacks are retained. Ordinary turret proxies
can sleep after settling. Reset reloads the original pose and support relationship.
This initial feature applies to Enemy elements; arbitrary Mesh children are not
converted into falling rigid bodies.

Use Focus to selected object [F] in Map Editor Camera, or press F in Map Editor,
to center and frame the selected item. The viewing direction and projection are
preserved, including exact XZ/XY/YZ views. Text input, active controls, popups and
Gizmo drags suppress the shortcut. StartPoint and ClearArea markers are supported;
Empty nodes focus their world origin.

Focus framing uses the enclosing sphere at 50% of the limiting viewport dimension
(width or height), providing more surrounding context in both projection modes.

The Map Editor Sub Window collapsing header groups Enemy Unit Editor, Camera
Window, Cheat Sheet and Output toggles. Output shares its state with F9 and the
Output window close button.

## Clear Area transforms

Clear Area is a centered 1 x 1 x 1 meter unit box. Create it with Add Clear Area
in the Map Tree, then use the shared Position/Rotation/Scale inspector or Gizmo.
There is no separate Clear Area transform panel. XYZ Scale corresponds to the
local XYZ dimensions; parent rotation and scale are preserved in both the visible
boundary and the clear condition. Tests transform the player point into the same
unit box, including nonuniform parent transforms. New saves use position,
rotationDegrees and scale; legacy center/size entries remain readable.
