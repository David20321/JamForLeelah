CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)
PROJECT(ogg)

CreateLibrary(${PROJECT_NAME}
DIRS
    src
INCLUDES
    PUBLIC
       include
PROPERTIES
    FOLDER "3rd Party"
)

# vim: ts=4 et
