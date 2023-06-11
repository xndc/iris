cmake_minimum_required(VERSION 3.14)
project(ExternalDependencies)
include(FetchContent)

# CMake script to fetch external dependencies using FetchContent. Used by build.py. This is simpler
# to work with than Git submodules and easier than reimplementing FetchContent in Python.

FetchContent_Populate(emsdk
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/emsdk"
	GIT_REPOSITORY https://github.com/emscripten-core/emsdk
	GIT_TAG 775ba048040f663abbca9ca66e264ee795f64ef3 # 3.1.41 (6 Jun 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(sdl
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdl"
	GIT_REPOSITORY https://github.com/libsdl-org/SDL
	GIT_TAG ac13ca9ab691e13e8eebe9684740ddcb0d716203 # release-2.26.5 (5 Apr 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(glad
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glad"
	GIT_REPOSITORY https://github.com/Dav1dde/glad
	GIT_TAG d08b1aa01f8fe57498f04d47b5fa8c48725be877 # v2.0.4 (17 Feb 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(glm
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glm"
	GIT_REPOSITORY https://github.com/g-truc/glm
	GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e # 0.9.9.8 (13 Apr 2020)
	GIT_SHALLOW YES)

FetchContent_Populate(stb
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stb"
	GIT_REPOSITORY https://github.com/nothings/stb
	GIT_TAG 5736b15f7ea0ffb08dd38af21067c314d6a3aae9 # (29 Jan 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(dear_imgui
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dear_imgui"
	GIT_REPOSITORY https://github.com/ocornut/imgui
	GIT_TAG 5319d1cffafd5045c4742892c38c9e5cfa23d195 # v1.89.6 (31 May 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(implot
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/implot"
	GIT_REPOSITORY https://github.com/epezent/implot
	GIT_TAG 18758e237e8906a97ddf42de1e75793526f30ce9 # (19 Apr 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(cmake_checks_cache
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake_checks_cache"
	GIT_REPOSITORY https://github.com/cristianadam/cmake-checks-cache.git
	GIT_TAG fa345a9dbdf7dc4a6b27d6b6e5113ab38d0b9db9 # (4 Mar 2022)
	GIT_SHALLOW YES)

FetchContent_Populate(parson
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/parson"
	GIT_REPOSITORY https://github.com/kgabis/parson
	GIT_TAG 60c37844d7a1c97547812cac3423d458c73e60f9 # 1.5.2 (21 May 2023)
	GIT_SHALLOW YES)
