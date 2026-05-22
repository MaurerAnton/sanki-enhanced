#include "tui_app.h"
#include "tui_utils.h"
#include "global.h"
#include "mainMenu/sessions/sessionStruct.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <ftxui/component/screen_interactive.hpp>

namespace t = ftxui;

TuiApp::TuiApp()
{
    initDirectories();
    menuScreen.init(this);
    sessionCreateScreen.init(this);
    studyScreen.init(this);
    cardBrowserScreen.init(this);
    cardEditorScreen.init(this);
    settingsScreen.init(this);
    current_component = menuScreen.getComponent();
}

TuiApp::~TuiApp() {}

void TuiApp::initDirectories()
{
    pc = true;
    warningsEnabled = false;
    directories::config = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                          + QDir::separator() + "sanki";
    directories::deckStorage = directories::config.path() + QDir::separator() + "decks";
    directories::sessionSaves = directories::config.path() + QDir::separator() + "sessions";
    directories::fileSelect = QDir::homePath();
    directories::globalSettings.setFileName(directories::config.filePath("sanki.ini"));
    createDir(directories::config.absolutePath());
    createDir(directories::deckStorage.absolutePath());
    createDir(directories::sessionSaves.absolutePath());
}

void TuiApp::navigateTo(TuiScreen screen)
{
    previousScreen = currentScreen;
    currentScreen = screen;
    switch (screen) {
    case TuiScreen::MENU: menuScreen.refresh(); current_component = menuScreen.getComponent(); break;
    case TuiScreen::SESSION_CREATE: sessionCreateScreen.reset(); current_component = sessionCreateScreen.getComponent(); break;
    case TuiScreen::STUDY: current_component = studyScreen.getComponent(); break;
    case TuiScreen::CARD_BROWSER: current_component = cardBrowserScreen.getComponent(); break;
    case TuiScreen::CARD_EDITOR: current_component = cardEditorScreen.getComponent(); break;
    case TuiScreen::SETTINGS: current_component = settingsScreen.getComponent(); break;
    case TuiScreen::QUIT: break;
    }
}

void TuiApp::startStudy(sessionStr session)
{
    activeSession = session;
    studyScreen.startSession(session);
    navigateTo(TuiScreen::STUDY);
}

void TuiApp::browseCards(sessionStr session)
{
    activeSession = session;
    cardBrowserScreen.startSession(session);
    navigateTo(TuiScreen::CARD_BROWSER);
}

void TuiApp::editCard(sessionStr session, int cardIndex)
{
    activeSession = session;
    activeCardIndex = cardIndex;
    cardEditorScreen.setCard(session, cardIndex);
    navigateTo(TuiScreen::CARD_EDITOR);
}

void TuiApp::goBack() { navigateTo(previousScreen); }

void TuiApp::run()
{
    auto screen = t::ScreenInteractive::Fullscreen();
    screen.Loop(current_component);
}
