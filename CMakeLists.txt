cmake_minimum_required(VERSION 3.31.6)
project(ChaosEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar dependencias en el sistema (Ubuntu)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Box2D no siempre tiene archivo .cmake, lo buscamos manualmente
find_path(BOX2D_INCLUDE_DIR NAMES box2d/box2d.h)
find_library(BOX2D_LIBRARY NAMES box2d)

if(NOT BOX2D_INCLUDE_DIR OR NOT BOX2D_LIBRARY)
    message(FATAL_ERROR "Box2D no encontrado. Instalá libbox2d-dev pelotudo.")
endif()

include_directories(${SFML_INCLUDE_DIR} ${BOX2D_INCLUDE_DIR} src/Core src/Physics src/Graphics src/Utils src/Gen)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(ChaosEngine ${SOURCES})

target_link_libraries(ChaosEngine sfml-graphics sfml-window sfml-system ${BOX2D_LIBRARY})