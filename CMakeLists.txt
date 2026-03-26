cmake_minimum_required(VERSION 3.16)
project(ChaosEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- SFML 3 ---
FetchContent_Declare(
  SFML
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.2
)
FetchContent_MakeAvailable(SFML)

# --- Box2D ---
find_path(BOX2D_INCLUDE_DIR NAMES box2d/box2d.h)
find_library(BOX2D_LIBRARY NAMES box2d)
if(NOT BOX2D_INCLUDE_DIR OR NOT BOX2D_LIBRARY)
    message(FATAL_ERROR "Box2D no encontrado. Instalá libbox2d-dev.")
endif()

# --- ImGui ---
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG        v1.91.8
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SFML_FIND_SFML OFF)

# --- ImGui-SFML ---
FetchContent_Declare(
  imgui-sfml
  GIT_REPOSITORY https://github.com/SFML/imgui-sfml
  GIT_TAG        master
)
FetchContent_MakeAvailable(imgui-sfml)

# --- Sources ---
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(ChaosEngine ${SOURCES})

target_include_directories(ChaosEngine PRIVATE
    ${BOX2D_INCLUDE_DIR}
    src/Core src/Physics src/Graphics src/Utils src/Gen
)

target_link_libraries(ChaosEngine PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    ${BOX2D_LIBRARY}
    ImGui-SFML::ImGui-SFML
)
