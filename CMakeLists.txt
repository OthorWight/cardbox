cmake_minimum_required(VERSION 3.10)
project(cardbox C CXX)

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

# Share the engine with the application and headless regression tests.
add_library(cardbox_engine STATIC src/Game.cpp src/ParticleSystem.cpp ${IMGUI_SOURCES})
# Keep Lua argument and numeric checks enabled in Release builds too.
target_compile_definitions(cardbox_engine PUBLIC SOL_ALL_SAFETIES_ON=1)
add_executable(cardbox src/main.cpp)
target_link_libraries(cardbox PRIVATE cardbox_engine)

target_include_directories(cardbox_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${glfw_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_link_libraries(cardbox_engine PUBLIC
    glfw
    OpenGL::GL
    sol2::sol2
    lua_static
)

if (UNIX AND NOT APPLE)
    target_link_libraries(cardbox_engine PUBLIC dl pthread)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(cardbox_tests tests/GameTests.cpp)
    target_link_libraries(cardbox_tests PRIVATE cardbox_engine)
    add_test(NAME cardbox_engine COMMAND cardbox_tests)
    set_tests_properties(cardbox_engine PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(particle_tests tests/ParticleTests.cpp)
    target_link_libraries(particle_tests PRIVATE cardbox_engine)
    add_test(NAME cardbox_particles COMMAND particle_tests)
    add_executable(dungeon_tests tests/DungeonTests.cpp)
    target_link_libraries(dungeon_tests PRIVATE cardbox_engine)
    add_test(NAME cardbox_dungeon COMMAND dungeon_tests)
    set_tests_properties(cardbox_dungeon PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# Copy Lua scripts to a rules/ folder next to the executable
file(GLOB LUA_SCRIPTS CONFIGURE_DEPENDS "src/*.lua")
add_custom_command(TARGET cardbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardbox>/rules
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUA_SCRIPTS} $<TARGET_FILE_DIR:cardbox>/rules/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/cards $<TARGET_FILE_DIR:cardbox>/cards
)

# Lua-only edits do not relink the executable, so refresh rules on every normal
# build as well. Run after cardbox to avoid racing its initial asset copy.
add_custom_target(cardbox_rules ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:cardbox>/rules
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUA_SCRIPTS} $<TARGET_FILE_DIR:cardbox>/rules/
    DEPENDS ${LUA_SCRIPTS}
)
add_dependencies(cardbox_rules cardbox)
