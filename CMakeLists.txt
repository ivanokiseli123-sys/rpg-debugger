cmake_minimum_required(VERSION 3.24)
project(RPGDebugger LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WIN32_EXECUTABLE ON)

include(FetchContent)

FetchContent_Declare(
  lua
  GIT_REPOSITORY https://github.com/lua/lua.git
  GIT_TAG v5.4.7
)
FetchContent_MakeAvailable(lua)

file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/lua.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/luac.c$")
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}")
target_compile_definitions(lua_static PRIVATE LUA_USE_WINDOWS _CRT_SECURE_NO_WARNINGS)

add_executable(RPGDebugger WIN32 src/main.cpp)
target_include_directories(RPGDebugger PRIVATE "${lua_SOURCE_DIR}")
target_link_libraries(RPGDebugger PRIVATE lua_static psapi comctl32)
target_compile_definitions(RPGDebugger PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS NOMINMAX)

if(MSVC)
  target_compile_options(RPGDebugger PRIVATE /W4 /permissive-)
else()
  target_compile_options(RPGDebugger PRIVATE -Wall -Wextra)
endif()

install(TARGETS RPGDebugger RUNTIME DESTINATION .)
