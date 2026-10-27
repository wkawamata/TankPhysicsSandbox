# Game viewport

Camera > Game Viewport controls the game display. Enable Custom game viewport.
Map Editor and the top menu keep their normal full-window view.

- Position X/Y specify the top-left corner as fractions of the client window.
  (0, 0) is top-left; (0.1, 0.2) starts at 10% of width and 20% of height.
- Width/Height are fractions of the entire client window, not the remaining space.
- Free rectangle allows any aspect ratio. 16:9, 4:3 and 21:9 fit within that rectangle,
  preserving its top-left corner. Overflow is clipped to the window; at least one
  pixel remains at an edge endpoint.
- Center viewport centers the effective rectangle and updates its XY and size.
- Reset viewport restores full-window display.
- Save Viewport writes Config/game_viewport.json. It loads at startup; Load Viewport
  restores it manually. This is a global display setting, independent of camera slots.

GUI remains full-window and can be placed over the black bands. Start camera mouse
controls inside the game rectangle; releases still terminate gestures anywhere.
Viewport edits, reset, centering and manual load share the Tank Settings Undo/Redo
history. Continuous slider changes form one command. File writes are not undone.

The renderer allocates scene buffers (G-buffer, depth, lighting, reflections and
path tracing) at the game rectangle's dimensions. DLSS uses this rectangle as its
output size and selects a smaller internal input when enabled. The camera projects
directly at the scene aspect ratio; camera-slot FOV and lens values stay unchanged.
Tone mapping places the scene at the specified XY, with black outside the rectangle.
The swap chain and GUI remain full-window. No full-window image copy is required.
Moving XY without changing the effective size does not recreate scene buffers.
Changing size waits for in-flight GPU work before rebuilding size-dependent resources.
Shadow map resolution and scene/physics CPU work are unaffected by the game rectangle.
The Game Viewport panel displays both scene output and internal buffer dimensions.
This implementation includes changes to the RtPbrSurvey renderer submodule.

GameViewportTests covers normalized coordinates, resizing, overflow and endpoints,
aspect fitting, renderer output dimensions, JSON validation and grouped Undo/Redo.

Buffer-size validation used a 1920x1080 window with a 960x540 scene rectangle.
With the saved DLSS Quality setting, internal buffers changed from 1280x720 to
640x360 (one quarter of the pixels). GUI checks covered XY movement, reset,
Undo and maximize/resize. The D3D12 debug log contained no errors; startup buffer
COMMON-state warnings remained. This checks dimensions, not a GPU-time benchmark.
Validation restored the original UI layout and viewport configuration.
