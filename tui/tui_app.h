#ifndef TUI_APP_H
#define TUI_APP_H

#include "tui_menu.h"
#include "tui_study.h"
#include "tui_session_create.h"
#include "tui_card_browser.h"
#include "tui_card_editor.h"
#include "tui_settings.h"

#include <QObject>
#include <QSettings>
#include <QDir>
#include <ftxui/component/component.hpp>

enum class TuiScreen { MENU, SESSION_CREATE, STUDY, CARD_BROWSER, CARD_EDITOR, SETTINGS, QUIT };

class TuiApp {
public:
    TuiApp();
    ~TuiApp();
    void run();
    void initDirectories();
    void navigateTo(TuiScreen screen);
    void startStudy(sessionStr session);
    void browseCards(sessionStr session);
    void editCard(sessionStr session, int cardIndex);
    void goBack();

    TuiScreen currentScreen = TuiScreen::MENU;
    TuiScreen previousScreen = TuiScreen::MENU;
    sessionStr activeSession;
    int activeCardIndex = -1;

    TuiMenu menuScreen;
    TuiStudy studyScreen;
    TuiSessionCreate sessionCreateScreen;
    TuiCardBrowser cardBrowserScreen;
    TuiCardEditor cardEditorScreen;
    TuiSettings settingsScreen;

    ftxui::Component current_component;
};

#endif // TUI_APP_H
