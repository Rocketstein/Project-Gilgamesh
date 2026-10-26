# Current Prototype Architecture

## Purpose

This document records the architecture implemented by Project Gilgamesh. It
describes the code as it exists now. Future
module, runtime simulation, reflection, and Play-in-Editor design
belongs in separate evolution documents.

The current executable is an editor prototype. It opens one Win32 window,
initializes the D3D11 renderer, runs one `EditorProgram`, draws the primitive
cube and editor grid into an offscreen viewport, and hosts an ImGui workspace.
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
          -> World and registered RenderExtractionSystem
          -> temporary demo mesh resource
          -> editor viewport and viewport client
          -> EditorViewportRenderer and configured render passes
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
| Fixed vertex-color command preparation and drawing | `Engine/Render/Passes/VertexColorMeshPass.*` |
| Common pass contract and viewport clear | `Engine/Render/Passes/IRenderPass.h`, `ViewportClearPass.*` |
| Renderer-facing snapshots and shared camera constants | `Engine/Render/Renderer/RenderFrameData.h`, `RenderView.*` |
| Active-scene extraction system | `Engine/ECS/System/RenderExtractionSystem.*` |
| Texture dependency, pass-state, and execution orchestration | `Engine/Render/RenderGraph/` |
| Editor world, extraction, and UI composition | `Editor/EditorProgram.*` |
| Viewport graph ownership and synchronous submission | `Editor/Render/EditorViewportRenderer.*` |
| Clear, grid, and mesh pipeline selection | `Editor/Render/EditorViewportPasses.*` |
| Procedural editor-grid background | `Editor/Render/EditorGridPass.*` |
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
private PImpl contains editor-only state and a `World` while
keeping those dependencies out of `EditorProgram.h`. Its constructor and
destructor are defined in `EditorProgram.cpp`, where `Impl` is complete.

The implementation owns:

- The editor viewport client, world, and temporary demo mesh handle
- `EditorViewportRenderer`, owning a graph and an ordered list of `IRenderPass` instances
- A borrowed pointer to the extraction system owned by the world's extraction group
- ImGui integration, the editor workspace, and the viewport panel
- The console buffer, sink registration, configuration, Output Log panel,
  commands, and pending command state when editor diagnostics are enabled

`EditorProgram::Shutdown()` resets the implementation while the engine window
and renderer are still alive. The viewport renderer releases graph callbacks
before its pass resources, and its synchronous render calls leave no pending
graph between frames. The implementation retires the temporary demo mesh through
`MeshManager`.
World destruction clears pending extraction and destroys the registered system.
This also lets ImGui disconnect its
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
boundaries; they do not yet form a general subsystem API.

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
      -> create EditorProgram
      -> EditorProgram::Initialize
          -> optionally create console state and register ConsoleLogSink
          -> initialize ImGui against the engine window and D3D11 device
          -> initialize the editor viewport
          -> create Clear -> Grid -> Mesh through CreateEditorViewportPasses
          -> initialize EditorViewportRenderer and its configured passes
          -> register and enable RenderExtractionSystem in the world
          -> upload the demo cube through MeshManager
          -> create an active scene and a transform/mesh entity
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
console command submitted by the previous frame, advances `World::Update()`,
and begins the new ImGui frame. It retains the frame time and number for the
render submission.

`EditorProgram::Render()` draws the workspace and viewport panel before
extracting the finalized world. A visible viewport runs this sequence:

```text
World::PreparePresentation(realDeltaTime)
  -> transform systems
  -> RenderExtractionSystem
  -> TakeFrame(), assign the engine frame number
  -> build RenderView from the editor camera and viewport extent
  -> EditorViewportRenderer::Render
      -> import viewport color and depth resources
      -> call IRenderPass::AddToGraph in configured order
          -> ViewportClearPass declares color/depth clears
          -> EditorGridPass captures view and grid constants by value
          -> VertexColorMeshPass prepares and owns MeshPassCommands in its callback
      -> RenderGraph::Compile / Execute
          -> Clear -> Grid background -> Mesh
      -> Reset the graph before returning, including on failure
```

A hidden viewport discards pending extraction instead of retaining a previous
snapshot. Presentation failure is caught at the program boundary; pass preparation
or execution failure is caught and logged by the viewport renderer. Failed
extraction is not submitted again. The viewport can resize on the next UI frame
because every viewport render call releases imported pointers and callbacks.

The swap chain and ImGui remain outside the graph: after graph execution,
`Renderer::BindOutput()` restores the swap-chain target before the Output Log
and accumulated ImGui draw data are submitted. The engine surrounds this work
with renderer begin/end calls.

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

`Renderer` owns the D3D11 device, presentation surface, shader manager, and
`MeshManager`. It owns no concrete render pass and has no `AddScenePasses` API.
`MeshManager` owns uploaded static meshes behind generation-checked handles.
`EditorViewportRenderer` owns the graph and `vector<unique_ptr<IRenderPass>>`.
`CreateEditorViewportPasses()` selects Clear -> Grid -> Mesh; the viewport
renderer itself contains no concrete pass member or type-specific branch.

`IRenderPass::Initialize(device, shaders)` creates pass-owned resources, and
`AddToGraph(graph, context)` declares prepared work. `RenderPassContext` supplies
color/depth handles and borrows the extracted frame, view, and mesh manager.
Callbacks copy submission data and borrow only longer-lived passes and services.
The viewport module can initialize from `Renderer&` or explicit device, immediate
context, shader-manager, and mesh-manager services, including for WARP tests.
The mesh pass owns its fixed vertex-color `DrawSetup` and separate view and
object constant buffers.

`PrepareCommands(frame, view, meshes)` is CPU-only. It validates the viewport
and invertible view-projection matrix through the shared CPU-only
`PrepareViewConstants(view)` helper, copies shader-ready camera constants,
and prepares one `MeshDrawCommand` for each currently valid extracted mesh.
Each command owns a transposed model matrix and a `MeshHandle`; it does not
retain an ECS reference or a raw `StaticMesh*`. Preparation preserves extraction
order and performs no culling, sorting, or GPU updates.

`MeshPassCommands` owns the per-view `FrameConstants` and vector of draw commands.
`VertexColorMeshPass::AddToGraph()` prepares that value and registers a `VertexColorMeshes`
pass whose execute callback captures it by value. Callers can release the
extraction snapshot after preparation. The pass and mesh manager must outlive
graph execution; graph callbacks borrow them.

`ExecuteCommands(context, commands, meshes)` uploads view constants only when
the pass executes, applies the draw setup, and uploads object constants for each
draw. It resolves each handle again before mesh binding and `DrawIndexed()`,
skipping resources removed since preparation. Preparing multiple views cannot
overwrite their GPU constants in advance. Empty command lists preserve the
background initialized by the independent clear and grid passes. Failed constant
uploads return failure; the graph callback converts it to an exception for the
viewport renderer to handle.

The Primitive vertex shader uses `b0 FrameConstants.viewProjection` and
`b2 ObjectConstants.model` to evaluate model then view-projection. CPU matrices
use row-vector conventions and are transposed for HLSL upload. Model constants
no longer contain a combined MVP matrix.

The editor owns the offscreen viewport and world. Temporary cube geometry is
uploaded through `MeshManager`; its instance transform and mesh handle live in
ECS components. `EditorViewport` exposes borrowed pointers to its native color
and depth resources, with writable and read-only views of the same depth texture,
without depending on `RenderGraph`; `EditorViewportRenderer` imports them per
submission.
All replacement resources must be created before a resize publishes them.
`EditorViewportPanel` measures and presents the viewport texture
but does not own scene rendering.

`EditorViewportRenderer` owns `EditorGridPass` through its configured interface
list. The clear pass initializes color and depth first. The grid then loads
color, disables depth testing and writes, and alpha-blends without a depth
attachment. The mesh pass loads color/depth and draws afterward, so opaque
meshes on either side of Z=0 cover the grid. The grid shader emits color only,
with no `SV_Depth`. It reconstructs intersections with the XY plane at Z=0 from a procedural
full-screen triangle submitted with `Draw(3, 0)`. An empty mesh snapshot still
runs the clear and grid passes, so the grid remains visible
without an active scene.

The grid uses the same `PrepareViewConstants` helper as the mesh pass and owns
separate `b0 FrameConstants` and `b1 EditorConstants` GPU buffers. Its graph
callback captures CPU constants by value and uploads them only at execution.
`SetSettings` validates and copies appearance settings; already registered
callbacks retain their own values. The module resets its graph before every
render return, releasing callbacks and imported pointers before resize or
shutdown.
The viewport retains its read-only depth view for other consumers; the grid
does not use it.

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

The vertex-color pass is the first concrete consumer of ECS render extraction.
There is no separate `MeshDrawExecutor`; command preparation and execution are
two responsibilities within the same pass. Materials, mesh bounds, visibility
culling, sorting, instancing, a general world renderer, and an API-independent
RHI remain unimplemented. `EditorGridPass` replaces the former
`EditorGridLinePass` stub and is composed by the editor rather than the world
or general scene renderer.

## ECS scheduling and render extraction

Application composition code owns `World` directly and passes borrowed `World&`
access to systems. The world's simulation and presentation entry points are
public. Callers must execute them at the application boundary, outside entity
and scene traversals; the world phase guard rejects nested execution while a
scheduled phase is already running.

Command-buffer construction and playback are private to `World`; payload callbacks
are private to the buffer, so they do not expose another playback entry point.

`World` owns three generic `SystemGroup` roots: simulation, transform, and
extraction. Each root is initialized and enabled by the world. Concrete systems
are registered by composition code through `AddSystem(unique_ptr<ISystem>)`,
then explicitly enabled. Groups own their children exclusively, execute them in
registration order, and disable and destroy them in reverse order while the
world storage remains alive. Callers may retain borrowed system references but
must discard them before removal or world destruction.

`World::Update(deltaTime)` replays structural commands, runs simulation unless
paused, then replays simulation requests. `World::PreparePresentation(realDeltaTime)`
replays the latest editor requests, runs transform systems, and finally runs
extraction systems. It is a separate entry point so presentation can run once
per render submission, including while simulation is paused. There is no
structural playback between transform evaluation and extraction. Structural
requests, immediate scene mutation, recursive world updates, and changes to
system registration are rejected during the phases where they would invalidate
storage or scheduling. A system exception restores the world to its idle phase.

The world retains one full generation-checked active `SceneHandle`, initially
null. Scene creation does not activate a scene. `RequestCreateScene` reserves a
full scene identity immediately and records an owned name in the entity command
FIFO. The reservation is usable by subsequent entity, activation, and destruction
requests, but `TryGetScene` and `IsSceneHandleValid` expose only committed scenes.
Creation is published at playback, before later dependent commands.
`RequestSetActiveScene`,
`RequestClearActiveScene`, and `RequestDestroyScene` enter the same FIFO as
entity commands and are applied at playback boundaries. Requests are validated
again at playback. Destroying the selected scene clears selection without
choosing a fallback. The synchronous `CreateScene` and `DestroyScene` APIs are
public for tests, initialization, and other controlled setup outside execution
and traversal. They are not the normal mutation path. Immediate destruction can
also cancel a reserved scene. Cancelled or discarded creation payloads retire
their reserved generation without allocating and cannot affect a reused slot.
The world no longer tracks traversal depth; callers must keep immediate setup
operations and world execution outside active traversals. Active scene selection
identifies the default render source; `World::ForEachComponent<T...>` retains
its whole-world component-query semantics. `World::ForEachEntity` instead visits
every live committed entity handle, including entities with no components or
scene membership. Reserved, cancelled, and destroyed entities are excluded.
Both traversals allow deferred requests to be recorded, but command playback
and immediate scene changes must wait until traversal has finished. Entity
traversal keeps its initial slot-count boundary so reservations recorded by a
visitor cannot extend the current traversal or invalidate its iteration.

`TransformHierarchySystem` maintains a validated hierarchy for every live
`TransformComponent`, independent of scene membership or mesh assignment. The
component's `parent` is the authored source; the system caches validated parents,
children, roots, depths, and a parent-first traversal using complete entity
handles. Direct parent edits and component/entity lifetime changes invalidate
the cache, while TRS-only edits do not. Invalid links and cycle-closing edges
are logged and treated as roots in the cache without rewriting source links.
`SetParent` validates live transforms and ancestry before changing the source
link, preserving local TRS; cached queries refresh on the next update. A rebuild
publishes all derived data together and advances a topology revision. Disabling
or destroying the system clears its views; a later rebuild publishes a newer
revision. The editor does not register this system yet.

`LocalTransformSystem` evaluates authored TRS as a row-vector local matrix in
scale, rotation, translation order. `LocalToWorldSystem` then visits the
hierarchy's parent-first traversal and publishes complete affine world matrices:
roots use their local matrix, and children use `local * parentWorld`. Full
matrices retain shear introduced by parent scale and child rotation. Local and
world revisions start at zero. Local revisions advance on first evaluation or
a changed local matrix. World revisions are process-wide publication tokens;
first evaluation or a changed world matrix receives a newer nonzero token,
which can skip values for an individual entity. Recreating a transform cannot
reuse an older parent's publication token. World evaluation records the consumed
local revision, validated parent, and parent world revision even when those
inputs produce an unchanged matrix; it does not clear revisions used by other
consumers. Unready local or parent world inputs are logged and skipped until a
later evaluation. The editor does not register these transform systems yet.

`RenderExtractionSystem : public ISystem` traverses the active scene's entity
membership and reads `TransformComponent` and `MeshComponent` from the world.
It copies assigned mesh handles and row-vector world transforms into
`RenderFrameData`; it does not resolve GPU resources or retain ECS references.
The current editor path still composes TRS directly as scale, rotation, then
translation. Extraction from the evaluated world-matrix cache remains
unimplemented. Stale resource generations are left for the renderer-facing
consumer to validate.

The extractor owns at most one completed `optional<RenderFrameData>`.
`TakeFrame` transfers it once and clears the pending slot; `DiscardFrame`,
disablement, and destruction clear unconsumed data. No active scene produces a
present but empty snapshot, distinct from no fresh result. `EditorProgram`
takes the result, assigns engine frame metadata, and supplies it to
`EditorViewportRenderer::Render()`. The old `EditorPreviewScene`, `StaticMeshDrawItem`,
and `SimpleMeshRenderer` path has been removed.

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
- Runtime gameplay simulation or Play-in-Editor
- Configurable action mapping and UI-aware editor/gameplay input routing
- Reflection, serialization, object handles, or garbage collection
- Asset/project management
- Runtime engine restart after `Engine::Shutdown()`

These omissions should not be inferred as architectural decisions. They mark
the boundary of the present baseplate and are subjects for the evolution
architecture.
