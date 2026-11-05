cmake_minimum_required(VERSION 3.28)


project(rme)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# No C++20 modules are used; skip the per-file dependency scan step.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Explicitly set configuration types for multi-config generators
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE)

# Compiler-specific flags
if(MSVC)
	set(CMAKE_CXX_FLAGS                "/W3 /wd4996 /wd4800 /wd4100 /wd4706 /EHsc")
	set(CMAKE_CXX_FLAGS_DEBUG          "/Od /Zi /D__DEBUG__")
	set(CMAKE_CXX_FLAGS_MINSIZEREL     "/O1 /DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE        "/O2 /DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /DNDEBUG")
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
else()
	set(CMAKE_CXX_FLAGS                "-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated-declarations -Wno-overloaded-virtual -Wno-strict-aliasing -Wno-sign-compare -Wno-unused-function -Wunused-result")
	set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g -D__DEBUG__")
	set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
endif()

find_package(OpenGL REQUIRED)
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf REQUIRED)
find_package(LibLZMA REQUIRED)

if(WIN32)
    set(Boost_THREADAPI win32)
endif()
find_package(Boost 1.34.0 COMPONENTS thread system REQUIRED)

find_package(wxWidgets 3.3 COMPONENTS html aui gl adv core net base propgrid stc REQUIRED)


find_package(ZLIB REQUIRED)


# wxWidgets_USE_FILE is set by vcpkg/system wxWidgets, but empty with Conan
if(wxWidgets_USE_FILE)
	include(${wxWidgets_USE_FILE})
endif()

find_package(NanoSVG CONFIG QUIET)
if(NOT TARGET NanoSVG::nanosvg)
	find_file(NANOSVG_CONFIG_FILE
		NAMES NanoSVGConfig.cmake nanosvg-config.cmake
		PATH_SUFFIXES share/nanosvg share/NanoSVG
	)
	if(NANOSVG_CONFIG_FILE)
		get_filename_component(NANOSVG_CONFIG_DIR "${NANOSVG_CONFIG_FILE}" DIRECTORY)
		find_package(NanoSVG CONFIG QUIET PATHS "${NANOSVG_CONFIG_DIR}" NO_DEFAULT_PATH)
	endif()
endif()
if(NOT TARGET NanoSVG::nanosvg)
	find_package(nanosvg CONFIG QUIET)
endif()
if(NOT TARGET NanoSVG::nanosvg)
	find_library(NANOSVG_LIB NAMES nanosvg)
	find_library(NANOSVGRAST_LIB NAMES nanosvgrast)
endif()
if(NOT TARGET NanoSVG::nanosvg AND NOT NANOSVG_LIB)
	# Fallback: if NanoSVG was not found as a CMake package or library, compile from headers
	find_path(NANOSVG_INCLUDE_DIR NAMES nanosvg/nanosvg.h nanosvg.h)
	if(NANOSVG_INCLUDE_DIR)
		message(STATUS "NanoSVG package not found, creating fallback library from headers in ${NANOSVG_INCLUDE_DIR}...")
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/nanosvg_impl.c"
			"#include <stdio.h>\n"
			"#include <string.h>\n"
			"#include <math.h>\n"
			"#define NANOSVG_IMPLEMENTATION\n"
			"#if __has_include(<nanosvg/nanosvg.h>)\n"
			"#include <nanosvg/nanosvg.h>\n"
			"#else\n"
			"#include <nanosvg.h>\n"
			"#endif\n"
		)
		file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/nanosvgrast_impl.c"
			"#include <stdio.h>\n"
			"#include <string.h>\n"
			"#include <math.h>\n"
			"#define NANOSVGRAST_IMPLEMENTATION\n"
			"#define NANOSVG_ALL_COLOR_KEYWORDS\n"
			"#if __has_include(<nanosvg/nanosvgrast.h>)\n"
			"#include <nanosvg/nanosvgrast.h>\n"
			"#else\n"
			"#include <nanosvgrast.h>\n"
			"#endif\n"
		)
		add_library(nanosvg_fallback STATIC
			"${CMAKE_CURRENT_BINARY_DIR}/nanosvg_impl.c"
			"${CMAKE_CURRENT_BINARY_DIR}/nanosvgrast_impl.c"
		)
		target_include_directories(nanosvg_fallback PUBLIC "${NANOSVG_INCLUDE_DIR}")
		add_library(NanoSVG::nanosvg ALIAS nanosvg_fallback)
		add_library(NanoSVG::nanosvgrast ALIAS nanosvg_fallback)
	endif()
endif()
if(TARGET NanoSVG::nanosvgrast)
	list(APPEND wxWidgets_LIBRARIES NanoSVG::nanosvgrast)
elseif(NANOSVGRAST_LIB)
	list(APPEND wxWidgets_LIBRARIES ${NANOSVGRAST_LIB})
endif()
if(TARGET NanoSVG::nanosvg)
	list(APPEND wxWidgets_LIBRARIES NanoSVG::nanosvg)
elseif(NANOSVG_LIB)
	list(APPEND wxWidgets_LIBRARIES ${NANOSVG_LIB})
endif()

find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)

# nanovg: try vcpkg/Conan first, fallback to ext/nanovg
find_package(nanovg CONFIG QUIET)
if(NOT nanovg_FOUND)
	message(STATUS "nanovg not found via package manager, using ext/nanovg...")
	
	# Build nanovg from ext/nanovg source
	add_library(nanovg_lib STATIC
		${CMAKE_CURRENT_SOURCE_DIR}/ext/nanovg/src/nanovg.c
	)
	target_include_directories(nanovg_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ext/nanovg/src)
	add_library(nanovg::nanovg ALIAS nanovg_lib)
endif()

include(source/CMakeLists.txt)

set(RME_APPEARANCES_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/source/protobuf/appearances.proto")
protobuf_generate_cpp(RME_PROTO_SRCS RME_PROTO_HDRS "${RME_APPEARANCES_PROTO}")

# Removed WIN32 to enable Console subsystem for persistent debugging logs
add_library(rme_proto STATIC ${RME_PROTO_SRCS} ${RME_PROTO_HDRS})
set_target_properties(rme_proto PROPERTIES CXX_STANDARD 17)
set_target_properties(rme_proto PROPERTIES CXX_STANDARD_REQUIRED ON)
target_include_directories(rme_proto PUBLIC
	${Protobuf_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
)
if(TARGET protobuf::libprotobuf)
	target_link_libraries(rme_proto PUBLIC protobuf::libprotobuf)
else()
	target_link_libraries(rme_proto PUBLIC ${Protobuf_LIBRARIES})
endif()

# Removed WIN32 to enable Console subsystem for persistent debugging logs
add_executable(rme ${rme_H} ${rme_SRC})

set_target_properties(rme PROPERTIES CXX_STANDARD 23)
set_target_properties(rme PROPERTIES CXX_STANDARD_REQUIRED ON)
target_compile_definitions(rme PRIVATE WXWIN_COMPATIBILITY_3_3 NANOVG_GL3)
target_precompile_headers(rme PRIVATE source/app/main.h)
target_link_libraries(rme PRIVATE rme_proto)
# Defines NANOVG_GL3_IMPLEMENTATION, which must not leak into a unity batch
set_source_files_properties(source/util/nanovg_canvas.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
	target_compile_options(rme PRIVATE /MP)
endif()
if(MSVC AND CMAKE_UNITY_BUILD)
	target_compile_options(rme PRIVATE /bigobj)
endif()
 
# Copy assets to build directory
add_custom_command(TARGET rme POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
	"${CMAKE_CURRENT_SOURCE_DIR}/source/assets"
	"$<TARGET_FILE_DIR:rme>/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
	"${CMAKE_CURRENT_SOURCE_DIR}/data"
	"$<TARGET_FILE_DIR:rme>/data"
	COMMENT "Copying assets to output directory..."
)


# Link against wxWidgets (works with both vcpkg and Conan)
if(TARGET wxWidgets::wxWidgets)
	# Conan provides a single wxWidgets::wxWidgets target
	target_link_libraries(rme PRIVATE wxWidgets::wxWidgets)
	if(TARGET NanoSVG::nanosvgrast)
		target_link_libraries(rme PRIVATE NanoSVG::nanosvgrast)
	elseif(NANOSVGRAST_LIB)
		target_link_libraries(rme PRIVATE ${NANOSVGRAST_LIB})
	endif()
	if(TARGET NanoSVG::nanosvg)
		target_link_libraries(rme PRIVATE NanoSVG::nanosvg)
	elseif(NANOSVG_LIB)
		target_link_libraries(rme PRIVATE ${NANOSVG_LIB})
	endif()
else()
	# vcpkg/system provides wxWidgets_LIBRARIES variable
	target_link_libraries(rme PRIVATE ${wxWidgets_LIBRARIES})
	if(TARGET NanoSVG::nanosvgrast)
		target_link_libraries(rme PRIVATE NanoSVG::nanosvgrast)
	elseif(NANOSVGRAST_LIB)
		target_link_libraries(rme PRIVATE ${NANOSVGRAST_LIB})
	endif()
	if(TARGET NanoSVG::nanosvg)
		target_link_libraries(rme PRIVATE NanoSVG::nanosvg)
	elseif(NANOSVG_LIB)
		target_link_libraries(rme PRIVATE ${NANOSVG_LIB})
	endif()
endif()

# Include directories (for legacy find modules)
target_include_directories(rme PRIVATE
	${Boost_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${Protobuf_INCLUDE_DIRS}
	${LIBLZMA_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
	${LUA_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/source
)

# Link other libraries (works with both vcpkg targets and Conan targets)
if(TARGET Boost::boost)
	target_link_libraries(rme PRIVATE Boost::boost Boost::thread Boost::system)
else()
	target_link_libraries(rme PRIVATE ${Boost_LIBRARIES})
endif()


if(TARGET ZLIB::ZLIB)
	target_link_libraries(rme PRIVATE ZLIB::ZLIB)
else()
	target_link_libraries(rme PRIVATE ${ZLIB_LIBRARIES})
endif()

if(TARGET protobuf::libprotobuf)
	target_link_libraries(rme PRIVATE protobuf::libprotobuf)
else()
	target_link_libraries(rme PRIVATE ${Protobuf_LIBRARIES})
endif()

if(TARGET LibLZMA::LibLZMA)
	target_link_libraries(rme PRIVATE LibLZMA::LibLZMA)
else()
	target_link_libraries(rme PRIVATE ${LIBLZMA_LIBRARIES})
endif()

target_link_libraries(rme PRIVATE ${OPENGL_LIBRARIES})
target_link_libraries(rme PRIVATE glad::glad)
target_link_libraries(rme PRIVATE glm::glm)
target_link_libraries(rme PRIVATE nanovg::nanovg)
target_link_libraries(rme PRIVATE spdlog::spdlog)
target_link_libraries(rme PRIVATE tomlplusplus::tomlplusplus)
target_link_libraries(rme PRIVATE sol2::sol2)
target_link_libraries(rme PRIVATE cpr::cpr)
target_link_libraries(rme PRIVATE WebP::webp)
target_link_libraries(rme PRIVATE ${LUA_LIBRARIES})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "rme")

# --- Unit test for universal rotator ---
set(TEST_ROTATOR_SRC ${rme_SRC})
list(REMOVE_ITEM TEST_ROTATOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/app/application.cpp")

add_executable(test_universal_rotator ${rme_H} ${TEST_ROTATOR_SRC} tests/test_universal_rotator.cpp)
target_link_libraries(test_universal_rotator PRIVATE rme_proto)
set_target_properties(test_universal_rotator PROPERTIES CXX_STANDARD 23)
set_target_properties(test_universal_rotator PROPERTIES CXX_STANDARD_REQUIRED ON)
target_compile_definitions(test_universal_rotator PRIVATE WXWIN_COMPATIBILITY_3_3 NANOVG_GL3)
target_precompile_headers(test_universal_rotator PRIVATE source/app/main.h)

if(TARGET wxWidgets::wxWidgets)
	target_link_libraries(test_universal_rotator PRIVATE wxWidgets::wxWidgets)
	if(TARGET NanoSVG::nanosvgrast)
		target_link_libraries(test_universal_rotator PRIVATE NanoSVG::nanosvgrast)
	elseif(NANOSVGRAST_LIB)
		target_link_libraries(test_universal_rotator PRIVATE ${NANOSVGRAST_LIB})
	endif()
	if(TARGET NanoSVG::nanosvg)
		target_link_libraries(test_universal_rotator PRIVATE NanoSVG::nanosvg)
	elseif(NANOSVG_LIB)
		target_link_libraries(test_universal_rotator PRIVATE ${NANOSVG_LIB})
	endif()
else()
	target_link_libraries(test_universal_rotator PRIVATE ${wxWidgets_LIBRARIES})
	if(TARGET NanoSVG::nanosvgrast)
		target_link_libraries(test_universal_rotator PRIVATE NanoSVG::nanosvgrast)
	elseif(NANOSVGRAST_LIB)
		target_link_libraries(test_universal_rotator PRIVATE ${NANOSVGRAST_LIB})
	endif()
	if(TARGET NanoSVG::nanosvg)
		target_link_libraries(test_universal_rotator PRIVATE NanoSVG::nanosvg)
	elseif(NANOSVG_LIB)
		target_link_libraries(test_universal_rotator PRIVATE ${NANOSVG_LIB})
	endif()
endif()

target_include_directories(test_universal_rotator PRIVATE
	${Boost_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${Protobuf_INCLUDE_DIRS}
	${LIBLZMA_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
	${LUA_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/source
)

if(TARGET Boost::boost)
	target_link_libraries(test_universal_rotator PRIVATE Boost::boost Boost::thread Boost::system)
else()
	target_link_libraries(test_universal_rotator PRIVATE ${Boost_LIBRARIES})
endif()

if(TARGET ZLIB::ZLIB)
	target_link_libraries(test_universal_rotator PRIVATE ZLIB::ZLIB)
else()
	target_link_libraries(test_universal_rotator PRIVATE ${ZLIB_LIBRARIES})
endif()

if(TARGET protobuf::libprotobuf)
	target_link_libraries(test_universal_rotator PRIVATE protobuf::libprotobuf)
else()
	target_link_libraries(test_universal_rotator PRIVATE ${Protobuf_LIBRARIES})
endif()

if(TARGET LibLZMA::LibLZMA)
	target_link_libraries(test_universal_rotator PRIVATE LibLZMA::LibLZMA)
else()
	target_link_libraries(test_universal_rotator PRIVATE ${LIBLZMA_LIBRARIES})
endif()

target_link_libraries(test_universal_rotator PRIVATE ${OPENGL_LIBRARIES})
target_link_libraries(test_universal_rotator PRIVATE glad::glad)
target_link_libraries(test_universal_rotator PRIVATE glm::glm)
target_link_libraries(test_universal_rotator PRIVATE nanovg::nanovg)
target_link_libraries(test_universal_rotator PRIVATE spdlog::spdlog)
target_link_libraries(test_universal_rotator PRIVATE tomlplusplus::tomlplusplus)
target_link_libraries(test_universal_rotator PRIVATE sol2::sol2)
target_link_libraries(test_universal_rotator PRIVATE cpr::cpr)
target_link_libraries(test_universal_rotator PRIVATE WebP::webp)
target_link_libraries(test_universal_rotator PRIVATE ${LUA_LIBRARIES})

enable_testing()
add_test(NAME test_universal_rotator COMMAND test_universal_rotator)
