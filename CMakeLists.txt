cmake_minimum_required(VERSION 3.25)

project(TankPhysicsSandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(TankSandbox
    Source/TankSandbox/Main.cpp
)

target_compile_features(TankSandbox PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(TankSandbox PRIVATE /W4 /permissive-)
endif()
