# Project Gilgamesh Rendering Trajectory

## Purpose

This document records the intended evolution of rendering in Project
Gilgamesh as a general-purpose ECS engine. It supersedes the earlier
domain-specific trajectory.

The goal is still not to build a general-purpose rendering framework in
advance. Each abstraction should appear when a visible feature gives it a
concrete job. The renderer is currently D3D11-specific; a cross-API rendering
interface remains out of scope until a second backend becomes a real
requirement.

The ECS direction changes the boundary through which scenes reach the
renderer. It does not require mesh resources, draw setups, or the renderer
itself to know about entities or component storage.

## Target boundary

The intended data flow is:

```text
ECS world
  -> transform update, visibility, and render extraction
  -> frame-local RenderView + StaticMeshDrawItem[]
  -> rendering techniques and pass orchestration
  -> D3D11DrawSetup + D3D11 resources
  -> GPU submission
```

The ECS is a source of renderable state, not the renderer's submission API.
Extraction is the dependency firewall: it converts component data into
renderer-owned, frame-local values. The renderer must not query or traverse an
ECS registry.

This keeps both sides reusable. The same ECS world may feed a game view, an
editor view, a thumbnail renderer, or no renderer at all. The renderer can
also draw editor previews and tests that do not originate in an ECS world.

## Architectural principles

### Draw setups describe how a draw is processed

A draw setup is treated as an immutable, reusable collection of
draw-processing state after successful initialization. It does not own
geometry, render targets, per-frame data, or the draw call.

The current draw setup contains:

- A required vertex shader
- An optional pixel shader, allowing depth-only draws
- An optional input layout, allowing `SV_VertexID` full-screen draws
- Primitive topology
- Rasterizer state
- Depth-stencil state
- Blend state

The static-mesh path made rasterizer state explicit, while the editor-grid
requirement made blend state configurable. Safe defaults remain solid,
back-face-culled, and opaque; techniques opt into different behavior through
the draw-state description so their result cannot depend on earlier draw state.

The shader manager owns logical shader resources. A draw setup resolves
typed shader handles during initialization and retains shared COM references
to the native shaders it requires. This keeps binding self-contained and gives
an initialized draw setup a clear lifetime guarantee.

`D3D11DrawSetup` is intentionally not called a pipeline. In this project, a
future `RenderPipeline` will mean the coordinator that orders and executes
render passes.

### Mesh data, GPU geometry, and drawing are separate concepts

Static-mesh support should have three boundaries:

```text
StaticMeshData
  -> CPU vertices, indices, and local bounds

StaticMesh
  -> immutable D3D11 vertex/index buffers and draw metadata

SimpleMeshRenderer
  -> shader, draw setup, per-draw constants, and DrawIndexed
```

`StaticMeshData` is API-independent and suitable for procedural generation,
asset import, validation, and unit tests. `StaticMesh` is a move-only GPU
resource. `SimpleMeshRenderer` is the first fixed rendering technique.

Neither `StaticMesh` nor a renderable component owns a draw setup,
camera, render target, or draw call. Here, *static* means non-deforming mesh
topology; an entity using the mesh may move every frame.

### Frame render data belongs to the renderer

The initial renderer-facing values can remain deliberately small. The exact
API may evolve, but its responsibilities should resemble:

```cpp
struct RenderView
{
    Matrix4 view;
    Matrix4 projection;
};

struct StaticMeshDrawItem
{
    const StaticMesh* mesh;
    Matrix4 localToWorld;
    Color4 tint;
};
```

A draw item is a non-owning, frame-local description. It contains resolved
render data, not component references, ECS queries, or persistent entity
ownership. A source entity identifier may be carried later for editor
selection or diagnostics, but rendering must not require one.

The initial, synchronous editor path may use borrowed `StaticMesh` references
in draw items because submission finishes before the meshes can be replaced or
destroyed. Once resources can unload or submission can be deferred, resolved
resources must remain pinned through submission or draw items must retain safe
handles. Extraction can adopt that policy without reversing the renderer/ECS
dependency direction.

### ECS components describe persistent world state

Once the generic ECS has stable entity lifetime, component storage, and query
semantics, a renderable entity will conceptually combine separate data-only
components such as:

```text
Transform or WorldTransform
  -> spatial state

StaticMeshComponent
  -> MeshHandle
  -> MaterialHandle, when materials exist
```

Renderable components must never contain COM pointers, D3D11 buffers, draw
setups, cameras, or rendering code. Meshes are shared resources,
not duplicated component data. Transform hierarchy evaluation and
local-to-world computation happen before extraction, outside the renderer.

Do not introduce `MeshHandle` merely as a typedef. Add it with the resource
manager and lifetime rules that make it meaningful: validity, ownership,
unloading, stale-handle behavior, and fallback resources.

### Materials describe reusable appearance

A future material should reference a rendering technique and supply textures,
samplers, and parameter data. It should not expose D3D11 objects to gameplay,
tools, or ECS code.

```text
Material
  -> technique identity
  -> texture and sampler handles
  -> parameter data
```

Broad scheduling classifications such as opaque or translucent may be
material properties, but concrete pass scheduling remains a renderer or
rendering-technique responsibility. A general material system is premature
while one fixed mesh appearance is sufficient.

### Render passes describe why and where rendering happens

A render pass owns an objective and its output context: render targets, clears,
viewport, ordering, and any resource preparation around its draws.

Draw setups and rendering techniques do not register passes. The dependency
points in the other direction:

```text
Render pass or explicit orchestration
  -> establishes outputs, clears, viewport, and ordering
  -> selects draw items and invokes rendering techniques

Rendering technique
  -> applies its draw setup
  -> binds geometry and resources
  -> issues draws
```

One pass may invoke several techniques, and one draw setup may be reusable by
several techniques or passes. The existing editor viewport is an offscreen
target, but that alone does not justify a general pass framework. Introduce
explicit passes when a second rendering objective creates real ordering or
resource dependencies, such as an editor-selection ID target, shadows, or a
depth-only prepass.

## Current baseline

The editor prototype currently renders one indexed, vertex-colored cube. The
implementation establishes these working pieces:

- The D3D11 device, context, swap chain, and back-buffer lifecycle
- An offscreen editor viewport with color and depth targets
- Typed shader loading through `ShaderManager`
- API-independent `StaticMeshData` and a procedural unit-cube factory
- Renderer-owned static meshes addressed by generation-checked `MeshHandle`
  values through `MeshManager`
- Frame-local `RenderView` and `StaticMeshDrawItem` submission data
- `SimpleMeshRenderer` ownership of the fixed shaders, `D3D11DrawSetup`,
  model-view-projection constant buffer, and indexed draw loop
- An editor-only `EditorPreviewScene` that owns temporary mesh handles and
  instance transforms

`EditorProgram` owns the editor viewport and preview scene. It creates the
`RenderView`, asks the preview scene to gather draw items, and invokes the
renderer-owned technique. Cube creation, handle lifetime, and instance state
stay in `EditorPreviewScene`; mesh upload, draw state, constants, binding,
and `DrawIndexed()` stay on the renderer side. This is deliberately not yet a
general scene graph or ECS extraction layer.

## Milestones

### Milestone 1: static-mesh vertical slice — in progress

Replace the prototype cube path with a small but real static-mesh path.

#### CPU mesh data and procedural primitives

- Retain `NormalVertex` for now, using its position and color fields while its
  tangent, normal, and UV fields remain zero.
- Define API-independent `StaticMeshData` with vertices and 32-bit indices.
- Generate a unit cube centered at the origin with 8 shared vertices and 36
  indices.
- Follow the engine convention of left-handed, Z-up world space (`+X`
  forward, `+Y` right, `+Z` up) and document the triangle winding.
- Reject empty geometry and out-of-range indices before GPU upload.
- Unit-test cube counts, corners, deferred attributes, and index validity
  without requiring D3D11.

Bounds are a separate pass. Normals should arrive with lighting, and texture
coordinates and tangents should wait until materials require them.

#### GPU mesh resource

- Add a move-only `StaticMesh` that owns immutable D3D11 vertex and index
  buffers plus index count and format.
- Make initialization atomic: failure must not leave a partially usable mesh.
- Validate sizes before narrowing to D3D11's `UINT` fields.
- Keep draw setup, material, transform, camera, and draw submission outside the
  mesh.

Do not introduce a generic buffer hierarchy for this milestone. A second real
geometry use case should determine whether one is valuable.

#### Simple mesh rendering technique

- Add a `SimpleMeshRenderer` that owns the fixed mesh shaders,
  `D3D11DrawSetup`, and constant-buffer updates.
- Accept a `RenderView` and a span of `StaticMeshDrawItem` values.
- Use local-to-world and view/projection data to compute per-draw transforms.
- Apply the draw setup, bind the mesh, and issue `DrawIndexed()` without owning
  the view target or clearing it.

Normal transforms, lighting, tint, and explicit rasterizer/culling policy are
deferred until those systems have a concrete use case.

`EditorProgram` remains responsible for editor viewport orchestration.
`EditorPreviewScene` owns temporary demo handles and transforms, while
`SimpleMeshRenderer` consumes the gathered draw items. The editor program no
longer owns raw mesh buffers, primitive shaders, the primitive draw setup,
primitive constants, or cube-specific lifecycle code.

This milestone deliberately does not require an ECS, asset importer, general
material system, or generic rendering API.

### Milestone 2: stable scene submission

Exercise the renderer-facing boundary with several cube and sphere instances:

- Stabilize `RenderView` and frame-local draw-item responsibilities.
- Submit a list or span instead of making one editor-specific draw call.
- Support distinct transforms and simple appearance values per item.
- Keep draw items non-owning and independent of entities.
- Establish deterministic behavior for missing or invalid mesh references.

This milestone may remain part of Milestone 1 if the implementation stays
small. Its purpose is to prove that the technique consumes scene data rather
than merely relocating the original hard-coded cube.

### Milestone 3: resource identity and reusable appearance

Introduce mesh and material handles only when resources need shared ownership,
lookup, unloading, or reuse across worlds and tools. The manager must define
handle generation, stale-handle detection, and fallback behavior rather than
exposing raw indexes or unvalidated identifiers.

Introduce materials when textures, samplers, or shader parameters form
reusable combinations. Introduce centralized draw-setup lifetime or caching
only when multiple rendering techniques need it.

Likely relationships are:

```text
StaticMeshComponent
  -> MeshHandle
  -> MaterialHandle

Resolved StaticMeshDrawItem
  -> GPU mesh and material resources pinned through submission
  -> local-to-world matrix
  -> frame-local overrides
```

If draw-setup descriptions are retained for caching or hot reload, copy their
input-element data. Never retain a temporary `std::span` supplied during
creation.

### Milestone 4: generic ECS render extraction

After the generic ECS provides reliable entity lifetime, type-safe component
storage, and queries, add an extraction system that:

- Queries the required transform and static-mesh component combination.
- Uses already-evaluated world transforms.
- Resolves resource handles through renderer-facing resource services.
- Emits frame-local draw items for a particular `RenderView`.
- Skips or substitutes invalid resources according to documented policy.

The extraction system is an integration layer; it may depend on the ECS and
renderer-facing data definitions. The low-level renderer, `StaticMesh`, and
`D3D11DrawSetup` must not depend on the ECS. The ECS storage model should
likewise be chosen for general component workloads rather than around renderer
submission details.

### Milestone 5: visibility, sorting, and batching

Add these only when scene scale makes them useful:

- Per-view frustum culling using mesh bounds and world transforms
- Sorting by pass, draw setup or technique, material, and mesh
- Instanced draws for measured high-volume repetition
- A D3D11 state cache when redundant binding is observable
- Parallel transform, visibility, or extraction work when profiling supports it

These are extraction and submission concerns. They do not belong in generic
ECS storage or in the mesh resource itself.

### Milestone 6: explicit pass orchestration

Introduce an explicit pass model when there are multiple rendering objectives
or outputs with meaningful ordering. Likely generic triggers include:

- Editor selection or object-ID rendering
- Shadow maps
- Depth-only rendering
- Post-processing
- Multiple views

Start with explicit orchestration in the renderer. A registry, dependency
solver, or render graph is unnecessary while the pass sequence is short and
static.

## Responsibility guide

Use the following rule when deciding where new state belongs:

| State | Owner |
| --- | --- |
| Persistent entity state | ECS component |
| Shared imported or procedural asset | Resource system |
| Frame-local visible instance data | Render extraction output |
| GPU geometry lifetime | `StaticMesh` |
| Shader and fixed draw behavior | Rendering technique and `D3D11DrawSetup` |
| Output target, clears, and ordering | Render pass or current explicit orchestration |
| Window, swap chain, and D3D11 device | `Renderer` |

## Explicitly deferred

The following remain deferred until a measured or feature-driven need appears:

- Multiple graphics backends or a generic RHI
- Render graph and automatic resource-dependency scheduling
- A universal material or pass-definition language
- General asset import, streaming, and hot reload
- Draw-setup descriptor hashing and caching
- Shader reflection as the sole input-layout source
- Shader permutations and automatic variant management
- Multithreaded D3D11 command recording
- General state-change deduplication
- Geometry, hull, and domain shader support

## Decision rule

Prefer the smallest abstraction that completes the next visible feature. When
a concept begins appearing in multiple call sites—resource creation, draw-state
setup, geometry binding, scene extraction, pass setup, or draw submission—that
repetition is evidence for the next abstraction.

The immediate visible feature is a cube and sphere rendered through a reusable
static-mesh path. The ECS matters now only by preserving the extraction seam;
it should not delay or invade that vertical slice.
