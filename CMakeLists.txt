cmake_minimum_required(VERSION 2.8.12)
Project(UnderGlass)

### pull in boilerplate cmake
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BoilerPlate)
include(UtilityFunctions)

### Find Libraries
find_package(OpenGL)

## Setup libs

link_directories(
	lib/SDL2-2.0.3/lib/x86
	lib/glew-1.12.0/lib/Release/Win32
	lib/FBX_SDK_2015.1/lib/vs2012/x86/debug/
)

if(WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS")
endif()

CreateProgram(${PROJECT_NAME}
INCLUDES
	lib/SDL2-2.0.3/include
	lib/glew-1.12.0/include
	lib/glm/
	lib/stb-master
	lib/FBX_SDK_2015.1/include/
	src/
FILES
    src/main.cpp
    src/fbx/fbx.h
    src/fbx/fbx.cpp
    src/fbx/ImportScene/DisplayAnimation.cxx
    src/fbx/ImportScene/DisplayAnimation.h
    src/fbx/ImportScene/DisplayCache.cxx
    src/fbx/ImportScene/DisplayCache.h
    src/fbx/ImportScene/DisplayCamera.cxx
    src/fbx/ImportScene/DisplayCamera.h
    src/fbx/ImportScene/DisplayCommon.cxx
    src/fbx/ImportScene/DisplayCommon.h
    src/fbx/ImportScene/DisplayGenericInfo.cxx
    src/fbx/ImportScene/DisplayGenericInfo.h
    src/fbx/ImportScene/DisplayGlobalSettings.cxx
    src/fbx/ImportScene/DisplayGlobalSettings.h
    src/fbx/ImportScene/DisplayHierarchy.cxx
    src/fbx/ImportScene/DisplayHierarchy.h
    src/fbx/ImportScene/DisplayLight.cxx
    src/fbx/ImportScene/DisplayLight.h
    src/fbx/ImportScene/DisplayLink.cxx
    src/fbx/ImportScene/DisplayLink.h
    src/fbx/ImportScene/DisplayLodGroup.cxx
    src/fbx/ImportScene/DisplayLodGroup.h
    src/fbx/ImportScene/DisplayMarker.cxx
    src/fbx/ImportScene/DisplayMarker.h
    src/fbx/ImportScene/DisplayMaterial.cxx
    src/fbx/ImportScene/DisplayMaterial.h
    src/fbx/ImportScene/DisplayMesh.cxx
    src/fbx/ImportScene/DisplayMesh.h
    src/fbx/ImportScene/DisplayNurb.cxx
    src/fbx/ImportScene/DisplayNurb.h
    src/fbx/ImportScene/DisplayPatch.cxx
    src/fbx/ImportScene/DisplayPatch.h
    src/fbx/ImportScene/DisplayPivotsAndLimits.cxx
    src/fbx/ImportScene/DisplayPivotsAndLimits.h
    src/fbx/ImportScene/DisplayPose.cxx
    src/fbx/ImportScene/DisplayPose.h
    src/fbx/ImportScene/DisplayShape.cxx
    src/fbx/ImportScene/DisplayShape.h
    src/fbx/ImportScene/DisplaySkeleton.cxx
    src/fbx/ImportScene/DisplaySkeleton.h
    src/fbx/ImportScene/DisplayTexture.cxx
    src/fbx/ImportScene/DisplayTexture.h
    src/fbx/ImportScene/DisplayUserProperties.cxx
	src/fbx/ImportScene/DisplayUserProperties.h
    src/internal/memory.h
    src/internal/memory.cpp
    src/internal/common.h
    src/internal/common.cpp
    src/platform_sdl/audio.h
    src/platform_sdl/audio.cpp
    src/platform_sdl/graphics.h
    src/platform_sdl/graphics.cpp
    src/platform_sdl/error.h
    src/platform_sdl/error.cpp
    src/platform_sdl/file_io.h
    src/platform_sdl/file_io.cpp
LINK
LINK
	SDL2
	SDL2main
	glew32
	opengl32
	libfbxsdk-md
COPY_FILES
	lib/SDL2-2.0.3/lib/x86/SDL2.dll
	lib/glew-1.12.0/bin/Release/Win32/glew32.dll
	lib/FBX_SDK_2015.1/lib/vs2012/x86/debug/libfbxsdk.dll
)
