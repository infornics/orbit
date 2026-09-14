cmake_minimum_required(VERSION 3.21)
project(Orbit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
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

set(AGENT_SOURCES
    src/agent/AcpClient.h
    src/agent/AcpClient.cpp
    src/agent/AntigravityInstaller.h
    src/agent/AntigravityInstaller.cpp
    src/agent/AntigravityPanel.h
    src/agent/AntigravityPanel.cpp
    src/agent/AgentTypes.h
    src/agent/AgentThread.h
    src/agent/AgentThread.cpp
    src/agent/AgentTerminalHost.h
    src/agent/AgentTerminalHost.cpp
    src/agent/AgentComposer.h
    src/agent/AgentComposer.cpp
    src/agent/AgentTranscriptView.h
    src/agent/AgentTranscriptView.cpp
)

set(TERMINAL_SOURCES
    src/terminal/PtyProcess.h
    src/terminal/PtyProcess.cpp
    src/terminal/TerminalWidget.h
    src/terminal/TerminalWidget.cpp
    src/terminal/TerminalPanel.h
    src/terminal/TerminalPanel.cpp
)

set(RESOURCES
    resources/resources.qrc
)

set(PROJECT_SOURCES
    ${APP_SOURCES}
    ${EDITOR_SOURCES}
    ${EXPLORER_SOURCES}
    ${UI_SOURCES}
    ${AGENT_SOURCES}
    ${TERMINAL_SOURCES}
    ${RESOURCES}
)

find_library(UTIL_LIB util)

qt_add_executable(orbit ${PROJECT_SOURCES})

target_link_libraries(orbit PRIVATE
    Qt6::Widgets
    Qt6::Network
    $<$<BOOL:${UTIL_LIB}>:${UTIL_LIB}>
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
        src/agent/AcpClient.h
        src/agent/AcpClient.cpp
        src/agent/AntigravityInstaller.h
        src/agent/AntigravityInstaller.cpp
        src/agent/AntigravityPanel.h
        src/agent/AntigravityPanel.cpp
        src/agent/AgentTypes.h
        src/agent/AgentThread.h
        src/agent/AgentThread.cpp
        src/agent/AgentTerminalHost.h
        src/agent/AgentTerminalHost.cpp
        src/agent/AgentComposer.h
        src/agent/AgentComposer.cpp
        src/agent/AgentTranscriptView.h
        src/agent/AgentTranscriptView.cpp
        ${TERMINAL_SOURCES}
        ${RESOURCES}
    )
    target_include_directories(orbit_tests PRIVATE src)
    target_link_libraries(orbit_tests PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Test
        $<$<BOOL:${UTIL_LIB}>:${UTIL_LIB}>
    )
    add_test(NAME OrbitTests COMMAND orbit_tests)

    qt_add_executable(orbit_terminal_tests
        tests/test_terminal.cpp
        ${TERMINAL_SOURCES}
        src/ui/Icons.h
        src/ui/Icons.cpp
        ${RESOURCES}
    )
    target_include_directories(orbit_terminal_tests PRIVATE src)
    target_link_libraries(orbit_terminal_tests PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Test
        $<$<BOOL:${UTIL_LIB}>:${UTIL_LIB}>
    )
    add_test(NAME OrbitTerminalTests COMMAND orbit_terminal_tests)
endif()

# ==============================================================================
# Installation & Packaging (Linux / Debian / Ubuntu .deb)
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS orbit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES resources/icons/orbit.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES resources/icons/orbit_logo.jpg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
    RENAME orbit.jpg
)

install(FILES resources/orbit.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# CPack Debian Package Settings
set(CPACK_PACKAGE_NAME "orbit")
set(CPACK_PACKAGE_VENDOR "Infornics")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast, focused C++/Qt6 code and text editor with Google Antigravity integration")
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 1)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_PACKAGE_CONTACT "Infornics <support@infornics.com>")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Infornics")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)

