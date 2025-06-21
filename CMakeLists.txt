cmake_minimum_required(VERSION 3.5)

project(ImageProcessor)

set(CMAKE_AUTOMOC ON)  # 自动生成 MOC 文件
set(CMAKE_AUTORCC ON)  # 自动处理资源文件
set(CMAKE_AUTOUIC ON)  # 自动处理 UI 文件

find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui)

add_executable(ImageProcessor main.cpp)

target_link_libraries(ImageProcessor Qt5::Widgets Qt5::Core Qt5::Gui)