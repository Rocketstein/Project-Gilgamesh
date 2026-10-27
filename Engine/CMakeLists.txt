add_library (GilgameshEngine STATIC)

target_sources (GilgameshEngine
	PRIVATE
		Core/Color.h
		Core/Extent2D.h
		Core/BoundingShape.cpp
		Core/BoundingShape.h
		Core/Logging/ILogSink.h
		Core/Logging/Logger.cpp
		Core/Logging/Logger.h
		Core/Logging/LogTypes.h
		Core/Math/Matrix.h
		Core/Math/Quat.h
		Core/Math/Rotator.h
		Core/Math/Vector.h
		Core/Text/Utf8.cpp
		Core/Text/Utf8.h

		Input/InputFrame.cpp
		Input/InputFrame.h
		Input/InputSystem.cpp
		Input/InputSystem.h
		Input/InputTypes.h

		Platform/Windows/Window.cpp
		Platform/Windows/Window.h
		Platform/Windows/Win32InputBackend.cpp
		Platform/Windows/Win32InputBackend.h
		Platform/Windows/Logging/DebugOutputSink.cpp
		Platform/Windows/Logging/DebugOutputSink.h

		ECS/Entity/Entity.h
		ECS/Component/BoundsComponent.h
		ECS/Component/MeshComponent.h
		ECS/Component/TransformComponent.h
		ECS/Framework/ArchetypeResource.h
		ECS/Framework/ArchetypeManager.cpp
		ECS/Framework/ArchetypeManager.h
		ECS/Framework/Archetype.cpp
		ECS/Framework/Archetype.h
		ECS/Framework/Chunk.cpp
		ECS/Framework/Chunk.h
		ECS/Framework/ECSCommand.cpp
		ECS/Framework/ECSCommand.h
		ECS/Framework/ECSCommandBuffer.cpp
		ECS/Framework/ECSCommandBuffer.h
		ECS/Framework/EntityRegistry.cpp
		ECS/Framework/EntityRegistry.h
		ECS/Framework/EntitySlot.h
		ECS/Framework/IDGenerator.h
		ECS/Framework/MemoryArena.cpp
		ECS/Framework/MemoryArena.h
		ECS/Framework/QueryTerm.h
		ECS/Framework/Scene.cpp
		ECS/Framework/Scene.h
		ECS/Framework/World.cpp
		ECS/Framework/World.h
		
		ECS/System/Core/ISystem.h
		ECS/System/Core/ISystem.cpp
		ECS/System/Core/SystemGroup.cpp
		ECS/System/Core/SystemGroup.h

		ECS/System/Transform/LocalTransformSystem.cpp
		ECS/System/Transform/LocalTransformSystem.h
		ECS/System/Transform/TransformHierarchySystem.cpp
		ECS/System/Transform/TransformHierarchySystem.h
		ECS/System/Transform/LocalToWorldSystem.cpp
		ECS/System/Transform/LocalToWorldSystem.h
		ECS/System/Transform/UpdateBoundsSystem.cpp
		ECS/System/Transform/UpdateBoundsSystem.h

		ECS/System/RenderExtractionSystem.cpp
		ECS/System/RenderExtractionSystem.h

		Render/D3D11/D3D11Device.cpp
		Render/D3D11/D3D11Device.h
		Render/D3D11/PresentationSurface.cpp
		Render/D3D11/PresentationSurface.h
		Render/Shader/Shader.cpp
		Render/Shader/Shader.h
		Render/Shader/ShaderManager.cpp
		Render/Shader/ShaderManager.h
		Render/Shader/ShaderTypes.h
		Render/Shader/ShaderReflection.cpp
		Render/Shader/ShaderReflection.h
		Render/Shader/ShaderConstants.h

		Render/DrawSetup/DrawSetupDesc.h
		Render/DrawSetup/DrawSetup.h
		Render/DrawSetup/DrawSetup.cpp

		Render/RenderGraph/Resource/RenderGraphTexture.h
		Render/RenderGraph/RenderGraph.cpp
		Render/RenderGraph/RenderGraph.h
		Render/RenderGraph/GraphPassBuilder.cpp
		Render/RenderGraph/GraphPassBuilder.h


		Render/Passes/IRenderPass.h
		Render/Passes/DebugLinePass.cpp
		Render/Passes/DebugLinePass.h
		Render/Passes/ViewportClearPass.cpp
		Render/Passes/ViewportClearPass.h
		Render/Passes/VertexColorMeshPass.cpp
		Render/Passes/VertexColorMeshPass.h

		Render/Renderer/Renderer.cpp
		Render/Renderer/Renderer.h
		Render/Renderer/RenderView.cpp
		Render/Renderer/RenderView.h
		Render/Renderer/RenderFrameData.h

		Render/Buffers/BufferUsage.h
		Render/Buffers/ConstantBuffer.h
		Render/Buffers/IndexBuffer.h
		Render/Buffers/IndexBuffer.cpp
		Render/Buffers/VertexBuffer.h
		Render/Buffers/VertexBuffer.cpp

		Render/Mesh/StaticMeshData.h
		Render/Mesh/PrimitiveMeshes.cpp
		Render/Mesh/PrimitiveMeshes.h
		Render/Mesh/StaticMesh.cpp
		Render/Mesh/StaticMesh.h
		Render/Mesh/MeshHandle.h
		Render/Mesh/MeshManager.cpp
		Render/Mesh/MeshManager.h
		Render/Mesh/Vertex.h

		Runtime/Engine.cpp
		Runtime/Engine.h
		Runtime/EngineServices.h
		Runtime/FrameContext.h
		Runtime/Program.h
		Runtime/RenderContext.h

		Viewport/ViewportClient.h

		Time/FrameClock.cpp
		Time/FrameClock.h
)

get_target_property (GILGAMESH_ENGINE_SOURCES GilgameshEngine SOURCES)
source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${GILGAMESH_ENGINE_SOURCES})

target_include_directories(GilgameshEngine
	PUBLIC
		${CMAKE_SOURCE_DIR}
)

target_compile_options(GilgameshEngine
    PUBLIC
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>
)

target_compile_definitions(GilgameshEngine
	PUBLIC
		UNICODE
		_UNICODE
		NOMINMAX
		WIN32_LEAN_AND_MEAN
)

target_compile_features(GilgameshEngine
	PUBLIC
		cxx_std_20
)

target_link_libraries(GilgameshEngine
	PRIVATE
		user32
        d3d11
        d3dcompiler
        dxgi
		dxguid
)
