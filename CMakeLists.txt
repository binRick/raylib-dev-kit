cmake_minimum_required(VERSION 3.20)
project(kit_dev_example C)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "" FORCE)
    set(GRAPHICS GRAPHICS_API_OPENGL_ES3 CACHE STRING "" FORCE)  # WebGL 2
endif()
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

# The library is the single header in the repo root; this builds the example around it.
add_executable(demo example/main.c)
target_include_directories(demo PRIVATE ${CMAKE_SOURCE_DIR})
# The example loads "assets/..." relative to example/: bundled into the page for the browser,
# found by changing directory on the desktop.
if(NOT EMSCRIPTEN)
    target_compile_definitions(demo PRIVATE EXAMPLE_DIR="${CMAKE_SOURCE_DIR}/example")
endif()
target_link_libraries(demo PRIVATE raylib)
target_compile_options(demo PRIVATE -Wall -Wextra -Wno-unused-parameter)
if(NOT WIN32)
    target_link_libraries(demo PRIVATE m)
endif()
if(EMSCRIPTEN)
    set_target_properties(demo PROPERTIES OUTPUT_NAME "index" SUFFIX ".html"
        LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/shell.html")
    target_link_options(demo PRIVATE
        -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/example/assets@assets"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html")
endif()
