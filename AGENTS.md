# Project Gilgamesh agent notes

These instructions apply to the entire repository.

## Coding rules
- Readability first. Efficiency second.
- Add a brief, readable comment for each logical block and function headers with reasonable complexity.
- Don't just select an approach because it is a minimal solution. Long term architecture matters.

## Platform and build

- The current project is Windows-only, uses Win32 and D3D11, and builds as
  C++20 with MSVC and Ninja.
- Configure from a Visual Studio developer environment. The build expects
  `cl.exe`, the Windows SDK, `fxc.exe`, and `VSINSTALLDIR` to be available.
  Preset architecture selection uses the `external` strategy, so the preset
  does not initialize x64 or x86 compiler variables; use a developer shell
  whose architecture matches the selected preset.
  When an automation shell is not initialized, locate and invoke the installed
  `VsDevCmd.bat`; do not hard-code a Visual Studio version or installation path.
- Use the checked-in presets and build outside the source tree:

  ```powershell
  cmake --preset x64-debug
  cmake --build out/build/x64-debug --target Gilgamesh --parallel
  cmake --build out/build/x64-debug --target GilgameshTests --parallel
  ctest --test-dir out/build/x64-debug --output-on-failure
  ```

- Treat `out/`, `.vs/`, `CMakeUserPresets.json`, generated Ninja files, `.cso`
  files, and CMake File API data as generated or machine-local output.
  Regenerate them instead of editing them.
- Source lists are explicit. Whenever a source or header is added, moved, or
  renamed, update the owning `CMakeLists.txt`; do not assume CMake will discover
  it automatically.
- `GILGAMESH_ENABLE_EDITOR_DIAGNOSTICS` defaults to `ON`. It controls the
  console and Output Log only, not the editor viewport or editor build.
- Dear ImGui is vendored under `ThirdParty/imgui`; do not fetch a second copy
  as part of normal configure or build work.

## Rendering layout and naming

- The renderer is intentionally D3D11-specific. Do not introduce a generic RHI
  or cross-API abstraction unless a task explicitly requires another backend.
- Do not add a `D3D11` prefix to general renderer concepts merely because their
  implementation uses D3D11. Reserve the prefix for core backend/native
  wrappers such as `D3D11Device`, and retain native API names such as
  `ID3D11Device`.
- Canonical renderer locations are:
  - GPU buffer wrappers: `Engine/Render/Buffers/`
  - C++ shader resources: `Engine/Render/Shader/`
  - Draw state setup: `Engine/Render/DrawSetup/`
  - Mesh resources and vertex formats: `Engine/Render/Mesh/`
  - D3D11 device/presentation backend: `Engine/Render/D3D11/`
- `DrawSetup` is the reusable shader/fixed-state bundle. `RenderPipeline` is the
  coordinator that orders and executes passes; do not use those names
  interchangeably.
- Use repository-root-qualified includes across directories, for example
  `#include "Engine/Render/Shader/ShaderManager.h"`. A source file may include
  its own header, or another header in the same directory, by basename.
- The world coordinate system is left-handed and Z-up: `+X` is forward, `+Y`
  is right, and `+Z` is up.

## Shaders

- `Shaders/` contains HLSL assets. `Engine/Render/Shader/` contains the C++
  shader resource and manager implementation; the two directories are not
  interchangeable.
- Register HLSL through `register_shader` in CMake. It uses `fxc`, defaults to
  Shader Model 5.0, and detects stages by the standard entry points `VSMain`,
  `PSMain`, `GSMain`, `HSMain`, `DSMain`, and `CSMain`.
- A shader required by tests must be registered separately for
  `GilgameshTests`; registration for the `Gilgamesh` executable does not make
  its compiled output available to the test executable.
- HLSL warnings are errors. Shared `.hlsli` files beneath `Shaders/` are common
  dependencies and changes to them rebuild registered shaders.

## Tests and documentation

- Tests use the repository's custom test framework, not GoogleTest. The test
  executable accepts either `--list` or one substring filter:

  ```powershell
  & .\out\build\x64-debug\Tests\GilgameshTests.exe DrawSetup
  ```

- Renderer integration tests use the D3D11 WARP device, so they do not require
  a physical GPU. They may depend on compiled shaders in the test output's
  `Shaders` directory.
- `Docs/Architecture/CurrentArchitecture.md` describes the implemented system.
  `Docs/Rendering/RenderingTrajectory.md` includes future and explicitly
  deferred work; do not treat roadmap items as already implemented.
