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
set_source_files_properties(${RME_PROTO_SRCS} PROPERTIES COMPILE_OPTIONS "/std:c++17")

# Core static library - ZERO wxWidgets include directories or libraries
add_library(rme_core STATIC ${rme_core_H} ${rme_core_SRC} ${RME_PROTO_SRCS} ${RME_PROTO_HDRS})

set_target_properties(rme_core PROPERTIES CXX_STANDARD 23)
set_target_properties(rme_core PROPERTIES CXX_STANDARD_REQUIRED ON)
target_compile_definitions(rme_core PUBLIC NANOVG_GL3)

target_include_directories(rme_core PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/source
	${CMAKE_CURRENT_BINARY_DIR}
	${Boost_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIR}
	${ZLIB_INCLUDE_DIR}
	${Protobuf_INCLUDE_DIRS}
	${LIBLZMA_INCLUDE_DIRS}
)

target_link_libraries(rme_core PUBLIC
	glad::glad
	glm::glm
	spdlog::spdlog
	tomlplusplus::tomlplusplus
	WebP::webp
	${OPENGL_LIBRARIES}
)

if(TARGET Boost::boost)
	target_link_libraries(rme_core PUBLIC Boost::boost Boost::system)
else()
	target_link_libraries(rme_core PUBLIC ${Boost_LIBRARIES})
endif()

if(TARGET ZLIB::ZLIB)
	target_link_libraries(rme_core PUBLIC ZLIB::ZLIB)
else()
	target_link_libraries(rme_core PUBLIC ${ZLIB_LIBRARIES})
endif()

if(TARGET protobuf::libprotobuf)
	target_link_libraries(rme_core PUBLIC protobuf::libprotobuf)
else()
	target_link_libraries(rme_core PUBLIC ${Protobuf_LIBRARIES})
endif()

if(TARGET LibLZMA::LibLZMA)
	target_link_libraries(rme_core PUBLIC LibLZMA::LibLZMA)
else()
	target_link_libraries(rme_core PUBLIC ${LIBLZMA_LIBRARIES})
endif()

# Main executable (UI layer)
# Removed WIN32 to enable Console subsystem for persistent debugging logs
add_executable(rme ${rme_ui_H} ${rme_ui_SRC})

set_target_properties(rme PROPERTIES CXX_STANDARD 23)
set_target_properties(rme PROPERTIES CXX_STANDARD_REQUIRED ON)
target_compile_definitions(rme PRIVATE WXWIN_COMPATIBILITY_3_3 NANOVG_GL3)
target_precompile_headers(rme PRIVATE source/app/main.h)

target_include_directories(rme PRIVATE
	${LUA_INCLUDE_DIR}
)

target_link_libraries(rme PRIVATE
	rme_core
	nanovg::nanovg
	sol2::sol2
	cpr::cpr
	${LUA_LIBRARIES}
)

if(TARGET Boost::boost)
	target_link_libraries(rme PRIVATE Boost::thread)
endif()

# Link against wxWidgets (only in UI layer)
if(TARGET wxWidgets::wxWidgets)
	target_link_libraries(rme PRIVATE wxWidgets::wxWidgets)
else()
	target_link_libraries(rme PRIVATE ${wxWidgets_LIBRARIES})
endif()

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

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "rme")

# --- Unit test for universal rotator ---
set(TEST_ROTATOR_SRC ${rme_ui_SRC})
list(REMOVE_ITEM TEST_ROTATOR_SRC "${CMAKE_CURRENT_SOURCE_DIR}/source/app/application.cpp")

add_executable(test_universal_rotator ${rme_H} ${TEST_ROTATOR_SRC} tests/test_universal_rotator.cpp)
target_link_libraries(test_universal_rotator PRIVATE rme_core)
set_target_properties(test_universal_rotator PROPERTIES CXX_STANDARD 23)
set_target_properties(test_universal_rotator PROPERTIES CXX_STANDARD_REQUIRED ON)
target_compile_definitions(test_universal_rotator PRIVATE WXWIN_COMPATIBILITY_3_3 NANOVG_GL3)
target_precompile_headers(test_universal_rotator PRIVATE source/app/main.h)

if(TARGET wxWidgets::wxWidgets)
	target_link_libraries(test_universal_rotator PRIVATE wxWidgets::wxWidgets)
else()
	target_link_libraries(test_universal_rotator PRIVATE ${wxWidgets_LIBRARIES})
endif()

target_include_directories(test_universal_rotator PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
	${LUA_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/source
)

if(TARGET Boost::boost)
	target_link_libraries(test_universal_rotator PRIVATE Boost::thread)
endif()

target_link_libraries(test_universal_rotator PRIVATE
	nanovg::nanovg
	sol2::sol2
	cpr::cpr
	${LUA_LIBRARIES}
)

enable_testing()
add_test(NAME test_universal_rotator COMMAND test_universal_rotator)
