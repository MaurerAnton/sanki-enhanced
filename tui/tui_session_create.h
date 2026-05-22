#ifndef TUI_SESSION_CREATE_H
#define TUI_SESSION_CREATE_H

#include "mainMenu/sessions/sessionStruct.h"
#include <QStringList>
#include <QDir>
#include <ftxui/component/component.hpp>

class TuiApp;

class TuiSessionCreate {
public:
    TuiSessionCreate();
    void init(TuiApp* app);
    void reset();
    ftxui::Component getComponent();

    TuiApp* app = nullptr;

private:
    void loadAvailableDecks();
    void createSession();
    void buildComponent();

    struct DeckChoice {
        QString path;
        QString name;
        bool selected = false;
    };
    std::vector<DeckChoice> availableDecks;

    int modeIndex = 0;
    std::vector<std::string> modeNames;
    std::string sessionName = "New Session";
    std::string statusMessage;
    bool inputtingName = false;

    ftxui::Component component;
};

#endif // TUI_SESSION_CREATE_H
