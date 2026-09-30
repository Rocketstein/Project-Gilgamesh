add_library (GilgameshEngine STATIC)

target_sources (GilgameshEngine
	PRIVATE
		Core/Color.h
		Core/Extent2D.h
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
		ECS/Framework/Archetype.h
		ECS/Framework/Chunk.h
		ECS/Framework/EntityRegistry.cpp
		ECS/Framework/EntityRegistry.h
		ECS/Framework/Scene.cpp
		ECS/Framework/Scene.h
		ECS/Framework/World.cpp
		ECS/Framework/World.h

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


		Render/Techniques/SimpleMeshRenderer.cpp
		Render/Techniques/SimpleMeshRenderer.h

		Render/Renderer/Renderer.cpp
		Render/Renderer/Renderer.h
		Render/Renderer/RenderView.h
		Render/Renderer/StaticMeshDrawItem.h

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
