cmake_minimum_required(VERSION 3.14)
project(Lycoris)

# Helpers to delete compiler and linker flags. Have to use these when setting options for MSVC.
# See https://gitlab.kitware.com/cmake/cmake/-/issues/19084 (open as of Nov 2022)
function(delete_match regex varname)
	string(REGEX REPLACE "${regex}" "" result "${${varname}}")
	set("${varname}" "${result}" CACHE STRING "" FORCE)
endfunction()
function(delete_compiler_flags regex)
	delete_match("${regex}" CMAKE_C_FLAGS)
	delete_match("${regex}" CMAKE_C_FLAGS_DEBUG)
	delete_match("${regex}" CMAKE_C_FLAGS_RELWITHDEBINFO)
	delete_match("${regex}" CMAKE_C_FLAGS_MINSIZEREL)
	delete_match("${regex}" CMAKE_C_FLAGS_RELEASE)
	delete_match("${regex}" CMAKE_CXX_FLAGS)
	delete_match("${regex}" CMAKE_CXX_FLAGS_DEBUG)
	delete_match("${regex}" CMAKE_CXX_FLAGS_RELWITHDEBINFO)
	delete_match("${regex}" CMAKE_CXX_FLAGS_MINSIZEREL)
	delete_match("${regex}" CMAKE_CXX_FLAGS_RELEASE)
endfunction()
function(delete_linker_flags regex)
	delete_match("${regex}" LINK_FLAGS)
	delete_match("${regex}" LINK_FLAGS_DEBUG)
	delete_match("${regex}" LINK_FLAGS_RELWITHDEBINFO)
	delete_match("${regex}" LINK_FLAGS_MINSIZEREL)
	delete_match("${regex}" LINK_FLAGS_RELEASE)
endfunction()

# **************************************************************************************************
# Main executable target configuration:

add_executable(Main
	"code/main.cc"
	"code/external.cc"
	"code/base/debug.cc"
	"code/graphics/opengl.cc"
)
target_include_directories(Main PRIVATE "code")
set_target_properties(Main PROPERTIES OUTPUT_NAME "${CMAKE_PROJECT_NAME}")

# Targeting C++20 for designated initialisers. This means the project will only build with:
# GCC >= 8.0, Clang >= 10.0 / Xcode >= 12.0, MSVC 19.21 / Visual Studio 2019 16.1
# Detailed compiler support tables for C++20: https://en.cppreference.com/w/cpp/20
set_target_properties(Main PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(Main PRIVATE /std:c++20)
	target_compile_options(Main PRIVATE /Zc:__cplusplus)
endif()

# Disable exceptions. See P0709R4, the Google Style Guide and the Emscripten docs for rationale.
if (MSVC) # MSVC + Clang-CL
	delete_compiler_flags(" [-/]EH([ascr][\+-]?)*")
	target_compile_options(Main PRIVATE /EHs-c-)
	target_compile_definitions(Main PRIVATE _HAS_EXCEPTIONS=0)
else()
	target_compile_options(Main PRIVATE -fno-exceptions)
endif()

# Disable runtime type information. This project doesn't need typeid or dynamic_cast.
if (MSVC) # MSVC + Clang-CL
	delete_compiler_flags(" [-/]GR-?")
	target_compile_options(Main PRIVATE /GR-)
else()
	target_compile_options(Main PRIVATE -fno-rtti)
endif()

# Enable floating point math optimisations that break IEEE-754 or the C/C++ spec.
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(Main PRIVATE /fp:fast)
else()
	target_compile_options(Main PRIVATE -ffast-math)
endif()

# Disable type-based alias analysis. Allows type punning via unions or casts. This is UB but very
# common and convenient. Bulletproofing against strict aliasing violations is not worth the effort.
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(Main PRIVATE -fno-strict-aliasing)
endif()

# Disable MSVCRT warnings for non-Microsoft-approved "insecure" functions.
if (MSVC) # MSVC + Clang-CL
	target_compile_definitions(Main PRIVATE _CRT_NONSTDC_NO_WARNINGS)
	target_compile_definitions(Main PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# FIXME: This is temporary. We'll want to write our own HTML shell and possibly JS loader.
if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

# **************************************************************************************************
# External libraries:

if (CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	set(EM_USE "-sUSE_SDL=2 -sMIN_WEBGL_VERSION=2 -sFULL_ES3")
	set(EM_PRELOAD "--preload-file ../../data")
	set(EM_SHELL "--shell-file ../../code/shell.html")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EM_USE}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EM_USE}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EM_USE} ${EM_PRELOAD} ${EM_SHELL}")
	target_include_directories(Main PRIVATE ${SDL2_INCLUDE_DIRS})
else()
	set(SDL_SHARED      OFF CACHE BOOL "Build a shared version of the library")
	set(SDL_STATIC      ON  CACHE BOOL "Build a static version of the library")
	set(VIDEO_OPENGL    ON  CACHE BOOL "Include OpenGL support")
	set(VIDEO_OPENGLES  ON  CACHE BOOL "Include OpenGL ES support")
	set(VIDEO_VULKAN    ON  CACHE BOOL "Include Vulkan support")
	add_subdirectory("external/sdl")
	target_link_libraries(Main PRIVATE SDL2-static)
	target_link_libraries(Main PRIVATE SDL2main)
endif()

if (NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	add_subdirectory("external/glad/cmake")
	glad_add_library(glad_loader STATIC API gl:core=4.0)
	target_link_libraries(Main PRIVATE glad_loader)
endif()

add_subdirectory("external/glm")
target_link_libraries(Main PRIVATE glm)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "external/stb")
target_link_libraries(Main PRIVATE stb)

add_library(dear_imgui STATIC
	"external/dear_imgui/imgui.cpp"
	"external/dear_imgui/imgui_demo.cpp"
	"external/dear_imgui/imgui_draw.cpp"
	"external/dear_imgui/imgui_tables.cpp"
	"external/dear_imgui/imgui_widgets.cpp"
	"external/dear_imgui/backends/imgui_impl_sdl2.cpp"
	"external/dear_imgui/backends/imgui_impl_opengl3.cpp")
target_include_directories(dear_imgui PUBLIC
	"external/dear_imgui"
	"external/dear_imgui/backends")
if (NOT CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	target_link_libraries(dear_imgui PUBLIC SDL2-static)
	target_link_libraries(dear_imgui PUBLIC glad_loader)
endif()
set_target_properties(dear_imgui PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
target_link_libraries(Main PRIVATE dear_imgui)

add_library(loguru STATIC "external/loguru/loguru.cpp")
target_include_directories(loguru INTERFACE "external/loguru")
target_link_libraries(loguru PRIVATE stb)
target_link_libraries(Main PRIVATE loguru)

# **************************************************************************************************
# IDE convenience features:

# Set the working directory and startup project for Visual Studio:
set_target_properties(Main PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Main)

# Enable Xcode scheme file generation. Build.py needs this in order to set the working directory.
set_target_properties(Main PROPERTIES XCODE_GENERATE_SCHEME ON)

# FIXME: This is broken in some bizarre way. It seems to add some garbage in CMakeFiles to the
# target sources list, without setting HEADER_FILE_ONLY. I have no idea how.
# Adds all source files under ${subdir} to ${target} with the HEADER_FILE_ONLY property, making them
# visible in Visual Studio and Xcode without affecting compilation.
function(ide_add_source_files target subdir)
	get_target_property(target_sources ${target} SOURCES)
	file(GLOB_RECURSE sources_in_subdir RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}"
		"*.cpp" "*.cc" "*.c" "*.hpp" "*.hh" "*.h" "*.inl" "*.rc")
	foreach(source_file ${sources_in_subdir})
		# Normalise path names:
		string(REPLACE "\\" "/" source_file_normalized ${source_file})
		# Set the group name, e.g. "Code/Platform" from "Code/Platform/Win32.cc":
		get_filename_component(source_file_dir ${source_file_normalized} DIRECTORY)
		source_group("Code/${source_file_dir}" FILES ${source_file})
		# Don't do anything else to files that are already linked to the target:
		list(FIND target_sources ${source_file} file_index_in_target)
		if (${file_index_in_target} GREATER -1)
			continue()
		endif()
		# Add file to target as header:
		set_source_files_properties(${source_file} PROPERTIES HEADER_FILE_ONLY ON)
		target_sources(${target} PRIVATE ${source_file})
	endforeach()
endfunction()
#ide_add_source_files(Main "Code")
