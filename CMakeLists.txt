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
        Network
        REQUIRED)

add_executable(pstu_doc_client
        main.cpp
        api_client.h
        api_client.cpp
        dialogs/authdialog.h
        dialogs/authdialog.cpp
        widgets/chatwidget.h
        widgets/chatwidget.cpp
        mainwindow.h
        mainwindow.cpp
        widgets/collapsiblepanel.h
        widgets/collapsiblepanel.cpp
        widgets/personswidget.h
        widgets/personswidget.cpp
        dialogs/persondialog.h
        dialogs/persondialog.cpp
        dialogs/sharedialog.h
        dialogs/sharedialog.cpp)
target_link_libraries(pstu_doc_client
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Network
)

add_custom_target(update_style ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/style.qss
        ${CMAKE_BINARY_DIR}/style.qss
    COMMENT "Updating style.qss"
)
add_dependencies(pstu_doc_client update_style)

