# Enemy Unit Editor controls

Numeric fields use a slider followed by three buttons: < decreases by one step,
> increases by one step, and | restores the field default. Bullet box dimensions
and attack-mount coordinates have separate X/Y/Z or width/height/depth rows.
Name and JSON path inputs remain text fields; type selections remain combos.
Defaults are clamped to the current dependent range so resetting Detection,
Reach, Firing range or movement thresholds preserves their ordering.

## Enemy movement

Enemy movement simulation has an Enemy movement enabled checkbox. Its seven
tuning controls are shown only while ON: physical-entry/return distances,
braking, node arrival, observation radius, support-height tolerance and simple
pivot speed. OFF retains these values in memory and in the catalog JSON.

The checkbox is saved as simulation.movementEnabled. Missing values default to
true for existing catalogs. During play it applies on the next simulation step;
it does not require Apply & Reset. Simple patrols stop at the current pose and
resume from that pose. A tracked enemy releases its tracked controller and uses
a stationary collision proxy at the current pose; re-enabling recreates tracked
physics when inside the configured distance. Detection, aiming, firing, HP and
map placement are retained. Physics after destruction of a supporting object
is still allowed; disabling navigation does not cancel falling.

Movement OFF skips movement planning and observation-radius/height processing.
The per-enemy status table remains available; movement group/action details are
hidden while OFF. Attack/template changes still require Apply & Reset during
play. Saving writes the catalog; OFF does not remove route data from the map.

Slider drags and checkbox changes use the existing editor/game Undo history.
A drag is one command; OFF/ON and Undo/Redo preserve hidden tuning values.

## Verification

EnemyEditorJsonTests and UndoHistoryTests cover movement save/load, legacy ON,
strict booleans and retained tuning through Undo/Redo. EnemyGroupIntegrationTests
cover initial OFF, live simple/physical stop and resume, controller cleanup and
attacks continuing while movement is disabled.

EnemyCombatTests currently fails the existing thin-blocker projectile assertion.
The same failure reproduces in the previously built TankPhysicsSandbox-2 test
binary (built before this change); no combat collision code was modified here.

GUI checks confirmed movement OFF hides all seven tuning fields, Undo restores
the checkbox and fields, node-arrival > changes 0.50 to 0.55, | restores 0.50,
and a slider value change is undoable. Attack fields were visually verified as
slider/three-button rows. No catalog or map was saved during these GUI checks.

The isolated Debug app was rebuilt and linked against the updated physics
archive; EnemyEditorJsonTests, UndoHistoryTests and EnemyGroupIntegrationTests
passed. The standard executable remains in use by an existing app session.
