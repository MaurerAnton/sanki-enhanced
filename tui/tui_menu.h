#ifndef TUI_MENU_H
#define TUI_MENU_H

#include "mainMenu/sessions/sessionStruct.h"

#include <QStringList>
#include <QDir>
#include <QList>
#include <ftxui/component/component.hpp>

class TuiApp;

class TuiMenu {
public:
    TuiMenu();
    void init(TuiApp* app);
    void refresh();
    ftxui::Component getComponent();

    TuiApp* app = nullptr;

    struct SessionInfo {
        QString filename;
        sessionStr data;
    };
    std::vector<SessionInfo> sessions;

    struct DeckInfo {
        QString name;
        QString path;
        int cardCount = 0;
    };
    std::vector<DeckInfo> decks;

private:
    void loadSessions();
    void loadDecks();
    void deleteSession(const QString& filename);
    void importDeck();
    void editSession();
    void renameDeck();
    void deleteDeck();
    void buildComponent();
    void createSubSession();
    std::string getDueForecast();

    ftxui::Component component;
    int sessionSelected = 0;
    int deckSelected = 0;

    std::string statusMessage;
    std::string importPath;
    bool inputtingImport = false;

    std::string editSessionName;
    int editModeIndex = 0;
    bool inputtingSessionEdit = false;
    bool sessionEditActive = false;
};

#endif // TUI_MENU_H
