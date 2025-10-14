cmake_minimum_required(VERSION 3.5)

project(SimParamEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.5.1 REQUIRED COMPONENTS Widgets)

add_executable(SimParamEditor
    src/main.cpp
    src/main_widget.cpp
    src/main_widget.h
    src/param_types.h
    src/json_io.cpp
    src/json_io.h
    src/validation.cpp
    src/validation.h
    src/generator/cpp_generator.cpp
    src/generator/cpp_generator.h
    resources/styles/app.qss
)

target_link_libraries(SimParamEditor PRIVATE Qt5::Widgets)


