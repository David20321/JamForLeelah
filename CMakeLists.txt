cmake_minimum_required(VERSION 2.8.12)
Project(UnderGlass)

# Force Clang/LLVM C++ library
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.7")

# force 32bit
set(FORCE32 ON CACHE BOOL "" FORCE)

### pull in boilerplate cmake
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BoilerPlate)
include(UtilityFunctions)

list(APPEND CMAKE_PREFIX_PATH
    lib/SDL2
    lib/FBX_SDK_2015.1
)

### Find Libraries
find_package(OpenGL)
find_package(SDL2 CONFIG)
find_package(FBXSDK CONFIG)

add_subdirectory(lib/glew glew)

CreateProgram(${PROJECT_NAME}
DIRS
    src
    src/fbx/**
    src/internal
    src/game
    src/platform_sdl
DEFINES
    $<$<BOOL:${WIN32}>:_CRT_SECURE_NO_WARNINGS>
INCLUDES
    src

    lib/glm/
    lib/stb-master
    ${SDL2_INCLUDE_DIRS}
    ${FBXSDK_INCLUDE_DIRS}
LINK
    ${SDL2_LIBRARIES}
    glew
    ${OPENGL_gl_LIBRARY}
    ${FBXSDK_LIBRARIES}
)

CopyDependentLibs(${PROJECT_NAME})