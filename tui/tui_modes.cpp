#include "tui_modes.h"

QDataStream& operator<<(QDataStream& out, const cardIndex& v)
{
    out << v.index << v.skip;
    return out;
}

QDataStream& operator>>(QDataStream& in, cardIndex& v)
{
    in >> v.index >> v.skip;
    return in;
}

QDataStream& operator<<(QDataStream& out, const box& v)
{
    out << v.howMuchBoxes << v.boxes << v.againValue
        << v.hardValue << v.goodValue << v.easyValue
        << v.defaultSkipValue << v.startingBox;
    return out;
}

QDataStream& operator>>(QDataStream& in, box& v)
{
    in >> v.howMuchBoxes >> v.boxes >> v.againValue
       >> v.hardValue >> v.goodValue >> v.easyValue
       >> v.defaultSkipValue >> v.startingBox;
    return in;
}

QDataStream& operator<<(QDataStream& out, const sm2CardData& v)
{
    out << v.noteId << v.deckId << v.easeFactor << v.interval
        << v.reps << v.lapses << v.state << v.stepIndex
        << v.due << v.lastReview << v.leeched << v.suspended
        << v.buryUntil << v.flagged;
    return out;
}

QDataStream& operator>>(QDataStream& in, sm2CardData& v)
{
    in >> v.noteId >> v.deckId >> v.easeFactor >> v.interval
       >> v.reps >> v.lapses >> v.state >> v.stepIndex
       >> v.due >> v.lastReview >> v.leeched >> v.suspended
       >> v.buryUntil >> v.flagged;
    return in;
}

QDataStream& operator<<(QDataStream& out, const sm2Config& v)
{
    out << v.learningSteps << v.graduatingInterval << v.easyInterval
        << v.startingEase << v.easyBonus << v.intervalModifier
        << v.hardIntervalMultiplier << v.newIntervalFactor
        << v.maxInterval << v.minInterval << v.leechThreshold
        << v.maxReviewsPerDay << v.reversedMode << v.twoButtonMode;
    return out;
}

QDataStream& operator>>(QDataStream& in, sm2Config& v)
{
    in >> v.learningSteps >> v.graduatingInterval >> v.easyInterval
       >> v.startingEase >> v.easyBonus >> v.intervalModifier
       >> v.hardIntervalMultiplier >> v.newIntervalFactor
       >> v.maxInterval >> v.minInterval >> v.leechThreshold
       >> v.maxReviewsPerDay >> v.reversedMode >> v.twoButtonMode;
    return in;
}
