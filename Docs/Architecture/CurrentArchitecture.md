# Current Prototype Architecture

## Purpose

This document records the architecture implemented by Project Gilgamesh after
the application-loop refactor. It describes the code as it exists now. Future
module, runtime simulation, reflection, and Play-in-Editor design
belongs in separate evolution documents.

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
          -> EditorPreviewScene
          -> editor viewport and viewport client
          -> frame-local RenderGraph orchestration
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
| Renderer-facing mesh resource management | `Engine/Render/Mesh/MeshManager.*` |
| Fixed vertex-color mesh technique | `Engine/Render/Techniques/SimpleMeshRenderer.*` |
| Texture dependency, pass-state, and execution orchestration | `Engine/Render/RenderGraph/` |
| Editor implementation and mesh submission | `Editor/EditorProgram.*` |
| Temporary preview content and mesh instances | `Editor/EditorScene/EditorPreviewScene.*` |
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
private PImpl contains editor-only state and an `EditorPreviewScene` while
keeping those dependencies out of `EditorProgram.h`. Its constructor and
destructor are defined in `EditorProgram.cpp`, where `Impl` is complete.

The implementation owns:

- The editor viewport client, temporary preview scene, and frame render graph
- ImGui integration, the editor workspace, and the viewport panel
- The console buffer, sink registration, configuration, Output Log panel,
  commands, and pending command state when editor diagnostics are enabled

`EditorProgram::Shutdown()` asks `EditorPreviewScene` to remove its resources
through `MeshManager`, then resets the implementation while the engine window
and renderer are still alive. This also lets ImGui disconnect its
window-message handler and releases program-owned viewport resources before
engine destruction.

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
          -> initialize Renderer
              -> create the D3D11 device and presentation surface
              -> initialize ShaderManager and MeshManager
              -> initialize SimpleMeshRenderer
      -> create EditorProgram
      -> EditorProgram::Initialize
          -> optionally create console state and register ConsoleLogSink
          -> initialize ImGui against the engine window and D3D11 device
          -> initialize the editor viewport
          -> initialize EditorPreviewScene through MeshManager
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

This is wall-clock frame time. It has no pause or simulation-speed semantics
and is not a frame limiter. No fixed-step simulation clock currently exists.

### Editor update and rendering

`EditorProgram::Update()` updates the viewport client, optionally executes a
console command submitted by the previous frame, and begins the new ImGui
frame.

`EditorProgram::Render()` submits the editor workspace and viewport panel, then
rebuilds its frame-local render graph. It imports the viewport-owned color and
depth textures and their native views, declares an `EditorScene` pass, builds a
`RenderView`, and gathers `StaticMeshDrawItem` values from
`EditorPreviewScene`. The pass callback invokes `SimpleMeshRenderer` after the
graph has bound and cleared the viewport attachments. The swap chain and ImGui
remain outside the graph: after graph execution, `Renderer::BindOutput()`
restores the swap-chain target before the Output Log and the accumulated ImGui
draw data are submitted. The engine surrounds this work with renderer
begin/end calls.

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

The renderer currently owns the D3D11 device, presentation surface, shader
manager, `MeshManager`, and `SimpleMeshRenderer`. `MeshManager` owns uploaded
static meshes behind generation-checked handles. `SimpleMeshRenderer` owns the
fixed vertex-color pipeline and object constant buffer and issues indexed
draws from a `RenderView` and a span of `StaticMeshDrawItem` values.

The editor owns the offscreen viewport target and `EditorPreviewScene`. The
preview scene owns the temporary cube handle and transform and resolves them
into frame-local draw items. `EditorViewport` exposes borrowed pointers to its
viewport-owned D3D11 color and depth resources without depending on
`RenderGraph`; `EditorProgram` is the composition point that imports those
resources into the graph each frame. Its `EditorScene` pass selects the target,
constructs the view, gathers the draw items, and invokes the renderer-owned
technique. `EditorViewportPanel` measures and presents the viewport texture but
does not own scene rendering.

`RenderGraph` declares texture attachments and shader reads. `Compile(device)`
validates those declarations, derives a deterministic dependency order, and
creates graph-owned D3D11 textures with only their required RTV, DSV, and SRV
objects. Imported textures remain caller-owned and their native texture and
views must remain alive through graph execution. Repeated writes to one texture
retain declaration order, while independent passes may be reordered.

`Execute(context)` visits that compiled order, resolves graph-owned or imported
views, binds the RTV, DSV, and stage-specific SRV slots declared by each pass,
applies requested clears, and establishes a full-attachment default viewport.
It then invokes the pass callback and unbinds the pass outputs and declared
shader-resource slots at the pass boundary. This keeps graph-declared D3D11
read/write hazards and pass-level state transitions inside the graph. Callbacks
must not bind graph textures through undeclared shader stages or slots. The
callback remains responsible for applying `DrawSetup`, updating and binding
constant buffers, binding meshes and other draw-local resources, and issuing
draw calls. Depth clears currently affect only the depth plane; stencil access
and stencil load/clear behavior are not modeled yet.

The current graph boundary ends at the offscreen editor scene. Swap-chain
presentation and ImGui composition are not graph passes: `EditorProgram`
restores the presentation target with `Renderer::BindOutput()` and then submits
the UI, whose viewport image samples the already rendered color texture.

This is intentional prototype code, not a general scene renderer. There is no
asset manager, material system, world renderer, pass-specific mesh-command
preparation, or API-independent RHI. The ECS render extraction described below
is implemented independently and is not yet connected to the editor viewport.

## ECS scheduling and render extraction

`World` owns three generic `SystemGroup` roots: simulation, transform, and
extraction. Each root is initialized and enabled by the world. Concrete systems
are registered by composition code through `AddSystem(unique_ptr<ISystem>)`,
then explicitly enabled. Groups own their children exclusively, execute them in
registration order, and disable and destroy them in reverse order while the
world storage remains alive. Callers may retain borrowed system references but
must discard them before removal or world destruction.

`World::Update(deltaTime)` replays structural commands, runs simulation unless
paused, then replays simulation requests. `PreparePresentation(realDeltaTime)`
replays the latest editor requests, runs transform systems, and finally runs
extraction systems. It is a separate entry point so presentation can run once
per render submission, including while simulation is paused. There is no
structural playback between transform evaluation and extraction. Structural
requests, immediate scene mutation, recursive world updates, and changes to
system registration are rejected during the phases where they would invalidate
storage or scheduling. A system exception restores the world to its idle phase.

The world retains one full generation-checked active `SceneHandle`, initially
null. Scene creation does not activate a scene. `RequestSetActiveScene`,
`RequestClearActiveScene`, and `RequestDestroyScene` enter the same FIFO as
entity commands and are applied at playback boundaries. Requests are validated
again at playback. Destroying the selected scene clears selection without
choosing a fallback. The synchronous `CreateScene` and `DestroyScene` APIs are
available only outside world execution for setup and existing callers. Active
scene selection identifies the default render source; `World::ForEach` retains
its whole-world query semantics.

`RenderExtractionSystem : public ISystem` traverses the active scene's entity
membership and reads `TransformComponent` and `MeshComponent` from the world.
It copies assigned mesh handles and row-vector world transforms into
`RenderFrameData`; it does not resolve GPU resources or retain ECS references.
World-space TRS currently composes as scale, rotation, then translation. There
is no transform hierarchy evaluator yet. Stale resource generations are left
for the renderer-facing consumer to validate.

The extractor owns at most one completed `optional<RenderFrameData>`.
`TakeFrame` transfers it once and clears the pending slot; `DiscardFrame`,
disablement, and destruction clear unconsumed data. No active scene produces a
present but empty snapshot, distinct from no fresh result. Composition code can
move the snapshot to a future renderer submission API and assign engine frame
metadata there. This implementation does not alter `SimpleMeshRenderer`,
introduce pass-specific draw commands, or replace `EditorPreviewScene` yet.

Composition code registers an enabled `RenderExtractionSystem` in
`world.GetExtractionSystems()`, retains a borrowed pointer, and requests the
active scene explicitly. After `world.PreparePresentation(realDeltaTime)`
returns successfully, it calls `TakeFrame()` once and moves the result to its
consumer. A failed presentation preparation must abort consumption and discard
any pending result; extraction clears its own pending output before rebuilding.
System initialization hooks must release their own partially acquired resources
if they fail, just as constructors do. Successful world-owned systems are shut
down while ECS storage is still valid.

## Current boundaries and non-features

The following are not implemented by the current architecture:

- Standalone game executable or functioning `GameProgram`
- ECS world integration, runtime simulation, or Play-in-Editor
- Configurable action mapping and UI-aware editor/gameplay input routing
- Reflection, serialization, object handles, or garbage collection
- Asset/project management
- Runtime engine restart after `Engine::Shutdown()`

These omissions should not be inferred as architectural decisions. They mark
the boundary of the present baseplate and are subjects for the evolution
architecture.
