add_library (GilgameshEngine STATIC)

target_sources (GilgameshEngine
	PRIVATE
		Core/Color.h
		Core/Extent2D.h

		Platform/Windows/Window.cpp
		Platform/Windows/Window.h

		Render/D3D11/D3D11Device.cpp
		Render/D3D11/D3D11Device.h
		Render/D3D11/PresentationSurface.cpp
		Render/D3D11/PresentationSurface.h

		"Render/Pipeline/GraphicsPipeline.h" 
		"Render/Pipeline/GraphicsPipeline.cpp"
		"Render/D3D11/Shader/D3D11Shader.cpp"
		"Render/D3D11/Shader/D3D11Shader.h"
		"Render/D3D11/Shader/D3D11ShaderManager.cpp"
		"Render/D3D11/Shader/D3D11ShaderManager.h"
		"Render/D3D11/Shader/ShaderTypes.h"

		Render/Renderer/Renderer.cpp
		Render/Renderer/Renderer.h

		Render/VertexTypes/VertexTypes.h

		Runtime/Application.h "Render/Pipeline/PipelineDesc.h")

get_target_property (GILGAMESH_ENGINE_SOURCES GilgameshEngine SOURCES)
source_group (TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${GILGAMESH_ENGINE_SOURCES})

target_include_directories(GilgameshEngine
	PUBLIC
		${CMAKE_SOURCE_DIR}
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
		DearImGui
		user32
        d3d11
        dxgi
)
