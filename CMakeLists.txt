cmake_minimum_required(VERSION 4.0)
project(pstu_doc_client)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt5 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

add_executable(pstu_doc_client main.cpp)
target_link_libraries(pstu_doc_client
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
)

