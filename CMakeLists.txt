cmake_minimum_required(VERSION 3.16)
project(Statio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(BUILD_QT_GUI "Build Statio Qt GUI" ON)

add_executable(statio
    src/main.cpp
    src/system_info.cpp
)

target_include_directories(statio PRIVATE include)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(statio PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (BUILD_QT_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC OFF)
    set(CMAKE_AUTORCC OFF)

    find_package(Qt6 COMPONENTS Widgets QUIET)
    if (Qt6_FOUND)
        set(STATIO_QT_LIB Qt6::Widgets)
    else()
        find_package(Qt5 COMPONENTS Widgets QUIET)
        if (Qt5_FOUND)
            set(STATIO_QT_LIB Qt5::Widgets)
        endif()
    endif()

    if (STATIO_QT_LIB)
        add_executable(statio-qt
            src/main_qt.cpp
            src/main_window.cpp
            src/system_info.cpp
            include/statio/main_window.hpp
        )
        target_include_directories(statio-qt PRIVATE include)
        target_link_libraries(statio-qt PRIVATE ${STATIO_QT_LIB})
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(statio-qt PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    else()
        message(WARNING "Qt5/Qt6 Widgets not found: statio-qt target will not be built")
    endif()
endif()
