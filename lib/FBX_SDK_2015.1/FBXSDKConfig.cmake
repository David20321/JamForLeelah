if(APPLE)
    set(suffix "lib/osx")
elseif(WIN32)
    set(suffix "lib/vs2012/x86")
elseif(CMAKE_SYSTEM MATCHES "Linux")
    set(suffix "lib/linux")
endif()

find_library(FBXSDK_LIBRARY_DEBUG
    NAMES fbxsdk libfbxsdk-md
    PATHS "${CMAKE_CURRENT_LIST_DIR}/${suffix}/debug/lib" "${CMAKE_CURRENT_LIST_DIR}/${suffix}/debug"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
find_library(FBXSDK_LIBRARY_RELEASE
    NAMES fbxsdk libfbxsdk-md
    PATHS "${CMAKE_CURRENT_LIST_DIR}/${suffix}/release/lib" "${CMAKE_CURRENT_LIST_DIR}/${suffix}/release"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
find_path(FBXSDK_INCLUDE_DIR
    NAMES fbxsdk.h
    PATHS "${CMAKE_CURRENT_LIST_DIR}/include"
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)
set(FBXSDK_INCLUDE_DIRS ${FBXSDK_INCLUDE_DIR})

include(SelectLibraryConfigurations)
select_library_configurations( FBXSDK )

set(FBXSDK_LIBRARIES ${FBXSDK_LIBRARY})

mark_as_advanced(FBXSDK_INCLUDE_DIR FBXSDK_LIBRARY_DEBUG FBXSDK_LIBRARY_RELEASE)
