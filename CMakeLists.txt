cmake_minimum_required(VERSION 3.31.6)
project(ChaosEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Buscar dependencias del sistema (Ubuntu)
find_package(SFML 2.5 COMPONENTS graphics window system audio REQUIRED)

# 2. Buscar Box2D
find_path(BOX2D_INCLUDE_DIR NAMES box2d/box2d.h)
find_library(BOX2D_LIBRARY NAMES box2d)

if(NOT BOX2D_INCLUDE_DIR OR NOT BOX2D_LIBRARY)
    message(FATAL_ERROR "Box2D no encontrado. Instalá libbox2d-dev.")
endif()

# 3. FetchContent Configuración
include(FetchContent)

# --- PASO 1: BAJAR IMGUI (SOLO) ---
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG        v1.89.9
)
FetchContent_MakeAvailable(imgui)

# --- PASO 2: CONECTAR CABLES ---
# ImGui-SFML necesita esta variable para encontrar los headers de ImGui
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# Fix para que no llore por librerías estáticas de SFML
set(IMGUI_SFML_FIND_SFML OFF)

# --- PASO 3: BAJAR IMGUI-SFML ---
FetchContent_Declare(
  imgui-sfml
  GIT_REPOSITORY https://github.com/SFML/imgui-sfml
  GIT_TAG        v2.6
)
FetchContent_MakeAvailable(imgui-sfml)

# 4. Includes
include_directories(
    ${SFML_INCLUDE_DIR} 
    ${BOX2D_INCLUDE_DIR} 
    src/Core 
    src/Physics 
    src/Graphics 
    src/Utils 
    src/Gen
)

# 5. Código Fuente
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(ChaosEngine ${SOURCES})

# 6. Linkeo
target_link_libraries(ChaosEngine 
    sfml-graphics 
    sfml-window 
    sfml-system 
    sfml-audio
    ${BOX2D_LIBRARY} 
    ImGui-SFML::ImGui-SFML
)