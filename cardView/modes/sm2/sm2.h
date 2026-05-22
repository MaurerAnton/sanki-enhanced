#ifndef SM2_H
#define SM2_H

#include "cardView/deckPlay.h"
#include "cardView/buttons/fourOptions.h"

#include <ui_deckPlay.h>
#include <QObject>
#include <QTextBrowser>
#include <QMap>
#include <QTimer>
#include <QSettings>

struct sm2CardData {
    quint64 noteId = 0;
    uint deckId = 0;
    qreal easeFactor = 2.5;
    qint64 interval = 0;
    int reps = 0;
    int lapses = 0;
    int state = 0;
    int stepIndex = 0;
    int leeched = 0;
    int suspended = 0;
    int flagged = 0;
    QDateTime due;
    QDate buryUntil;
    QDateTime lastReview;
};
Q_DECLARE_METATYPE(sm2CardData)

struct sm2Config {
    QString learningSteps = "60 600";
    qint64 graduatingInterval = 86400;
    qint64 easyInterval = 345600;
    qreal startingEase = 2.5;
    qreal easyBonus = 0.15;
    qreal intervalModifier = 1.0;
    qreal hardIntervalMultiplier = 1.2;
    qreal newIntervalFactor = 0.0;
    qint64 maxInterval = 3153600000LL;
    qint64 minInterval = 0;
    int leechThreshold = 8;
    int maxReviewsPerDay = 200;
    bool reversedMode = false;
    bool twoButtonMode = false;
};
Q_DECLARE_METATYPE(sm2Config)

enum Sm2State {
    SM2_NEW = 0,
    SM2_LEARNING = 1,
    SM2_REVIEW = 2
};

enum Sm2Answer {
    SM2_ANSWER_AGAIN = 1,
    SM2_ANSWER_HARD = 2,
    SM2_ANSWER_GOOD = 3,
    SM2_ANSWER_EASY = 4
};

enum StudyFilter {
    FILTER_ALL = 0,
    FILTER_NEW = 1,
    FILTER_REVIEW = 2,
    FILTER_LEECH = 3,
    FILTER_FLAGGED = 4
};

class sm2Mode : public QObject
{
    Q_OBJECT
public:
    explicit sm2Mode(QObject *parent = nullptr);
    void setup(DeckPlay* parentArg, Ui::DeckPlay* parentUiArg);
    void loop();
    QString frontCard;
    QString backCard;

    int newCardsToday = 0;
    int reviewCardsToday = 0;
    int totalNew = 0;
    int totalLearning = 0;
    int totalReview = 0;
    void updateStatsLabel();
    void updateCardInfo();
    void showSessionSummary();

public slots:
    void againClicked();
    void hardClicked();
    void goodClicked();
    void easyClicked();
    void showBack();
    void saveData();
    void receiveCall(QString call);

signals:

private:
    DeckPlay* parent;
    Ui::DeckPlay* parentUi;
    QTextBrowser* frontText;
    QTextBrowser* backText;
    QObject* buttonWidget = nullptr;
    bool readyToSave = false;

    sm2Config config;
    QMap<QString, sm2CardData> cardDataMap;
    int currentCardListIndex = -1;

    bool cramMode = false;

    int reviewsDoneToday = 0;
    int newDoneToday = 0;
    QDate todayDate;

    bool currentIsReversed = false;

    StudyFilter studyFilter = FILTER_ALL;
    QString filterName() const;

    int sessionAgain = 0;
    int sessionHard = 0;
    int sessionGood = 0;
    int sessionEasy = 0;
    int sessionLeeched = 0;
    quint64 sessionStartTime = 0;
    void showSessionSummary();

    bool undoActive = false;
    int undoGeneration = 0;
    sm2CardData undoSavedData;
    int undoCardIndex = -1;
    void saveUndoSnap();
    void performUndo();

    QString makeKey(quint64 noteId, uint deckId);
    QString makeKeyRev(quint64 noteId, uint deckId);
    QList<qint64> parseLearningSteps();
    void answerCard(sm2CardData* data, int ease);
    qint64 clampInterval(qint64 interval);
    int daysFromNow(const QDateTime& dt);
};

extern QDataStream& operator<<(QDataStream& out, const sm2CardData& v);
extern QDataStream& operator>>(QDataStream& in, sm2CardData& v);
extern QDebug operator<<(QDebug dbg, const sm2CardData& c);

extern QDataStream& operator<<(QDataStream& out, const sm2Config& v);
extern QDataStream& operator>>(QDataStream& in, sm2Config& v);
extern QDebug operator<<(QDebug dbg, const sm2Config& c);

#endif // SM2_H
