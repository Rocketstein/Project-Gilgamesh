# Current Prototype Architecture

## Purpose

This document records the architecture implemented by Project Gilgamesh after
the application-loop refactor. It describes the code as it exists now. Future
module, game-session, world, reflection, and Play-in-Editor design belongs in a
separate evolution document.

The current executable is an editor prototype. It opens one Win32 window,
initializes the D3D11 renderer, runs one `EditorProgram`, draws the primitive
triangle test, and optionally hosts the ImGui developer console and dockspace.

## System overview

```text
WinMain
  -> Application
      -> Engine
          -> Window
          -> Renderer
          -> FrameClock
          -> Engine::Run(IProgram&)
      -> EditorProgram : IProgram
          -> primitive graphics test
          -> ImGui integration (optional)
          -> developer console (optional)
          -> developer-tools workspace (optional)
```

There is one concrete `Engine`. Editor behavior is supplied through the
`IProgram` interface rather than through an `EditorEngine` subclass.

## Build targets

The root CMake project currently produces these targets:

| Target | Kind | Responsibility |
| --- | --- | --- |
| `Gilgamesh` | Win32 executable | Entry point, `Application`, and `EditorProgram` |
| `GilgameshEngine` | Static library | Core, platform, renderer, engine loop, and frame clock |
| `GilgameshDeveloperTools` | Static library | Console, ImGui integration, and dockspace |
| `DearImGui` | Static library | Vendored ImGui core and Win32/D3D11 backends |

The target dependency direction is:

```text
Gilgamesh executable
  -> GilgameshEngine
  -> GilgameshDeveloperTools (when enabled)
       -> GilgameshEngine
       -> DearImGui
```

`GILGAMESH_BUILD_EDITOR=1` currently identifies the executable as the editor
build. `GILGAMESH_ENABLE_DEVELOPER_TOOLS` is a CMake option and is translated
to the numeric C++ definition `0` or `1`. Developer-tool code therefore uses
`#if GILGAMESH_ENABLE_DEVELOPER_TOOLS`, not `#ifdef`.

No standalone game target is implemented. `Game/GameProgram.h` is only a
placeholder, and the legacy `Client/` directory is not part of the active
application architecture.

## Source map

| Responsibility | Source |
| --- | --- |
| Windows entry point | `main.cpp` |
| Process composition and lifecycle | `Application/Application.*` |
| Program callback contract | `Engine/Runtime/Program.h` |
| Engine services exposed to a program | `Engine/Runtime/EngineServices.h` |
| Frame and render callback data | `Engine/Runtime/FrameContext.h`, `RenderContext.h` |
| Window, renderer, clock, and main loop | `Engine/Runtime/Engine.*` |
| Real-time frame measurement | `Engine/Time/FrameClock.*` |
| Editor implementation and primitive test | `Editor/EditorProgram.*` |
| ImGui backend integration | `DeveloperTools/Runtime/ImGuiIntegration.*` |
| Console implementation | `DeveloperTools/Console/` |
| Dockspace and default layout | `DeveloperTools/Workspace/DeveloperToolsWorkspace.*` |

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
Renderer renderer_;
FrameClock clock_;
```

The window consequently outlives the renderer during destruction. The engine
provides borrowed references to its window and renderer through
`EngineServices`; a program does not own either object.

`EditorProgram` owns its implementation through `std::unique_ptr<Impl>`. The
private PImpl contains editor-only and D3D11 test state while keeping those
dependencies out of `EditorProgram.h`. Its constructor and destructor are
defined in `EditorProgram.cpp`, where `Impl` is complete.

The implementation owns:

- The primitive graphics pipeline and vertex buffer
- ImGui integration and the dockspace when developer tools are enabled
- The console buffer, sink registration, configuration, panel, commands, and
  pending command state when developer tools are enabled

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
          -> initialize Renderer and shader manager
      -> create EditorProgram
      -> EditorProgram::Initialize
          -> create console state and register ConsoleLogSink
          -> initialize ImGui against the engine window and D3D11 device
          -> create Output Log and command state
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
pump Win32 messages
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
for visibility at a reduced rate. It resets the frame clock after the wait so
the suspended interval does not become a large frame delta.

### Frame time

`FrameClock` uses `std::chrono::steady_clock`. `BeginFrame()` produces:

- `realDeltaTime`, clamped to a maximum of 0.25 seconds
- A monotonically increasing `frameNumber`

This is wall-clock frame time. It has no pause or game-speed semantics and is
not a frame limiter. No fixed-update or strategic simulation clock currently
exists.

### Editor update and rendering

`EditorProgram::Update()` executes a console command submitted by the previous
frame, then begins the new ImGui frame.

`EditorProgram::Render()` binds the primitive pipeline, draws the triangle,
submits the dockspace and Output Log, records any newly submitted command, and
renders ImGui. The engine surrounds this work with renderer begin/end calls.

Deferring console execution until the next update prevents command handlers
from mutating the console buffer while the previous frame's widgets are being
built.

## Logging and console flow

```text
GILGAMESH_LOG
  -> Logger
      -> DebugOutputSink (_DEBUG)
      -> ConsoleLogSink (developer tools enabled)
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
manager. The editor owns the temporary primitive pipeline and vertex buffer and
issues the draw call directly through the D3D11 device context.

This is intentional prototype code, not a general scene renderer. There is no
render graph, offscreen editor viewport, resource manager, world renderer, or
API-independent RHI.

## Current boundaries and non-features

The following are not implemented by the current architecture:

- Standalone game executable or functioning `GameProgram`
- `GameSession`, worlds, strategic simulation, or Play-in-Editor
- Input abstraction or event routing beyond ImGui's Win32 message adapter
- Reflection, serialization, object handles, or garbage collection
- Asset/project management
- Offscreen rendering or a dedicated editor viewport
- Runtime engine restart after `Engine::Shutdown()`
- Automated tests

These omissions should not be inferred as architectural decisions. They mark
the boundary of the present baseplate and are subjects for the evolution
architecture.
