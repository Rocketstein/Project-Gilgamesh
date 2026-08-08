add_library (GilgameshEngine STATIC
	Platform/Windows/Window.cpp
	Platform/Windows/Window.h
 "Renderer/D3D11/D3D11Device.h" "Renderer/D3D11/D3D11Device.cpp" "Renderer/Renderer.h" "Renderer/Renderer.cpp" "Renderer/D3D11/PresentationSurface.h" "Renderer/D3D11/PresentationSurface.cpp")

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
)