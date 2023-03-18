cmake_minimum_required(VERSION 3.14)
project(ExternalDependencies)
include(FetchContent)

# CMake script to fetch external dependencies using FetchContent. Used by build.py. This is simpler
# to work with than Git submodules and easier than reimplementing FetchContent in Python.

FetchContent_Populate(emsdk
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/emsdk"
	GIT_REPOSITORY https://github.com/emscripten-core/emsdk
	GIT_TAG 0ab19024f08c6673a713e454ef8bd95e174c807f # 3.1.2 (18 Feb 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(sdl
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdl"
	GIT_REPOSITORY https://github.com/libsdl-org/SDL
	GIT_TAG adf31f6ec0be0f9ba562889398f71172c7941023 # release-2.26.3 (7 Feb 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(glad
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glad"
	GIT_REPOSITORY https://github.com/Dav1dde/glad
	GIT_TAG 3b95bf4f9a7a034374092d625e391ddac43fe66f # v2.0.4 (18 Feb 2023)
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
	GIT_TAG 458a1090314a965dd37b02c918d83077a0142ad5 # v1.89.3 (14 Feb 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(implot
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/implot"
	GIT_REPOSITORY https://github.com/epezent/implot
	GIT_TAG 33c5a965f55f80057f197257d1d1cdb06523e963 # v1.89.3 (16 Feb 2023)
	GIT_SHALLOW YES)

FetchContent_Populate(cmake_checks_cache
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake_checks_cache"
	GIT_REPOSITORY https://github.com/cristianadam/cmake-checks-cache.git
	GIT_TAG fa345a9dbdf7dc4a6b27d6b6e5113ab38d0b9db9 # (4 Mar 2022)
	GIT_SHALLOW YES)

FetchContent_Populate(parson
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/parson"
	GIT_REPOSITORY https://github.com/kgabis/parson
	GIT_TAG 3c4ee26dbb3df177a2d7b9d80e154ec435ca8c01 # 1.5.1 (15 Feb 2023)
	GIT_SHALLOW YES)
