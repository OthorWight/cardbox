cmake_minimum_required(VERSION 3.10)
project(ImGuiSolitaire C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/glfw)

# ImGui sources
set(IMGUI_DIR extern/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Application sources
set(APP_SOURCES
    src/main.cpp
    src/Game.cpp
)

add_executable(solitaire ${APP_SOURCES} ${IMGUI_SOURCES})

target_include_directories(solitaire PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    extern/glfw/include
)

target_link_libraries(solitaire PRIVATE
    glfw
    OpenGL::GL
)

if (UNIX AND NOT APPLE)
    target_link_libraries(solitaire PRIVATE dl pthread)
endif()
