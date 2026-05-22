#ifndef TUI_CARD_BROWSER_H
#define TUI_CARD_BROWSER_H

#include "mainMenu/sessions/sessionStruct.h"
#include "tui_modes.h"

#include <QSqlDatabase>
#include <ftxui/component/component.hpp>

class TuiApp;

class TuiCardBrowser {
public:
    TuiCardBrowser();
    void init(TuiApp* app);
    void startSession(sessionStr session);
    ftxui::Component getComponent();

    TuiApp* app = nullptr;

private:
    void loadCards();
    void buildComponent();

    sessionStr session;
    QList<QSqlDatabase> databases;
    int selectedIndex = 0;
    bool showBack = false;
    std::string currentFront;
    std::string currentBack;
    std::string statusText;
    std::string searchFilter;
    bool searchActive = false;

    ftxui::Component component;
};

#endif // TUI_CARD_BROWSER_H
