cmake_minimum_required(VERSION 3.21)

project(LSPConvert LANGUAGES CXX)

if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Build configurations" FORCE)
else()
	if(NOT CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
	endif()
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
	if(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
		message(FATAL_ERROR "CMAKE_BUILD_TYPE must be Debug or Release")
	endif()
endif()

# Include the converter, portability helpers, and vendored implementation.
file(GLOB_RECURSE LSPCONVERT_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(LSPConvert ${LSPCONVERT_SOURCES})
target_compile_features(LSPConvert PRIVATE cxx_std_17)
set_target_properties(LSPConvert PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(LSPConvert PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
	# /MT in Release and /MTd in Debug: no Visual C++ runtime DLL dependency.
	set_property(TARGET LSPConvert PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	target_compile_definitions(LSPConvert PRIVATE _CONSOLE)
elseif(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_compile_definitions(LSPConvert PRIVATE MACOS_LINUX)
endif()

# Keep all generated executables inside the build directory.
set_target_properties(LSPConvert PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

install(TARGETS LSPConvert RUNTIME DESTINATION .)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME LSPConvert)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
if(WIN32)
	set(CPACK_PACKAGE_FILE_NAME LSPConvert-win)
elseif(APPLE)
	set(CPACK_PACKAGE_FILE_NAME LSPConvert-mac)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CPACK_PACKAGE_FILE_NAME LSPConvert-ubuntu)
endif()
include(CPack)
