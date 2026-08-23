cmake_minimum_required(VERSION 3.16)
project(first_raylib_game CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep the executable next to the assets it loads at runtime.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Use a system raylib when one is installed, otherwise fetch and build it.
find_package(raylib 5.0 QUIET)
if(NOT raylib_FOUND)
    message(STATUS "raylib not found locally - fetching from GitHub")
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(WIN32)
    # raylib uses the multimedia timer and winsock symbols on Windows.
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
endif()

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Headless checks for the simulation. Links raylib for its types but never
# opens a window, so it can run in a terminal or in CI.
add_executable(${PROJECT_NAME}_tests tests/tests.cpp)
target_include_directories(${PROJECT_NAME}_tests PRIVATE src)
target_link_libraries(${PROJECT_NAME}_tests PRIVATE raylib)
if(WIN32)
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE winmm)
endif()
if(APPLE)
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE
        "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

enable_testing()
add_test(NAME simulation COMMAND ${PROJECT_NAME}_tests)

# Make assets/ available beside the binary so relative load paths work.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets)
