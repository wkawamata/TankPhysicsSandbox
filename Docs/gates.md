# Proximity shutter gates

Map Editor > Gate offers clear openings of 10 x 3 m, 5 x 3 m and 20 x 5 m.
Add Gate creates one named group with a fixed frame and a movable shutter.
Dummy glTF models include both Visual and Hit roles and are copied to Models/
inside the map. Existing files are never overwritten.

Move, rotate, scale, duplicate or remove the Gate group in Map Tree to keep both
parts together. Select the group or its Gate shutter child to edit Open radius (m)
with the slider and decrement/increment/reset buttons. The default is 12 m.
Save writes the gate settings to Manifest.json. Placement, size selection and
radius edits support Undo/Redo; continuous radius edits form one command.

During play the gate starts closed. When the player chassis center enters the
3D sphere centered at the shutter base, the gate slides along its local +Y axis.
The clear opening dimensions exclude the 0.5 m frame posts and lintel.
Travel is opening height + 0.2 m and speed is 2 m/s, stored as travelMeters and
openingSpeedMetersPerSecond in the shutter instance's optional gate object.
The trigger radius is world meters, independent of scale. Rotated/scaled parent
transforms affect travel, visuals and collision consistently.

Opening remains latched when the player leaves the radius. Replay starts closed.
Closing conditions and a general event editor are future work. GateState keeps
the trigger latch separate from slide progress for future event integration.
The gate is indestructible; its collision is a kinematic convex hull (the dummy
shutter is a box). The fixed frame retains its triangle collision. Visuals and
Hit overlays use the same world offset as the collision. Editor previews remain
closed; proximity only runs during gameplay. Do not parent other gameplay
objects to the shutter expecting them to follow its runtime motion yet.

GateTests covers prefab dimensions, glTF collision loading, persistence,
duplication, invalid settings, grouped radius Undo/Redo, spherical trigger,
latched slide motion, a closed gate blocking a driven tank, an open gate allowing
it through, and reset. MapManifestTests and MapUndoHistoryTests also pass.

GUI verification used a separate map under build/GateGuiValidation. The 10 m
gate renders with its frame; the radius increment button changes 12 to 12.5 m
and Undo restores 12 m. Gameplay visibly raises the shutter above the opening.
No user map was saved or changed during these checks.
