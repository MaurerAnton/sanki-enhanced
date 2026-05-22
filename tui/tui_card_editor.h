#ifndef TUI_CARD_EDITOR_H
#define TUI_CARD_EDITOR_H

#include "mainMenu/sessions/sessionStruct.h"
#include "tui_modes.h"

#include <QSqlDatabase>
#include <QString>
#include <ftxui/component/component.hpp>

class TuiApp;

class TuiCardEditor {
public:
    TuiCardEditor();
    void init(TuiApp* app);
    void setCard(sessionStr session, int cardIndex);
    ftxui::Component getComponent();

    TuiApp* app = nullptr;

private:
    void loadCardData();
    void saveCard();
    void addNewCard();
    void deleteCurrentCard();
    void buildComponent();

    sessionStr session;
    int cardIndex = -1;
    QList<QSqlDatabase> databases;
    std::string frontText;
    std::string backText;
    std::string statusText;
    bool editingFront = true;

    ftxui::Component component;
};

#endif // TUI_CARD_EDITOR_H
