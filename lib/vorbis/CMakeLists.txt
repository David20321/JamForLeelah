CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)
PROJECT(vorbis)

CreateLibrary(${PROJECT_NAME}
DIRS
    lib
EXCLUDE
    lib/analysis.c
    lib/barkmel.c
    lib/lookup*
    lib/psytune.c
    lib/tone.c
    lib/vorbisenc.c
INCLUDES
    PRIVATE
        lib
    PUBLIC
        include
LINK
    ogg
PROPERTIES
    FOLDER "3rd Party"
)

# vim: ts=4 et
