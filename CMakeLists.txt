cmake_minimum_required(VERSION 3.21)
project(Orbit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# Modular source definitions
set(APP_SOURCES
    src/app/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
)

set(EDITOR_SOURCES
    src/editor/CodeEditor.h
    src/editor/CodeEditor.cpp
    src/editor/LineNumberArea.h
    src/editor/LineNumberArea.cpp
    src/editor/SyntaxHighlighter.h
    src/editor/SyntaxHighlighter.cpp
)

set(EXPLORER_SOURCES
    src/explorer/ExplorerPanel.h
    src/explorer/ExplorerPanel.cpp
)

set(UI_SOURCES
    src/ui/Icons.h
    src/ui/Icons.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
)

set(RESOURCES
    resources/resources.qrc
)

set(PROJECT_SOURCES
    ${APP_SOURCES}
    ${EDITOR_SOURCES}
    ${EXPLORER_SOURCES}
    ${UI_SOURCES}
    ${RESOURCES}
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
        src/app/MainWindow.h
        src/app/MainWindow.cpp
        src/explorer/ExplorerPanel.h
        src/explorer/ExplorerPanel.cpp
        src/editor/CodeEditor.h
        src/editor/CodeEditor.cpp
        src/editor/SyntaxHighlighter.h
        src/editor/SyntaxHighlighter.cpp
        src/editor/LineNumberArea.h
        src/editor/LineNumberArea.cpp
        src/ui/Icons.h
        src/ui/Icons.cpp
        src/ui/Theme.h
        src/ui/Theme.cpp
        ${RESOURCES}
    )
    target_include_directories(orbit_tests PRIVATE src)
    target_link_libraries(orbit_tests PRIVATE
        Qt6::Widgets
        Qt6::Test
    )
    add_test(NAME OrbitTests COMMAND orbit_tests)
endif()
