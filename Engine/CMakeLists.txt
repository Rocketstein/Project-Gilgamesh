add_library (GilgameshEngine STATIC)

target_sources (GilgameshEngine
	PRIVATE
		Core/Color.h
		Core/Extent2D.h
		Core/Logging/ILogSink.h
		Core/Logging/Logger.cpp
		Core/Logging/Logger.h
		Core/Logging/LogTypes.h
		Core/Text/Utf8.cpp
		Core/Text/Utf8.h

		Platform/Windows/Window.cpp
		Platform/Windows/Window.h
		Platform/Windows/Logging/DebugOutputSink.cpp
		Platform/Windows/Logging/DebugOutputSink.h

		Render/D3D11/D3D11Device.cpp
		Render/D3D11/D3D11Device.h
		Render/D3D11/PresentationSurface.cpp
		Render/D3D11/PresentationSurface.h
		Render/D3D11/Shader/D3D11Shader.cpp
		Render/D3D11/Shader/D3D11Shader.h
		Render/D3D11/Shader/D3D11ShaderManager.cpp
		Render/D3D11/Shader/D3D11ShaderManager.h
		Render/D3D11/Shader/ShaderTypes.h

		Render/Pipeline/GraphicsPipeline.h
		Render/Pipeline/GraphicsPipeline.cpp
		Render/Renderer/Renderer.cpp
		Render/Renderer/Renderer.h

		Render/VertexTypes/VertexTypes.h

		Runtime/Engine.cpp
		Runtime/Engine.h
		Runtime/EngineServices.h
		Runtime/FrameContext.h
		Runtime/Program.h
		Runtime/RenderContext.h

		Render/Pipeline/PipelineDesc.h

		Time/FrameClock.cpp
        Time/FrameClock.h)

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
        dxgi
)
