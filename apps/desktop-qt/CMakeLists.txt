cmake_minimum_required(VERSION 3.16)
project(desktop-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 自动定位到您电脑上的最新版 Qt 6.11.0 MinGW 编译器
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/mingw_64")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(desktop_companion_qt
    src/main.cpp
    src/PetWidget.cpp
    src/PetWidget.h
)

target_link_libraries(desktop_companion_qt PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network dwmapi)
