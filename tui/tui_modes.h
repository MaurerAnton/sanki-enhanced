#ifndef TUI_MODES_H
#define TUI_MODES_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QDate>
#include <QMap>
#include <QDataStream>
#include <QDebug>
#include <QMetaType>

// From cardView/modes/boxes/boxes.h (data structs only, no QtWidgets)

struct cardIndex {
    uint index = 0;
    uint skip = 0;
};
Q_DECLARE_METATYPE(cardIndex)

struct box {
    uint howMuchBoxes = 4;
    QList<QList<cardIndex>> boxes;
    int againValue = -2;
    int hardValue = -1;
    int goodValue = 1;
    int easyValue = 2;
    uint defaultSkipValue = 10;
    uint startingBox = 2;
};
Q_DECLARE_METATYPE(box)

QDataStream& operator<<(QDataStream& out, const cardIndex& v);
QDataStream& operator>>(QDataStream& in, cardIndex& v);
QDataStream& operator<<(QDataStream& out, const box& v);
QDataStream& operator>>(QDataStream& in, box& v);

// From cardView/modes/sm2/sm2.h (data structs only, no QtWidgets)

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

QDataStream& operator<<(QDataStream& out, const sm2CardData& v);
QDataStream& operator>>(QDataStream& in, sm2CardData& v);
QDataStream& operator<<(QDataStream& out, const sm2Config& v);
QDataStream& operator>>(QDataStream& in, sm2Config& v);

enum StudyFilter {
    FILTER_ALL = 0,
    FILTER_NEW = 1,
    FILTER_REVIEW = 2,
    FILTER_LEECH = 3,
    FILTER_FLAGGED = 4
};

#endif // TUI_MODES_H
