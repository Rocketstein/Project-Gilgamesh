# Project Gilgamesh Rendering Trajectory

## Purpose

This document records the implemented rendering baseline and intended evolution
of Project Gilgamesh as a general-purpose ECS engine. It supersedes the earlier
domain-specific trajectory. `Docs/Architecture/CurrentArchitecture.md` describes
the complete running application boundary.

Each abstraction should appear when a visible feature gives it a concrete job.
The renderer is D3D11-specific; a cross-API interface remains out of scope until
a second backend becomes a real requirement.

## Current data boundary

The implemented path is:

```text
World::PreparePresentation
  -> transform systems
  -> RenderExtractionSystem
  -> RenderFrameData: mesh handles and copied world transforms
  -> EditorProgram adds RenderView: camera and viewport
  -> Renderer::AddScenePasses
  -> VertexColorMeshPass::PrepareCommands
  -> MeshPassCommands captured by value in RenderGraph
  -> RenderGraph binds and clears viewport attachments
  -> VertexColorMeshPass::ExecuteCommands
  -> DrawSetup, constant buffers, mesh binding, DrawIndexed
```

Extraction separates ECS storage from renderer input. The renderer does not
query entities or retain component references. `RenderFrameData` is
camera-independent and can be reused to prepare several views. Tests and
non-ECS previews can construct the same values without a world.

The extractor owns one optional completed snapshot. `TakeFrame()` transfers it
once; disablement, destruction, or `DiscardFrame()` clears unconsumed output.
An empty active-scene result is a fresh, empty snapshot and still clears the
viewport. Without a fresh result, callers must not reuse earlier commands.

## Architectural principles

### Mesh resources and draws have separate ownership

```text
StaticMeshData
  -> CPU vertices and indices

StaticMesh
  -> immutable D3D11 vertex/index buffers and draw metadata

MeshManager
  -> uploaded mesh ownership and generation-checked MeshHandle identities

VertexColorMeshPass
  -> fixed DrawSetup, command preparation, constants, and indexed submission
```

`StaticMeshData` is API-independent and suitable for procedural generation,
asset import, and validation. `StaticMesh` is a move-only GPU resource. Neither
a mesh nor a renderable component owns a camera, render target, or draw call.
Here, static means non-deforming topology; an entity may move every frame.
Bounds, normals, and texture coordinates remain future work driven by
visibility and material requirements.

### Draw setups describe reusable draw state

An initialized `DrawSetup` is a reusable bundle of shaders, optional input
layout, topology, rasterizer, depth-stencil, and blend state. It does not own
geometry, render targets, per-view data, or draw calls. The shader manager owns
logical shader resources; the setup retains shared COM references to native
shaders and state after successful initialization.

The vertex-color pass uses position and color from `NormalVertex`, triangle
topology, depth testing and writing, back-face culling, and opaque blending.
Future passes may select different setups without changing geometry ownership.

### Preparation and execution remain in one concrete pass

`VertexColorMeshPass` exposes two phases:

- `PrepareCommands(frame, view, meshes)` copies shader-ready view constants and
  per-instance model matrices without changing GPU state. It rejects an empty
  viewport or non-invertible view-projection matrix and skips invalid mesh
  handles. It preserves extraction order.
- `ExecuteCommands(context, commands, meshes)` applies the fixed setup,
  uploads constants, resolves each handle again, binds geometry, and issues
  whole-mesh indexed draws. A stale handle is skipped; an upload failure fails
  execution.

`MeshDrawCommand` owns a `MeshHandle` and `ObjectConstants`; `MeshPassCommands`
owns `FrameConstants` and a vector of those commands. Neither structure holds
component references or raw mesh pointers. A graph callback owns its prepared
commands by value, while borrowing the renderer for execution. Mesh handles
belong to the renderer's own `MeshManager`.

View constants are uploaded when a pass executes, so preparing another view
cannot overwrite the earlier view's GPU state. Primitive HLSL computes model
then view-projection using `b2 ObjectConstants.model` and
`b0 FrameConstants.viewProjection`. CPU row-vector matrices are transposed at
the preparation boundary.

There is no separate `MeshDrawExecutor`. A second mesh pass may reveal shared
submission code later; that concrete overlap should determine what to extract.

### ECS components describe persistent world state

The current renderable entity combines data-only `TransformComponent` and
`MeshComponent`. The mesh component stores a shared `MeshHandle`; it contains
no COM pointer, draw setup, camera, or rendering code. The extractor copies
world-space TRS in scale, rotation, translation order. Hierarchical transform
evaluation remains unimplemented.

Extraction does not resolve GPU resources or select a particular view. Resource
validation happens during pass preparation and again during execution. Future
view-dependent culling, material resolution, and sorting belong after the
camera-independent snapshot, without coupling generic ECS storage to rendering.

### The graph owns attachment state and ordering

`EditorProgram` owns the offscreen viewport and imports its color and depth
resources into `RenderGraph`. `Renderer::AddScenePasses()` registers the
`VertexColorMeshes` pass with color/depth clears and read-write depth access.
The graph compiles dependencies, realizes graph-owned textures, binds declared
attachments and shader resources, performs clears, and supplies the viewport.
The callback owns draw-local state and submission.

An empty mesh list still produces the clear pass. Execution failure is
propagated from the callback to the editor render boundary. Graph-held resource
pointers and callbacks are discarded by `Reset()` before viewport resize or
shutdown. The renderer and its pass must outlive any graph using their callbacks.

The swap chain and ImGui remain outside the graph. The editor restores the
presentation target after scene rendering and submits UI draw data whose
viewport image samples the rendered color texture.

### Materials wait for reusable appearance

A future material should reference rendering behavior and supply texture,
sampler, and parameter handles. It should not expose D3D11 objects to gameplay,
tools, or ECS code. Opaque/translucent classification may be a material
property, but concrete pass scheduling remains a renderer responsibility.
One fixed vertex-color appearance does not require a general material system.

## Current baseline

The editor renders an indexed, vertex-colored cube from ECS-extracted state.
`EditorProgram::Impl` owns a `World`, registers and enables its extraction
system, uploads the demo cube, and creates an active scene with a transform/mesh
entity. It retains a borrowed extractor pointer while the world group owns the
system. The previous `EditorPreviewScene`, `StaticMeshDrawItem`, and
`SimpleMeshRenderer` path has been removed.

`Update()` advances the public world simulation entry point. A visible viewport
runs `PreparePresentation()` after UI and resize decisions, takes the completed
snapshot, assigns its engine frame number, and supplies camera data to the
renderer. A hidden viewport discards pending output. Simulation and presentation
remain separate so rendering can continue while simulation is paused.

The working pieces are:

- D3D11 device, immediate context, swap chain, and back-buffer lifecycle
- Offscreen editor viewport with color and depth targets
- Typed shader loading and reflected metadata through `ShaderManager`
- API-independent procedural mesh data and a unit-cube factory
- Generation-checked mesh resource ownership through `MeshManager`
- Active-scene ECS extraction into copied `RenderFrameData`
- Per-view `MeshPassCommands` and pass-owned view/object constant buffers
- Texture dependency compilation and execution through `RenderGraph`
- Editor submission through `Renderer::AddScenePasses()`

## Milestones and next work

### Static-mesh vertical slice — implemented

Procedural CPU mesh data, validated immutable GPU upload, managed mesh handles,
and indexed vertex-color drawing form the current vertical slice. The concrete
pass owns preparation and execution, and ECS extraction feeds the running
viewport. Mesh generation uses left-handed, Z-up coordinates (`+X` forward,
`+Y` right, `+Z` up).

### Stable scene submission — foundation implemented

Snapshots and per-view commands support multiple instances with distinct
transforms and deterministic stale-handle skipping. Preparation and execution
are independent phases, and commands own the constants required after the
snapshot is released. Further work should exercise real multi-view features
and additional procedural/imported meshes. Tint, submesh ranges, and appearance
overrides require explicit data when their corresponding features are added.

### Resource identity and reusable appearance — partially implemented

Mesh identity, generation validation, release, and slot reuse are implemented.
Materials, asset import, streaming, and fallback resources remain future work.
A likely later relationship is:

```text
MeshComponent
  -> MeshHandle
  -> MaterialHandle, when materials exist

Pass-specific commands
  -> managed geometry and appearance identities
  -> copied per-draw constants
  -> resource lifetime valid through execution
```

Introduce centralized draw-setup caching only when multiple consumers need it.
Any retained description must own its input-element data rather than a temporary
`std::span`.

### Generic ECS render extraction — integrated

The world owns simulation, transform, and extraction system groups. Application
composition owns the world and calls its public entry points outside active
traversals. `PreparePresentation()` replays editor requests, evaluates transform
systems, and runs extraction without structural playback between the latter
two phases.

`RenderExtractionSystem` copies the selected scene's mesh handles and world
transforms, then transfers one completed result through `TakeFrame()`. The
editor consumes that result through renderer command preparation. Future
transform hierarchies and richer renderable components can extend extraction
without changing the renderer's dependency direction.

### Visibility, sorting, and batching — deferred

Add these when scene scale makes them useful:

- Mesh bounds and per-view frustum culling
- Sorting by pass, draw setup, material, and mesh
- Instanced draws for measured high-volume repetition
- D3D11 state caching when redundant binding is observable
- Parallel work when profiling supports it

These belong to extraction and submission, not generic ECS storage or GPU mesh
ownership. The current pass preserves extraction order and performs none of
these optimizations.

### Explicit pass orchestration — foundation implemented

The graph implements declaration, dependency compilation, GPU texture
realization, and attachment execution. The vertex-color mesh pass is its first
scene consumer. A concrete second objective should determine additional
requirements: editor selection, depth-only rendering, shadows, post-processing,
or multiple views.

The editor-grid shader and C++ grid stub are not connected to graph execution.
A future grid pass will need its own draw state and constants, plus a read-only
DSV for the viewport depth texture. Those changes are outside this mesh-command
integration. Stencil-plane access, subresources, and independent depth/stencil
load behavior remain deferred as well.

## Responsibility guide

| State | Owner |
| --- | --- |
| Persistent instance transform and mesh identity | ECS components |
| Camera-independent frame snapshot | Extraction system, then its consumer |
| Per-view CPU constants and draw list | `MeshPassCommands` owned by a graph callback |
| GPU geometry lifetime | `MeshManager` and `StaticMesh` |
| Fixed shaders, draw state, and mesh constant buffers | `VertexColorMeshPass` |
| Output targets, clears, texture binding, and ordering | `RenderGraph` |
| Swap chain and D3D11 device | `Renderer` |
| Window lifecycle | `Engine` |
| World, editor camera, viewport, and frame composition | `EditorProgram` |

## Explicitly deferred

- Multiple graphics backends or a generic RHI
- A universal material or pass-definition language
- General asset import, streaming, and hot reload
- Mesh bounds, culling, sorting, batching, and instancing
- Draw-setup descriptor hashing and caching
- Shader reflection as the sole input-layout source
- Shader permutations and automatic variant management
- Multithreaded D3D11 command recording
- General state-change deduplication
- Stencil-plane access and independent depth/stencil load and clear operations
- Geometry, hull, and domain shader integration
- Editor-grid submission and a read-only viewport DSV

## Decision rule

Complete the next visible feature with clear ownership and copied submission
data. Repeated resource creation, state setup, geometry binding, or draw loops
across actual passes provide evidence for a shared abstraction. The current
vertex-color pass keeps preparation and execution together until such overlap
exists.
