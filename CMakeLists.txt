cmake_minimum_required(VERSION 3.21)
project(Orbit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ExplorerPanel.h
    src/ExplorerPanel.cpp
    src/CodeEditor.h
    src/CodeEditor.cpp
    src/LineNumberArea.h
    src/LineNumberArea.cpp
    src/Icons.h
    src/Icons.cpp
    src/Theme.h
    src/Theme.cpp
    resources/resources.qrc
)

qt_add_executable(orbit ${PROJECT_SOURCES})

target_link_libraries(orbit PRIVATE
    Qt6::Widgets
)
target_include_directories(orbit PRIVATE src)

set_target_properties(orbit PROPERTIES
    OUTPUT_NAME "orbit"
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)

# Optional automated tests
find_package(Qt6 OPTIONAL_COMPONENTS Test)
if(Qt6Test_FOUND)
    enable_testing()
    qt_add_executable(orbit_tests
        tests/test_editor.cpp
        src/MainWindow.h
        src/MainWindow.cpp
        src/ExplorerPanel.h
        src/ExplorerPanel.cpp
        src/CodeEditor.h
        src/CodeEditor.cpp
        src/LineNumberArea.h
        src/LineNumberArea.cpp
        src/Icons.h
        src/Icons.cpp
        src/Theme.h
        src/Theme.cpp
        resources/resources.qrc
    )
    target_include_directories(orbit_tests PRIVATE src)
    target_link_libraries(orbit_tests PRIVATE
        Qt6::Widgets
        Qt6::Test
    )
    add_test(NAME OrbitTests COMMAND orbit_tests)
endif()
