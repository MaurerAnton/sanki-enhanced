cmake_minimum_required(VERSION 3.20)
project(sanki-tui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Sql Network REQUIRED)

include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.1.0
)
FetchContent_MakeAvailable(ftxui)

set(TUI_SOURCES
    tui_main.cpp
    tui_app.cpp
    tui_menu.cpp
    tui_study.cpp
    tui_session_create.cpp
    tui_card_browser.cpp
    tui_card_editor.cpp
    tui_settings.cpp
    tui_utils.cpp
    tui_card_utils.cpp
    tui_modes.cpp
    tui_global.cpp
    ../mainMenu/sessions/sessionStruct.cpp
)

set(TUI_HEADERS
    tui_app.h
    tui_menu.h
    tui_study.h
    tui_session_create.h
    tui_card_browser.h
    tui_card_editor.h
    tui_settings.h
    tui_utils.h
    tui_card_utils.h
    tui_modes.h
    global.h
)

add_executable(sanki-tui ${TUI_SOURCES} ${TUI_HEADERS})

# Put tui/ first so our global.h shadows all/global.h
target_include_directories(sanki-tui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../all
)

target_compile_definitions(sanki-tui PRIVATE
    PC=true
    TUI=true
)

target_link_libraries(sanki-tui PRIVATE
    Qt5::Core
    Qt5::Sql
    Qt5::Network
    ftxui::screen
    ftxui::dom
    ftxui::component
)
