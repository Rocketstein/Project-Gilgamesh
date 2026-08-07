add_library (GilgameshEngine
	Platform/Windows/Window.cpp
	Platform/Windows/Window.h
)

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
	PUBLIC
		user32
)