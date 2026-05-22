#ifndef TUI_STUDY_H
#define TUI_STUDY_H

#include "mainMenu/sessions/sessionStruct.h"
#include "tui_modes.h"

#include <QSqlDatabase>
#include <QSettings>
#include <QElapsedTimer>
#include <QMap>
#include <QDateTime>
#include <QDate>
#include <ftxui/component/component.hpp>

class TuiApp;

class TuiStudy {
public:
    TuiStudy();
    void init(TuiApp* app);
    void startSession(sessionStr session);
    ftxui::Component getComponent();

    TuiApp* app = nullptr;

private:
    void buildComponent();
    void initDatabases();
    void closeDatabases();
    void loadSessionData();
    void saveSessionData();
    void setupMode();
    void nextCard();
    void showBack();
    void answer(int ease);
    void updateDisplay();
    void showSessionSummary();

    // SM-2 helpers
    QString makeKey(quint64 noteId, uint deckId);
    QString makeKeyRev(quint64 noteId, uint deckId);
    QList<qint64> parseLearningSteps();
    void sm2AnswerCard(sm2CardData* data, int ease);
    void sm2DoCram();
    void sm2DoFilter();
    void sm2DoSuspend();
    void sm2DoBury();
    void sm2DoFlag();
    void sm2DoUndo();
    void sm2DoReverse();
    void updateSm2Stats();

    sessionStr session;
    QList<QSqlDatabase> databases;
    QSettings* saveSettings = nullptr;
    QElapsedTimer playTimer;
    quint64 savedPlayed = 0;
    bool working = false;
    bool backVisible = false;

    std::string frontText;
    std::string backText;
    std::string statusText;
    std::string deckName;
    std::string statsText;

    int currentCardIndex = -1;
    card* currentCard = nullptr;
    QString currentFrontHtml;
    QString currentBackHtml;

    // Boxes mode
    box theBox;
    int whichBox = 0;
    int whichCard = 0;
    QList<int> boxesCardCount;

    // SM2 mode
    sm2Config sm2config;
    QMap<QString, sm2CardData> cardDataMap;
    int sm2CardListIndex = -1;
    bool sm2Reversed = false;

    // SM-2 session state
    bool cramMode = false;
    StudyFilter studyFilter = FILTER_ALL;
    int sessionAgain = 0;
    int sessionHard = 0;
    int sessionGood = 0;
    int sessionEasy = 0;
    int sessionLeeched = 0;
    quint64 sessionStartTime = 0;
    int reviewsDoneToday = 0;
    int newDoneToday = 0;
    QDate todayDate;

    // SM-2 undo
    bool undoActive = false;
    sm2CardData undoSavedData;
    int undoCardIndex = -1;

    // Pomodoro
    bool pomodoroActive = false;
    int pomodoroSeconds = 0;
    int pomodoroPhase = 0; // 0=learning, 1=break
    int pomodoroSessionCount = 0;
    static const int POMODORO_LEARN = 25 * 60;
    static const int POMODORO_SHORT_BREAK = 5 * 60;
    static const int POMODORO_LONG_BREAK = 15 * 60;
    int pomodoroTotal = POMODORO_LEARN;
    void pomodoroTick();
    void pomodoroToggle();

    ftxui::Component component;
};

#endif // TUI_STUDY_H
