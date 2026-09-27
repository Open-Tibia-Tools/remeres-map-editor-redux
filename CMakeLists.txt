cmake_minimum_required(VERSION 3.28)


project(rme)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")
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

find_package(wxWidgets 3.2 COMPONENTS html aui gl adv core net base propgrid REQUIRED)


find_package(ZLIB REQUIRED)


# wxWidgets_USE_FILE is set by vcpkg/system wxWidgets, but empty with Conan
if(wxWidgets_USE_FILE)
	include(${wxWidgets_USE_FILE})
endif()

find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
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
add_executable(rme ${rme_H} ${rme_SRC} ${RME_PROTO_SRCS} ${RME_PROTO_HDRS})

set_target_properties(rme PROPERTIES CXX_STANDARD 23)
set_target_properties(rme PROPERTIES CXX_STANDARD_REQUIRED ON)
target_compile_definitions(rme PRIVATE WXWIN_COMPATIBILITY_3_3 NANOVG_GL3)
 
# Copy assets to build directory
add_custom_command(TARGET rme POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	"${CMAKE_CURRENT_SOURCE_DIR}/source/assets"
	"$<TARGET_FILE_DIR:rme>/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
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
if(UNIX AND NOT APPLE)
	find_library(RME_EGL_LIBRARY EGL REQUIRED)
	target_link_libraries(rme PRIVATE ${RME_EGL_LIBRARY})
endif()
target_link_libraries(rme PRIVATE glad::glad)
target_link_libraries(rme PRIVATE glm::glm)
target_link_libraries(rme PRIVATE nanovg::nanovg)
target_link_libraries(rme PRIVATE spdlog::spdlog)
target_link_libraries(rme PRIVATE tomlplusplus::tomlplusplus)
target_link_libraries(rme PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(rme PRIVATE sol2::sol2)
target_link_libraries(rme PRIVATE cpr::cpr)
target_link_libraries(rme PRIVATE WebP::webp)
target_link_libraries(rme PRIVATE ${LUA_LIBRARIES})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "rme")



