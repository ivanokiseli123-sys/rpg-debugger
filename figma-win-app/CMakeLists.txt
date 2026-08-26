cmake_minimum_required(VERSION 3.24)
project(NeonDebugger LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.2b)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

add_executable(NeonDebugger WIN32 src/main.cpp)
target_link_libraries(NeonDebugger PRIVATE imgui d3d11 dxgi dwmapi psapi)
target_include_directories(NeonDebugger PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# Lua 5.4 is embedded as a scripting runtime. The application deliberately does
# not expose DLL injection or arbitrary Roblox process manipulation.
FetchContent_Declare(lua GIT_REPOSITORY https://github.com/lua/lua.git GIT_TAG v5.4.7)
FetchContent_MakeAvailable(lua)
file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/*.c)
list(FILTER LUA_SOURCES EXCLUDE REGEX "/lua\.c$|/luac\.c$")
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})
target_link_libraries(NeonDebugger PRIVATE lua_static)

if(MSVC)
  target_compile_definitions(NeonDebugger PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(NeonDebugger PRIVATE /W4 /permissive-)
endif()
