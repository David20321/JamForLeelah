cmake_minimum_required(VERSION 2.8.12)
Project(glew)

CreateLibrary(${PROJECT_NAME}
DIRS
    src
    include/GL
INCLUDES
    PUBLIC include
DEFINES
    PUBLIC GLEW_STATIC GLEW_NO_GLU
PROPERTIES
    FOLDER "3rd Party"
)

source_group("Source" REGULAR_EXPRESSION ".*\\.(c|h)")
