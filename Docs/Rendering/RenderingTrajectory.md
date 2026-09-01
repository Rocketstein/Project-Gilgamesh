# Project Gilgamesh Rendering Trajectory

## Purpose

This document records the intended evolution of Project Gilgamesh's renderer. The goal is not to build a general-purpose rendering framework in advance. Each abstraction should appear when a grand-strategy feature gives it a concrete job.

The renderer is currently D3D11-specific. A cross-API rendering interface is deliberately out of scope until a second backend becomes a real requirement.

## Architectural principles

### Graphics pipelines describe how a draw is processed

A graphics pipeline is an immutable, reusable collection of draw-processing state. It does not own geometry, render targets, per-frame data, or the draw call.

The initial pipeline contains:

- A required vertex shader
- An optional pixel shader, allowing future depth-only draws
- An optional input layout, allowing `SV_VertexID` full-screen draws
- Primitive topology

Rasterizer, blend, and depth-stencil state should join the pipeline when the first feature needs non-default behavior.

The shader manager owns logical shader resources. A graphics pipeline resolves typed shader handles during initialization and retains shared COM references to the native shaders it requires. This keeps binding self-contained and gives an initialized pipeline a clear lifetime guarantee.

### Render passes describe why and where rendering happens

A render pass owns an objective and its output context: render targets, clears, viewport, ordering, and any transitions or resource preparation around its draws.

Pipelines do not register passes. The dependency points in the other direction:

```text
Render pass
  -> selects draw items
  -> binds their graphics pipelines
  -> binds geometry and resources
  -> issues draws
```

One pass may use several pipelines, and one pipeline may be reusable by several passes. An explicit render-pass abstraction should be introduced when the renderer gains an off-screen target or meaningful pass ordering. Province picking is the likely first natural use case.

### Materials describe reusable appearance

A future material should reference a graphics pipeline and supply textures, samplers, and parameter data. It should not expose D3D11 objects to gameplay or ECS code.

```text
Material
  -> GraphicsPipelineHandle
  -> texture and sampler handles
  -> parameter data
```

Broad scheduling classifications such as opaque or translucent may be material properties, but concrete pass scheduling should remain a renderer or rendering-technique responsibility.

### ECS does not drive the renderer directly

Rendering systems may query ECS components, but the renderer should not depend on or traverse the ECS registry. A later extraction step will convert visible world state into renderer-owned draw data.

```text
ECS and world state
  -> visibility and render extraction
  -> render items or draw packets
  -> sorting and batching
  -> pass execution
  -> GPU submission
```

Renderable components should contain engine handles such as `MeshHandle` and `MaterialHandle`, never D3D11 pointers. Geographic data also does not need to become one render entity per province; the strategic map will likely use large batches, ID textures, and lookup buffers.

## Milestones

### Milestone 1: basic graphics pipeline — complete

The primitive triangle establishes the first pipeline boundary:

```text
Load Primitive shaders
  -> initialize GraphicsPipeline
  -> begin frame
  -> bind pipeline
  -> bind vertex buffer
  -> draw
  -> end frame
```

`GraphicsPipeline` now owns shader, input-layout, and topology binding. The triangle call site still owns its vertex buffer and issues `Draw()`, which is the intended separation.

Completion criteria:

- Pipeline initialization validates and resolves its shader handles.
- Initialization is atomic: failed creation does not leave partial state.
- `Bind()` needs only the D3D11 device context.
- The triangle call site no longer creates an input layout or binds shaders and topology manually.
- Resize, minimization, occlusion, and presentation behavior remain intact.

### Milestone 2: finish the small rendering foundation

Add only the resource and state support needed for the first map prototype:

- Vertex and index buffer ownership wrappers
- Constant-buffer update support
- Texture and sampler ownership
- An orthographic camera with pan and zoom
- Rasterizer, blend, and depth-stencil pipeline state as concrete features require them

Keep resource creation D3D11-specific. Do not introduce a generic RHI or a pipeline cache during this milestone.

Move window and frame-loop orchestration into `Application` when doing so makes the sample or client boundary clearer. The engine should eventually stop using `main.cpp` as its rendering test harness.

### Milestone 3: strategic-map color rendering

Build the first portfolio-defining vertical slice rather than extending the generic triangle sample:

- Render a textured or data-driven province map.
- Support camera pan and zoom.
- Color provinces through ownership or country lookup data.
- Add selection and hover visualization.
- Add borders using either a separate draw/pipeline or an ID-texture technique, based on the chosen map representation.

This can initially be an implicit back-buffer map pass owned by a `MapRenderer`. It does not require a general render graph.

### Milestone 4: province picking and explicit passes

Introduce explicit pass structure when province picking creates the first off-screen output:

```text
MapColorPass
  -> province fill and border draws

ProvincePickingPass
  -> province ID target
  -> picking pipeline
```

Passes select pipelines; pipelines remain unaware of passes. Start with explicit orchestration inside `MapRenderer` or `Renderer`. A registry or graph is unnecessary while the pass order remains short and static.

### Milestone 5: materials and centralized pipeline lifetime

Introduce materials after textures and shader parameters form reusable combinations. Introduce a pipeline manager only when multiple rendering subsystems need centralized creation, lifetime, deduplication, or lookup.

Likely resource relationships:

```text
RenderItem
  -> PipelineHandle
  -> MeshHandle
  -> MaterialHandle
  -> per-draw data
```

If pipeline descriptions are retained for caching or hot reload, copy their input-element data. Never retain the temporary `std::span` supplied during creation.

### Milestone 6: ECS extraction and draw submission

Once the ECS and visible render population justify it, add a renderer-facing extraction stage and frame-local draw packets. Sort or batch packets by pass, pipeline, and material to reduce redundant state changes.

This is the appropriate point to consider:

- A draw-command list
- Pipeline handles in submitted commands
- Pass ranges within a sorted command list
- A D3D11 state cache
- Parallel visibility or extraction work

The ECS affects submission and ownership boundaries, not the implementation of `GraphicsPipeline` itself.

## Explicitly deferred

The following remain deferred until a measured or feature-driven need appears:

- Multiple graphics backends or a generic RHI
- Render graph and automatic resource dependency scheduling
- Pipeline descriptor hashing and caching
- Shader reflection as the sole input-layout source
- Shader permutations and automatic variant management
- Shader hot reload and dependency-driven pipeline rebuilding
- Multithreaded D3D11 command recording
- General state-change deduplication
- Geometry, hull, and domain shader support
- A universal material or pass-definition language

## Decision rule

Prefer the smallest abstraction that completes the next visible grand-strategy feature. When a concept begins appearing in multiple call sites—pipeline creation, resource binding, pass setup, or draw submission—that repetition is evidence for the next abstraction.
