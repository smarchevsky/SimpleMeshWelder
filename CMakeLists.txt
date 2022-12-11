cmake_minimum_required(VERSION 3.5)

project(MeshWeld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MeshWeld main.cpp)

target_link_libraries(${PROJECT_NAME} assimp)
