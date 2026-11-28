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
find_package(NanoSVG CONFIG QUIET)

# A static wxWidgets references NanoSVG, but its link list puts libnanosvg.a before the wx
# archives, so GNU ld drops it. Whole-archive the files themselves (the imported targets
# would clash with the plain nanosvgrast -> nanosvg dependency). MSVC does not care about order.
function(rme_link_nanosvg target)
	if(NOT TARGET NanoSVG::nanosvgrast)
		return()
	endif()
	if(MSVC)
		target_link_libraries(${target} PRIVATE NanoSVG::nanosvgrast NanoSVG::nanosvg)
	else()
		target_link_libraries(${target} PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:NanoSVG::nanosvgrast>" "$<TARGET_FILE:NanoSVG::nanosvg>" "-Wl,--no-whole-archive")
	endif()
endfunction()
find_package(Catch2 3 CONFIG REQUIRED)

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
else()
	# vcpkg/system provides wxWidgets_LIBRARIES variable
	target_link_libraries(rme PRIVATE ${wxWidgets_LIBRARIES})
endif()

rme_link_nanosvg(rme)

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

# --- Shared setup for unit tests that need the whole editor source tree (tiles, items, brushes) ---
# application.cpp is excluded so its main() does not conflict with the test entry point; the test
# file provides the wxGetApp()/Application::Unload stubs.
function(rme_add_full_editor_test target)
	set(test_sources ${rme_SRC})
	list(REMOVE_ITEM test_sources "${CMAKE_CURRENT_SOURCE_DIR}/source/app/application.cpp")

	add_executable(${target} ${rme_H} ${test_sources} ${ARGN})
	target_link_libraries(${target} PRIVATE rme_proto)
	set_target_properties(${target} PROPERTIES CXX_STANDARD 23)
	set_target_properties(${target} PROPERTIES CXX_STANDARD_REQUIRED ON)
	target_compile_definitions(${target} PRIVATE WXWIN_COMPATIBILITY_3_3 NANOVG_GL3)
	target_precompile_headers(${target} PRIVATE source/app/main.h)

	if(TARGET wxWidgets::wxWidgets)
		target_link_libraries(${target} PRIVATE wxWidgets::wxWidgets)
	else()
		target_link_libraries(${target} PRIVATE ${wxWidgets_LIBRARIES})
	endif()

	rme_link_nanosvg(${target})

	target_include_directories(${target} PRIVATE
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
		target_link_libraries(${target} PRIVATE Boost::boost Boost::thread Boost::system)
	else()
		target_link_libraries(${target} PRIVATE ${Boost_LIBRARIES})
	endif()

	if(TARGET ZLIB::ZLIB)
		target_link_libraries(${target} PRIVATE ZLIB::ZLIB)
	else()
		target_link_libraries(${target} PRIVATE ${ZLIB_LIBRARIES})
	endif()

	if(TARGET protobuf::libprotobuf)
		target_link_libraries(${target} PRIVATE protobuf::libprotobuf)
	else()
		target_link_libraries(${target} PRIVATE ${Protobuf_LIBRARIES})
	endif()

	if(TARGET LibLZMA::LibLZMA)
		target_link_libraries(${target} PRIVATE LibLZMA::LibLZMA)
	else()
		target_link_libraries(${target} PRIVATE ${LIBLZMA_LIBRARIES})
	endif()

	target_link_libraries(${target} PRIVATE ${OPENGL_LIBRARIES})
	target_link_libraries(${target} PRIVATE glad::glad)
	target_link_libraries(${target} PRIVATE glm::glm)
	target_link_libraries(${target} PRIVATE nanovg::nanovg)
	target_link_libraries(${target} PRIVATE spdlog::spdlog)
	target_link_libraries(${target} PRIVATE tomlplusplus::tomlplusplus)
	target_link_libraries(${target} PRIVATE sol2::sol2)
	target_link_libraries(${target} PRIVATE cpr::cpr)
	target_link_libraries(${target} PRIVATE WebP::webp)
	target_link_libraries(${target} PRIVATE ${LUA_LIBRARIES})
endfunction()

# --- Shared registration for standalone Catch2 suites that only need the source tree ---
function(rme_add_catch2_test target)
	add_executable(${target} ${ARGN})
	set_target_properties(${target} PROPERTIES CXX_STANDARD 23)
	set_target_properties(${target} PROPERTIES CXX_STANDARD_REQUIRED ON)
	target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain)
	target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
	add_test(NAME ${target} COMMAND ${target})
endfunction()

# --- Unit test for universal rotator ---
rme_add_full_editor_test(test_universal_rotator tests/test_universal_rotator.cpp)

# --- Unit tests for the magic wand plan and item-level selection (real tiles, items and brushes) ---
rme_add_full_editor_test(test_magic_wand_plan tests/test_magic_wand_plan.cpp)
target_link_libraries(test_magic_wand_plan PRIVATE Catch2::Catch2WithMain)

enable_testing()
add_test(NAME test_universal_rotator COMMAND test_universal_rotator)
add_test(NAME test_magic_wand_plan COMMAND test_magic_wand_plan)

# --- Unit test for subtype pattern mapping ---
rme_add_catch2_test(test_subtype_pattern tests/test_subtype_pattern.cpp)

# --- Unit test for tile stack row mapping (Tile Properties item list) ---
rme_add_catch2_test(test_tile_stack_rows tests/test_tile_stack_rows.cpp)

# --- Unit test for lasso polygon/tile geometry ---
rme_add_catch2_test(test_lasso_geometry tests/test_lasso_geometry.cpp)

# --- Unit test for lasso gesture state machine ---
rme_add_catch2_test(test_lasso_gesture tests/test_lasso_gesture.cpp)

# --- Unit test for box/lasso selection floor scope ---
rme_add_catch2_test(test_selection_floor_scope tests/test_selection_floor_scope.cpp)

# --- Unit test for collection tileset save plan and ordering ---
rme_add_catch2_test(test_collection_save_plan tests/test_collection_save_plan.cpp)

# --- Unit test for the shared tile context menu model (map canvas and Tile Properties) ---
rme_add_catch2_test(test_tile_context_menu tests/test_tile_context_menu.cpp)

# --- Unit test for the item-scoped context menu model (Tile Properties list) ---
rme_add_catch2_test(test_item_context_menu tests/test_item_context_menu.cpp)

# --- Unit test for minimap dirty rect bounds ---
rme_add_catch2_test(test_minimap_rect tests/test_minimap_rect.cpp)

# --- Unit test for pixel-perfect picking of multi-tile grounds ---
rme_add_catch2_test(test_selection_multitile_ground tests/test_selection_multitile_ground.cpp)

# --- Unit test for the magic wand flood fill and border ring ---
rme_add_catch2_test(test_magic_wand tests/test_magic_wand.cpp)

# --- Unit test for outfit walk animation frames ---
rme_add_catch2_test(test_walk_animation tests/test_walk_animation.cpp)

# --- Unit test for Autoborder UI state, preview preconditions, and labels ---
rme_add_catch2_test(test_autoborder_state tests/test_autoborder_state.cpp)

# --- Unit test for the load-warnings.log block formatter ---
rme_add_catch2_test(test_load_warnings_log tests/test_load_warnings_log.cpp source/util/load_warnings_log.cpp)

# --- Unit test for the per-chunk tile mask (Show Light Strength overlay) ---
rme_add_catch2_test(test_chunk_tile_mask tests/test_chunk_tile_mask.cpp)

# --- Unit test for the copy position formats and the position paste parser ---
rme_add_catch2_test(test_position_format tests/test_position_format.cpp source/map/position_format.cpp)

# --- Unit test for map version mismatch loading decisions ---
rme_add_catch2_test(test_map_load_decision tests/test_map_load_decision.cpp)

# --- Unit test for notebook tab close decisions ---
rme_add_catch2_test(test_tab_close_decision tests/test_tab_close_decision.cpp)

# --- Unit test for theme WCAG contrast button text selection ---
rme_add_catch2_test(test_theme_contrast tests/test_theme_contrast.cpp)

# --- Unit test for mismatch dialog presentation formatting ---
rme_add_catch2_test(test_client_mismatch_presentation tests/test_client_mismatch_presentation.cpp)

# --- Unit test for non-destructive switch client preflight evaluation ---
rme_add_catch2_test(test_switch_client_preflight tests/test_switch_client_preflight.cpp)
rme_add_catch2_test(test_blocking_overlay tests/test_blocking_overlay.cpp)

# --- Unit test for the missing ground overlay rule ---
rme_add_catch2_test(test_missing_ground tests/test_missing_ground.cpp)

# --- Full-map tests for Search -> Find Missing Grounds ---
rme_add_full_editor_test(test_search_missing_ground tests/test_search_missing_ground.cpp)
target_link_libraries(test_search_missing_ground PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_search_missing_ground COMMAND test_search_missing_ground)
