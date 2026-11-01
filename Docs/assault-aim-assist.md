# Assault aim assist

The Tracked Vehicle window exposes Aim assist under Assault Projectiles.
Settings apply live to newly fired rounds and are saved with Tank Settings.
Slider gestures and the enable checkbox share the existing MyTank Undo/Redo.

- Aim assist defaults to enabled, with horizontal and vertical half-angles of
  10 degrees and a search distance of 40 metres.
- The search cone follows the tank's firing direction, including elevation.
  Horizontal and vertical limits form an elliptical cone; either half-angle can
  be zero to constrain that axis to the centre line.
- At launch, select the closest reachable, visible enemy ordinary projectile.
  If none is eligible, select the closest eligible enemy tank. Special rounds
  remain immune to ordinary fire and are excluded from targeting.
- Lead moving targets using their current velocity and the ordinary round's
  configured speed. Both the current target and predicted firing direction must
  fit inside the cone. Shot lifetime, distance expiry, search distance and enemy
  projectile remaining travel limit the intercept time.
- Static map geometry and other bodies block selection. Target selection uses
  a segment or swept sphere matching the configured ordinary-round hit radius.
- No eligible target means the original firing direction. Once launched, the
  round flies straight; there is no continuous homing.

The cone changes target acquisition and launch direction only. Hit radius
continues to control collision independently.
