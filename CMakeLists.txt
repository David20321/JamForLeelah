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
)

### Find Libraries
find_package(OpenGL)
find_package(SDL2 CONFIG NO_CMAKE_FIND_ROOT_PATH)

add_subdirectory(lib/ogg ogg)
add_subdirectory(lib/vorbis vorbis)

if(NOT EMSCRIPTEN)
    add_subdirectory(lib/glew glew)
else()
    ## for emscripten the GL library is always there
    set(OPENGL_gl_LIBRARY "")
    EmscriptenCreatePackage(UnderGlass DATA_FILE
        PRELOAD ${CMAKE_CURRENT_SOURCE_DIR}/assets@assets
        ARGS --use-preload-cache --no-heap-copy
    )
endif()

CreateProgram(${PROJECT_NAME}
DIRS
    src
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
PRE_JS
    ${DATA_FILE}
ASM_FLAG
## Once we figure out the memory needed we use TOTAL_MEMORY (otherwise asm.js optimizations won't be enabled)
#    TOTAL_MEMORY=134217728
## For now we allow memory growth
    ALLOW_MEMORY_GROWTH=1
LINK
    ${SDL2_LIBRARIES}
    $<$<NOT:$<BOOL:${EMSCRIPTEN}>>:glew>
    ${OPENGL_gl_LIBRARY}
    vorbis
)

if(EMSCRIPTEN)
    set(EM_TARGET_JS ${PROJECT_NAME}.js)
    configure_file(src/shell.html index.html @ONLY)
endif()

CopyDependentLibs(${PROJECT_NAME})