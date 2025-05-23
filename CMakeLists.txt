cmake_minimum_required(VERSION 3.20)
project(MyFirstCppGame VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable more verbose output
set(CMAKE_VERBOSE_MAKEFILE ON)

# Set BGFX config flags
add_definitions(
    -DBX_CONFIG_DEBUG=0
)

# Paths
set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdl_src/SDL-release-3.2.14/build")
set(BGFX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")

# Include SDL3 and BGFX
list(APPEND CMAKE_PREFIX_PATH "${SDL3_DIR}")

# Find SDL3 package
find_package(SDL3 REQUIRED)

# Output SDL3 information for debugging
message(STATUS "SDL3_FOUND: ${SDL3_FOUND}")
message(STATUS "SDL3_INCLUDE_DIRS: ${SDL3_INCLUDE_DIRS}")
message(STATUS "SDL3_LIBRARIES: ${SDL3_LIBRARIES}")

# Source files
set(SOURCES 
    src/main.cpp
    src/model.cpp
    src/skills.cpp
    src/resources.cpp
    src/npcs.cpp
    src/player.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${SDL3_INCLUDE_DIRS}
    ${BGFX_DIR}/bx/include
    ${BGFX_DIR}/bimg/include
    ${BGFX_DIR}/bgfx/include
    ${BGFX_DIR}/bx/include/compat/osx
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/tinygltf
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${SDL3_LIBRARIES}
    ${BGFX_DIR}/bgfx/.build/osx-arm64/bin/libbgfxRelease.a
    ${BGFX_DIR}/bgfx/.build/osx-arm64/bin/libbxRelease.a
    ${BGFX_DIR}/bgfx/.build/osx-arm64/bin/libbimgRelease.a
)

# Add framework dependencies for macOS
if(APPLE)
    # Set runtime search paths
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "${SDL3_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    
    # Add macOS frameworks
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

# Copy shader files to the build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/shaders/)

# Copy texture assets to the build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/assets/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/assets/)