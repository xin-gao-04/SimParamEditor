cmake_minimum_required(VERSION 3.5)

project(SimParamEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出编译命令，供 VSCode 等 IDE 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(WIN32)
    find_package(Qt5 5.5.1 REQUIRED COMPONENTS Widgets)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
endif()

add_executable(SimParamEditor
    src/app/main.cpp
    src/ui/main_window/main_widget.cpp
    src/ui/main_window/main_widget_property.cpp
    src/ui/main_window/main_widget_tree.cpp
    src/ui/main_window/main_widget_instances.cpp
    src/ui/main_window/main_widget_project.cpp
    src/ui/main_window/main_widget_validation.cpp
    src/ui/main_window/main_widget.h
    src/ui/dialogs/instances_select_dialog.cpp
    src/ui/dialogs/instances_select_dialog.h
    src/ui/theme_manager.cpp
    src/ui/theme_manager.h
    src/core/param_types.h
    src/core/json_io.cpp
    src/core/json_io.h
    src/core/validation.cpp
    src/core/validation.h
    src/core/type_manager.cpp
    src/core/type_manager.h
    src/generator/cpp_generator.cpp
    src/generator/cpp_generator.h
    resources/styles/app.qss
)

target_link_libraries(SimParamEditor PRIVATE Qt5::Widgets)

target_include_directories(SimParamEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

