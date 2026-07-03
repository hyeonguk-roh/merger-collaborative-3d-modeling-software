cmake_minimum_required(VERSION 3.20)
project(merger)
set(CMAKE_CXX_STANDARD 20)

find_package(OpenGL REQUIRED)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")

set(ASSIMP_BUILD_TESTS OFF BOOL "No ASSIMP Tests")
set(ASSIMP_INSTALL OFF CACHE BOOL "No ASSIMP Install")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "ASSIMP Warnings as Warnings")
set(ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE BOOL "No ASSIMP view")

set(BUILD_WITH_WARNINGS OFF CACHE BOOL "" FORCE)         
set(ENABLE_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)    
set(ENABLE_WARNINGS ON CACHE BOOL "" FORCE)

include_directories(merger "${CMAKE_SOURCE_DIR}/vendor/glad/include" "${CMAKE_SOURCE_DIR}/vendor/ImGuiFileDialog" "${CMAKE_SOURCE_DIR}/vendor/glm" "${CMAKE_SOURCE_DIR}/vendor/asio-1.36.0/include" "${CMAKE_SOURCE_DIR}/vendor/imgui" "${CMAKE_SOURCE_DIR}/vendor/imgui/backends" "${CMAKE_SOURCE_DIR}/vendor/glfw/include" "${CMAKE_SOURCE_DIR}/vendor/ImGuizmo" "${CMAKE_SOURCE_DIR}/vendor/libdatachannel/include")
include_directories("${SRC_DIR}/client")
include_directories("${SRC_DIR}/merger")
include_directories("${SRC_DIR}/server")
include_directories("${CMAKE_SOURCE_DIR}/vendor/libdatachannel/deps/json/include")

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/assimp)
add_subdirectory(vendor/libdatachannel)

file(GLOB_RECURSE SRC_CXX_FILES "${SRC_DIR}/merger/*.cpp")
file(GLOB_RECURSE SRC_C_FILES "${SRC_DIR}/merger/*.c")

file(GLOB_RECURSE SRC_SERVER_FILES "${SRC_DIR}/server/*.cpp")
file(GLOB_RECURSE SRC_CLIENT_FILES "${SRC_DIR}/client/*.cpp")

set(IMGUI_SRC_DIR "${CMAKE_SOURCE_DIR}/vendor/imgui")
file(GLOB SRC_IMGUI_FILES "${IMGUI_SRC_DIR}/*.cpp")
list(APPEND SRC_IMGUI_FILES "${IMGUI_SRC_DIR}/backends/imgui_impl_glfw.cpp" "${IMGUI_SRC_DIR}/backends/imgui_impl_opengl3.cpp" "${CMAKE_SOURCE_DIR}/vendor/ImGuizmo/ImGuizmo.cpp")
list(APPEND SRC_IMGUI_FILES "${CMAKE_SOURCE_DIR}/vendor/ImGuiFileDialog/ImGuiFileDialog.cpp")
add_definitions(-DUSE_IMGUI_API -DIMGUI_DEFINE_MATH_OPERATORS)

add_executable(merger ${SRC_CXX_FILES} ${SRC_C_FILES} ${SRC_IMGUI_FILES})

target_include_directories(merger PRIVATE ${IMGUI_SRC_DIR})

if(WIN32)
    target_link_libraries(merger ${OPENGL_LIBRARIES} glfw ws2_32 assimp datachannel)
else()
    target_link_libraries(merger ${OPENGL_LIBRARIES} glfw assimp datachannel)
endif()