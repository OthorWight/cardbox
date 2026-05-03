cmake_minimum_required(VERSION 3.10)
project(ImGuiSolitaire C CXX)

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Lua and Sol2
include(FetchContent)
FetchContent_Declare(
    lua
    URL https://www.lua.org/ftp/lua-5.3.6.tar.gz
)
FetchContent_MakeAvailable(lua)
FetchContent_GetProperties(lua)
if(NOT TARGET lua_static AND lua_POPULATED)
    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
    list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/src/lua.c" "${lua_SOURCE_DIR}/src/luac.c")
    add_library(lua_static STATIC ${LUA_SOURCES})
    target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}/src")
    if(UNIX AND NOT APPLE)
        target_compile_definitions(lua_static PRIVATE LUA_USE_LINUX)
    endif()
endif()

FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.1
)
set(SOL2_BUILD_LUA OFF CACHE BOOL "Build Lua with sol2" FORCE)
FetchContent_MakeAvailable(sol2)

# stb_image
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# Download and extract playing cards assets
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/cards")
    FetchContent_Declare(
        playing_cards_assets
        URL https://github.com/hayeah/playing-cards-assets/archive/refs/heads/master.zip
    )
    FetchContent_MakeAvailable(playing_cards_assets)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${playing_cards_assets_SOURCE_DIR}/png ${CMAKE_CURRENT_BINARY_DIR}/cards)
endif()

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ImGui sources
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

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
    ${glfw_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_link_libraries(solitaire PRIVATE
    glfw
    OpenGL::GL
    sol2::sol2
    lua_static
)

if (UNIX AND NOT APPLE)
    target_link_libraries(solitaire PRIVATE dl pthread)
endif()

# Copy Lua scripts to a rules/ folder next to the executable
file(GLOB LUA_SCRIPTS "src/*.lua")
add_custom_command(TARGET solitaire POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:solitaire>/rules
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUA_SCRIPTS} $<TARGET_FILE_DIR:solitaire>/rules/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/cards $<TARGET_FILE_DIR:solitaire>/cards
)
