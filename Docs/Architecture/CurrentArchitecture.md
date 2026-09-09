# Current Prototype Architecture

## Purpose

This document records the architecture implemented by Project Gilgamesh after
the application-loop refactor. It describes the code as it exists now. Future
module, game-session, world, reflection, and Play-in-Editor design belongs in a
separate evolution document.

The current executable is an editor prototype. It opens one Win32 window,
initializes the D3D11 renderer, runs one `EditorProgram`, draws the primitive
cube into an offscreen editor viewport, and hosts an ImGui editor workspace.
The developer console and Output Log are optional editor diagnostics.

## System overview

```text
WinMain
  -> Application
      -> Engine
          -> Window
          -> InputSystem
          -> Win32InputBackend
          -> Renderer
          -> FrameClock
          -> Engine::Run(IProgram&)
      -> EditorProgram : IProgram
          -> primitive graphics test
          -> editor viewport and viewport client
          -> ImGui integration and editor workspace
          -> developer console and Output Log (optional diagnostics)
```

There is one concrete `Engine`. Editor behavior is supplied through the
`IProgram` interface rather than through an `EditorEngine` subclass.

## Build targets

The root CMake project currently produces these targets:

| Target | Kind | Responsibility |
| --- | --- | --- |
| `Gilgamesh` | Win32 executable | Entry point and process-level `Application` composition |
| `GilgameshEngine` | Static library | Core, platform, physical input, renderer, engine loop, and frame clock |
| `GilgameshEditor` | Static library | Editor program, camera, input, viewport, UI, and optional diagnostics |
| `DearImGui` | Static library | Vendored ImGui core and Win32/D3D11 backends |

The target dependency direction is:

```text
Gilgamesh executable
  -> GilgameshEngine
  -> GilgameshEditor
       -> GilgameshEngine
       -> DearImGui
```

`GILGAMESH_BUILD_EDITOR=1` currently identifies the executable as the editor
build. The editor workspace, viewport panel, and ImGui integration are always
part of that product. `GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS` controls only the
console, commands, log sink, and Output Log panel and is translated to the
numeric C++ definition `0` or `1`.

No standalone game target is implemented. `Game/GameProgram.h` is only a
placeholder.

## Source map

| Responsibility | Source |
| --- | --- |
| Windows entry point | `main.cpp` |
| Process composition and lifecycle | `Application/Application.*` |
| Program callback contract | `Engine/Runtime/Program.h` |
| Engine services exposed to a program | `Engine/Runtime/EngineServices.h` |
| Frame and render callback data | `Engine/Runtime/FrameContext.h`, `RenderContext.h` |
| Window, renderer, clock, and main loop | `Engine/Runtime/Engine.*` |
| Platform-neutral input snapshots | `Engine/Input/` |
| Win32 input translation | `Engine/Platform/Windows/Win32InputBackend.*` |
| Real-time frame measurement | `Engine/Time/FrameClock.*` |
| Editor implementation and primitive test | `Editor/EditorProgram.*` |
| Editor viewport presentation | `Editor/EditorUI/Panels/EditorViewportPanel.*` |
| ImGui backend integration | `Editor/EditorUI/Runtime/ImGuiIntegration.*` |
| Console implementation | `Editor/Console/` |
| Output Log panel | `Editor/EditorUI/Panels/OutputLogPanel.*` |
| Dockspace and default layout | `Editor/EditorUI/Workspace/EditorWorkspace.*` |

## Ownership and lifetime

`Application` is the process-level composition root. Its members are declared
in this order:

```cpp
Engine engine_;
std::unique_ptr<IProgram> program_;
```

Members are destroyed in reverse declaration order, so the selected program is
destroyed before the engine. `Application::Shutdown()` also enforces this
explicitly by shutting down and resetting the program before shutting down the
engine.

`Engine` owns, in order:

```cpp
Window window_;
InputSystem input_;
Win32InputBackend inputBackend_;
Renderer renderer_;
FrameClock clock_;
```

The window consequently outlives the Win32 input backend and renderer during
destruction. The engine provides borrowed references to its window, renderer,
and read-only input system through `EngineServices`; a program does not own any
of these objects.

`EditorProgram` owns its implementation through `std::unique_ptr<Impl>`. The
private PImpl contains editor-only and D3D11 test state while keeping those
dependencies out of `EditorProgram.h`. Its constructor and destructor are
defined in `EditorProgram.cpp`, where `Impl` is complete.

The implementation owns:

- The editor viewport client and primitive graphics resources
- ImGui integration, the editor workspace, and the viewport panel
- The console buffer, sink registration, configuration, Output Log panel,
  commands, and pending command state when editor diagnostics are enabled

`EditorProgram::Shutdown()` resets the implementation while the engine window
and renderer are still alive. This lets ImGui disconnect its window-message
handler and releases program-owned GPU resources before engine destruction.

## Program boundary

The engine calls the active program through this lifecycle contract:

```cpp
class IProgram
{
public:
    virtual ~IProgram() = default;

    virtual bool Initialize(EngineServices&) = 0;
    virtual void Update(const FrameContext&) = 0;
    virtual void Render(RenderContext&) = 0;
    virtual void Shutdown() = 0;
};
```

`EngineServices` currently exposes borrowed `Window&` and `Renderer&`
references during initialization. `RenderContext` exposes the renderer and
current output extent during rendering. These are deliberately small prototype
boundaries; they do not yet form a general subsystem or render-command API.

## Startup and shutdown

The current startup sequence is:

```text
WinMain
  -> construct Application
  -> Application::Run
      -> register DebugOutputSink in debug builds
      -> Engine::Initialize
          -> create Window
          -> attach Win32InputBackend to Window
          -> initialize Renderer and shader manager
      -> create EditorProgram
      -> EditorProgram::Initialize
          -> optionally create console state and register ConsoleLogSink
          -> initialize ImGui against the engine window and D3D11 device
          -> initialize the editor viewport
          -> load Primitive shaders
          -> create the test vertex buffer and graphics pipeline
      -> Engine::Run(EditorProgram)
```

Normal exit and initialization failure both return through
`Application::Shutdown()`. Program shutdown occurs first, followed by engine
shutdown. At present, `Engine::Shutdown()` marks the engine inactive; the
window and renderer perform their actual resource release through RAII when the
`Application` object is destroyed.

## Frame loop

`Engine::Run()` owns the invariant frame mechanics:

```text
begin input collection
  -> pump Win32 messages
  -> freeze the current InputFrame
  -> if minimized: wait, reset FrameClock, skip frame
  -> consume pending resize and resize Renderer
  -> FrameClock::BeginFrame
  -> EditorProgram::Update
  -> Renderer::BeginFrame
  -> EditorProgram::Render
  -> Renderer::EndFrame
  -> handle success, occlusion, device loss, or failure
```

When the presentation surface is occluded, the engine pumps messages and tests
for visibility at a reduced rate. Each occluded message pump still advances the
physical input snapshot so focus changes and releases cannot leave stale held
state. The engine resets the frame clock after the wait so the suspended
interval does not become a large frame delta.

### Physical input

`Win32InputBackend` observes window messages without consuming them and
translates supported keyboard and mouse messages into platform-neutral input
submissions. The consuming window-message handler remains independently
available to integrations such as ImGui, so UI handling cannot prevent the
physical device state from observing releases.

`InputSystem` accumulates submissions between `BeginFrame()` and `EndFrame()`.
`EndFrame()` publishes an `InputFrame` containing held state, per-frame press
and release transitions, absolute and relative mouse movement, wheel movement,
and focus state. Programs receive read-only access through `EngineServices`.
A hard-coded editor controller currently maps that physical state to camera
intent above the engine layer. Configurable action mapping, UI-aware editor
input routing, and gameplay meaning are not implemented yet.

### Frame time

`FrameClock` uses `std::chrono::steady_clock`. `BeginFrame()` produces:

- `realDeltaTime`, clamped to a maximum of 0.25 seconds
- A monotonically increasing `frameNumber`

This is wall-clock frame time. It has no pause or game-speed semantics and is
not a frame limiter. No fixed-update or strategic simulation clock currently
exists.

### Editor update and rendering

`EditorProgram::Update()` updates the viewport client, optionally executes a
console command submitted by the previous frame, and begins the new ImGui
frame.

`EditorProgram::Render()` submits the editor workspace and viewport panel,
renders the primitive cube into the viewport's offscreen target, restores the
swap-chain output, optionally submits the Output Log, and finally renders
ImGui. The engine surrounds this work with renderer begin/end calls.

Deferring console execution until the next update prevents command handlers
from mutating the console buffer while the previous frame's widgets are being
built.

## Logging and console flow

```text
GILGAMESH_LOG
  -> Logger
      -> DebugOutputSink (_DEBUG)
      -> ConsoleLogSink (editor diagnostics enabled)
          -> ConsoleBuffer
              -> OutputLogPanel
```

The debug sink registration is scoped to `Application::Run()`. The console
sink registration belongs to `EditorProgram::Impl` and is removed when the
editor implementation is destroyed. The engine logging system remains
independent of ImGui.

Console submission flows in the opposite direction:

```text
OutputLogPanel::Draw
  -> EditorProgram stores pending command
  -> next EditorProgram::Update
  -> CommandRegistry::Execute
  -> ConsoleCommandOutput
  -> ConsoleBuffer
```

## Rendering boundary

The renderer currently owns the D3D11 device, presentation surface, and shader
manager. The editor owns the offscreen viewport target, temporary primitive
pipeline, and vertex buffer and issues the draw call directly through the
D3D11 device context. `EditorViewportPanel` measures and presents the viewport
texture but does not own scene rendering.

This is intentional prototype code, not a general scene renderer. There is no
render graph, resource manager, world renderer, or API-independent RHI.

## Current boundaries and non-features

The following are not implemented by the current architecture:

- Standalone game executable or functioning `GameProgram`
- `GameSession`, worlds, strategic simulation, or Play-in-Editor
- Configurable action mapping and UI-aware editor/gameplay input routing
- Reflection, serialization, object handles, or garbage collection
- Asset/project management
- Runtime engine restart after `Engine::Shutdown()`

These omissions should not be inferred as architectural decisions. They mark
the boundary of the present baseplate and are subjects for the evolution
architecture.
