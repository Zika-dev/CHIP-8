# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(CHIP-8 VERSION 1.0)

# Add executable target
add_executable(CHIP-8 main.c)

# Find SDL2
find_package(SDL2 REQUIRED)

# Include SDL2 headers
target_include_directories(CHIP-8 PRIVATE ${SDL2_INCLUDE_DIRS})

# Link against SDL2 libraries
target_link_libraries(CHIP-8 PRIVATE ${SDL2_LIBRARIES} m)