add_library (GilgameshEngine STATIC
	Platform/Windows/Window.cpp
	Platform/Windows/Window.h
 "Render/D3D11/D3D11Device.h" "Render/D3D11/D3D11Device.cpp" "Render/Renderer/Renderer.h" "Render/Renderer/Renderer.cpp" "Render/D3D11/PresentationSurface.h" "Render/D3D11/PresentationSurface.cpp" "Core/Extent2D.h" "Core/Color.h" "Render/VertexTypes/VertexTypes.h")

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
		user32
        d3d11
        dxgi
)