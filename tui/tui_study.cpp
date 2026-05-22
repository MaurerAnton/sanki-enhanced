#include "tui_study.h"
#include "tui_app.h"
#include "tui_utils.h"
#include "tui_card_utils.h"
#include "global.h"

#include <QDebug>
#include <QSqlQuery>
#include <QDateTime>
#include <QDir>
#include <algorithm>
#include <random>
#include <climits>
#include <chrono>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

namespace t = ftxui;

// Helper
static inline QString sm2StateName(int state) {
    switch (state) { case 0: return "New"; case 1: return "Learn"; case 2: return "Review"; default: return "?"; }
}

TuiStudy::TuiStudy() {}
void TuiStudy::init(TuiApp* app) { this->app = app; buildComponent(); }

// ─── Session start ──────────────────────────────────────────────────────────

void TuiStudy::startSession(sessionStr newSession)
{
    working = false; backVisible = false;
    session = newSession; savedPlayed = session.time.played;
    initDatabases(); if (!working) return;

    saveSettings = new QSettings(directories::sessionSaves.filePath(session.core.name), QSettings::IniFormat);

    if (session.cardList.isEmpty()) {
        for (int i = 0; i < databases.count(); i++) {
            QSqlQuery q("SELECT id FROM notes ORDER BY RANDOM()", databases[i]);
            while (q.next()) { card c; c.id = q.value(0).toULongLong(); c.deckiD = (uint)i; session.cardList.append(c); }
        }
        std::random_device rd; std::mt19937 rng(rd());
        std::shuffle(session.cardList.begin(), session.cardList.end(), rng);
    }

    playTimer.start(); working = true;
    loadSessionData(); setupMode();

    // SM-2 session counters
    sessionAgain = 0; sessionHard = 0; sessionGood = 0; sessionEasy = 0; sessionLeeched = 0;
    sessionStartTime = QDateTime::currentSecsSinceEpoch();
    reviewsDoneToday = 0; newDoneToday = 0;
    cramMode = false; studyFilter = FILTER_ALL;

    // Pomodoro reset
    pomodoroActive = false;
    pomodoroSeconds = POMODORO_LEARN;
    pomodoroPhase = 0;
    pomodoroSessionCount = 0;
    pomodoroTotal = POMODORO_LEARN;

    saveSessionData();
    if (session.cardList.isEmpty()) { statusText = "Error: No cards!"; return; }
    nextCard();
}

void TuiStudy::initDatabases()
{
    for (int i = 0; i < session.core.deckPathList.count(); i++) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "study_" + QString::number(i));
        QString dbFile = findDatabaseFileTui(directories::deckStorage.filePath(session.core.deckPathList[i]));
        if (dbFile == "none") { working = false; return; }
        db.setDatabaseName(dbFile); if (!db.open()) { working = false; return; }
        databases.append(db);
    }
}

void TuiStudy::closeDatabases()
{
    if (saveSettings) { saveSessionData(); saveSettings->sync(); delete saveSettings; saveSettings = nullptr; }
    for (int i = 0; i < databases.count(); i++) { databases[i].close(); QSqlDatabase::removeDatabase("study_" + QString::number(i)); }
    databases.clear(); working = false;
}

// ─── Persistence ────────────────────────────────────────────────────────────

void TuiStudy::loadSessionData()
{
    if (session.core.mode == Boxes) {
        QVariant boxVar = saveSettings->value("boxMode/box");
        if (boxVar.isValid() && !boxVar.isNull()) { theBox = boxVar.value<box>(); }
        else {
            theBox = box();
            for (uint i = 0; i < theBox.howMuchBoxes; i++) theBox.boxes.append(QList<cardIndex>());
            for (int i = 0; i < session.cardList.count(); i++) { cardIndex ci; ci.index = i; theBox.boxes[theBox.startingBox - 1].append(ci); }
        }
        boxesCardCount.clear();
        for (int i = 0; i < (int)theBox.howMuchBoxes; i++) boxesCardCount.append(theBox.boxes[i].count());
    }
    if (session.core.mode == SM2) {
        QVariant cv = saveSettings->value("sm2Mode/config");
        sm2config = (cv.isValid() && !cv.isNull()) ? cv.value<sm2Config>() : sm2Config();
        QVariant dv = saveSettings->value("sm2Mode/cardData");
        if (dv.isValid() && !dv.isNull()) {
            QMap<QString, QVariant> raw = dv.toMap();
            for (auto it = raw.begin(); it != raw.end(); ++it) cardDataMap[it.key()] = it.value().value<sm2CardData>();
        }
    }
}

void TuiStudy::saveSessionData()
{
    if (!working) return;
    qint64 elapsed = playTimer.restart();
    session.time.lastUsed = QDateTime::currentDateTime();
    session.time.played = savedPlayed + elapsed; savedPlayed = session.time.played;

    QVariant var = QVariant::fromValue(session);
    saveSettings->setValue("session", var);
    if (session.core.mode == Boxes) saveSettings->setValue("boxMode/box", QVariant::fromValue(theBox));
    if (session.core.mode == SM2) {
        saveSettings->setValue("sm2Mode/config", QVariant::fromValue(sm2config));
        QMap<QString, QVariant> raw;
        for (auto it = cardDataMap.begin(); it != cardDataMap.end(); ++it) raw[it.key()] = QVariant::fromValue(it.value());
        saveSettings->setValue("sm2Mode/cardData", QVariant::fromValue(raw));
    }
    saveSettings->sync();
}

void TuiStudy::setupMode() {}

// ─── SM-2 helpers ───────────────────────────────────────────────────────────

QString TuiStudy::makeKey(quint64 nid, uint did)  { return QString::number(nid) + "_" + QString::number(did); }
QString TuiStudy::makeKeyRev(quint64 nid, uint did) { return makeKey(nid, did) + "_rev"; }

QList<qint64> TuiStudy::parseLearningSteps()
{
    QList<qint64> s;
    for (const QString& p : sm2config.learningSteps.split(" ", Qt::SkipEmptyParts)) {
        bool ok; qint64 v = p.toLongLong(&ok); if (ok && v > 0) s.append(v);
    }
    if (s.isEmpty()) s.append(60);
    return s;
}

void TuiStudy::sm2AnswerCard(sm2CardData* data, int ease)
{
    QDateTime now = QDateTime::currentDateTime();
    data->lastReview = now;
    data->reps++;

    if (data->state == SM2_REVIEW) reviewsDoneToday++;
    else if (data->state == SM2_NEW) newDoneToday++;

    switch (ease) { case 1: sessionAgain++; break; case 2: sessionHard++; break; case 3: sessionGood++; break; case 4: sessionEasy++; break; }

    if (data->state == SM2_NEW || data->state == SM2_LEARNING) {
        data->reps--;
        if (ease == Sm2Answer::SM2_ANSWER_EASY) {
            data->interval = sm2config.easyInterval; data->easeFactor = sm2config.startingEase;
            data->state = SM2_REVIEW; data->due = now.addSecs(data->interval);
        } else if (ease == Sm2Answer::SM2_ANSWER_GOOD) {
            QList<qint64> steps = parseLearningSteps();
            data->stepIndex++;
            if (data->stepIndex >= steps.size()) {
                data->interval = sm2config.graduatingInterval; data->easeFactor = sm2config.startingEase;
                data->state = SM2_REVIEW; data->due = now.addSecs(data->interval);
            } else { data->state = SM2_LEARNING; data->due = now.addSecs(steps[data->stepIndex]); }
        } else if (ease == Sm2Answer::SM2_ANSWER_HARD) {
            data->state = SM2_LEARNING;
            QList<qint64> steps = parseLearningSteps();
            if (data->stepIndex < steps.size()) data->due = now.addSecs(steps[data->stepIndex]);
        } else {
            data->stepIndex = 0; data->state = SM2_LEARNING;
            data->due = now.addSecs(parseLearningSteps()[0]);
        }
        return;
    }

    if (data->state == SM2_REVIEW) {
        if (ease == Sm2Answer::SM2_ANSWER_AGAIN) {
            data->lapses++;
            if (data->lapses >= sm2config.leechThreshold && sm2config.leechThreshold > 0) {
                if (!data->leeched) { data->leeched = 1; sessionLeeched++; }
            }
            data->easeFactor = std::max(1.3, data->easeFactor - 0.20);
            data->interval = (qint64)(data->interval * sm2config.newIntervalFactor);
            data->stepIndex = 0; data->state = SM2_LEARNING;
            data->due = now.addSecs(parseLearningSteps()[0]);
        } else if (ease == Sm2Answer::SM2_ANSWER_HARD) {
            data->easeFactor = std::max(1.3, data->easeFactor - 0.15);
            data->interval = (qint64)(data->interval * sm2config.hardIntervalMultiplier);
            data->interval = std::min(data->interval, sm2config.maxInterval);
            data->interval = (qint64)(data->interval * sm2config.intervalModifier);
            data->due = now.addSecs(std::min(data->interval, sm2config.maxInterval));
        } else if (ease == Sm2Answer::SM2_ANSWER_GOOD) {
            data->interval = (qint64)(data->interval * data->easeFactor);
            data->interval = std::min(data->interval, sm2config.maxInterval);
            data->interval = (qint64)(data->interval * sm2config.intervalModifier);
            data->due = now.addSecs(std::min(data->interval, sm2config.maxInterval));
        } else if (ease == Sm2Answer::SM2_ANSWER_EASY) {
            data->interval = (qint64)(data->interval * data->easeFactor * (1.0 + sm2config.easyBonus));
            data->easeFactor += 0.15;
            data->interval = std::min(data->interval, sm2config.maxInterval);
            data->interval = (qint64)(data->interval * sm2config.intervalModifier);
            data->due = now.addSecs(std::min(data->interval, sm2config.maxInterval));
        }
    }
}

// ─── Next card ──────────────────────────────────────────────────────────────

void TuiStudy::nextCard()
{
    if (!working || session.cardList.isEmpty()) return;
    backVisible = false; undoActive = false;
    saveSessionData();

    if (session.core.mode == CompletlyRandomised) {
        currentCardIndex = randomValue(0, session.cardList.count() - 1);
        sm2Reversed = false;
    } else if (session.core.mode == Boxes) {
        bool found = false;
        for (uint i = 0; i < theBox.howMuchBoxes && !found; i++) {
            for (int ii = 0; ii < theBox.boxes[i].count(); ii++) {
                if (theBox.boxes[i][ii].skip == 0) { whichBox = i; whichCard = ii; found = true; break; }
                theBox.boxes[i][ii].skip -= 1;
            }
        }
        if (!found) for (uint i = 0; i < theBox.howMuchBoxes && !found; i++)
            if (theBox.boxes[i].count() > 0) { whichBox = i; whichCard = 0; found = true; }
        if (!found) { statusText = "No cards available"; return; }
        currentCardIndex = theBox.boxes[whichBox][whichCard].index;
    } else if (session.core.mode == SM2) {
        QDateTime now = QDateTime::currentDateTime();
        QDate curDate = now.date();
        if (todayDate.isValid() && todayDate != curDate) { reviewsDoneToday = 0; newDoneToday = 0; }
        if (!todayDate.isValid()) todayDate = curDate;

        QList<int> dueIndices; int firstNew = -1;

        for (int i = 0; i < session.cardList.count(); i++) {
            card& c = session.cardList[i];

            // Forward direction
            QString keyFwd = makeKey(c.id, c.deckiD);
            if (!cardDataMap.contains(keyFwd)) { if (!sm2Reversed && firstNew == -1) firstNew = i; }
            else {
                sm2CardData& d = cardDataMap[keyFwd];
                bool skip = false;
                if (d.leeched) skip = (studyFilter != FILTER_LEECH);
                else if (studyFilter == FILTER_LEECH) skip = true;
                else if (studyFilter == FILTER_NEW && d.state != SM2_NEW) skip = true;
                else if (studyFilter == FILTER_REVIEW && d.state != SM2_REVIEW) skip = true;
                else if (studyFilter == FILTER_FLAGGED && !d.flagged) skip = true;
                if (!skip && (cramMode || (d.due.isValid() && d.due <= now))) {
                    if (d.state == SM2_REVIEW || d.state == SM2_LEARNING) dueIndices.append(i * 2);
                }
            }

            // Reverse direction
            if (sm2config.reversedMode) {
                QString keyRev = makeKeyRev(c.id, c.deckiD);
                if (!cardDataMap.contains(keyRev)) { if (sm2Reversed && firstNew == -1) firstNew = i; }
                else {
                    sm2CardData& d = cardDataMap[keyRev];
                    bool skip = false;
                    if (d.leeched) skip = (studyFilter != FILTER_LEECH);
                    else if (studyFilter == FILTER_NEW && d.state != SM2_NEW) skip = true;
                    else if (studyFilter == FILTER_REVIEW && d.state != SM2_REVIEW) skip = true;
                    else if (studyFilter == FILTER_FLAGGED && !d.flagged) skip = true;
                    if (!skip && (cramMode || (d.due.isValid() && d.due <= now))) {
                        if (d.state == SM2_REVIEW || d.state == SM2_LEARNING) dueIndices.append(i * 2 + 1);
                    }
                }
            }
        }

        if (!dueIndices.isEmpty()) {
            std::random_device rd; std::mt19937 rng(rd());
            std::shuffle(dueIndices.begin(), dueIndices.end(), rng);
            int encoded = dueIndices.first();
            sm2CardListIndex = encoded / 2; sm2Reversed = (encoded % 2) == 1;
        } else if (firstNew >= 0) {
            sm2CardListIndex = firstNew; sm2Reversed = false;
        } else {
            int fewestDays = INT_MAX; int bestEncoded = 0;
            for (int i = 0; i < session.cardList.count(); i++) {
                card& c = session.cardList[i];
                QString keyFwd = makeKey(c.id, c.deckiD);
                if (cardDataMap.contains(keyFwd)) {
                    sm2CardData& d = cardDataMap[keyFwd];
                    if (!d.suspended && d.due.isValid() && d.due > now) {
                        int days = now.secsTo(d.due) / 86400;
                        if (days < fewestDays) { fewestDays = days; bestEncoded = i * 2; }
                    }
                }
                if (sm2config.reversedMode) {
                    QString keyRev = makeKeyRev(c.id, c.deckiD);
                    if (cardDataMap.contains(keyRev)) {
                        sm2CardData& d = cardDataMap[keyRev];
                        if (!d.suspended && d.due.isValid() && d.due > now) {
                            int days = now.secsTo(d.due) / 86400;
                            if (days < fewestDays) { fewestDays = days; bestEncoded = i * 2 + 1; }
                        }
                    }
                }
            }
            sm2CardListIndex = bestEncoded / 2; sm2Reversed = (bestEncoded % 2) == 1;
        }
        currentCardIndex = sm2CardListIndex;

        // Ensure SM-2 data exists
        if (currentCardIndex >= 0 && currentCardIndex < session.cardList.count()) {
            card& c = session.cardList[currentCardIndex];
            QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
            if (!cardDataMap.contains(key)) {
                sm2CardData nd; nd.noteId = c.id; nd.deckId = c.deckiD; nd.easeFactor = sm2config.startingEase;
                cardDataMap[key] = nd;
            }
        }
    }

    if (currentCardIndex < 0 || currentCardIndex >= session.cardList.count()) return;
    currentCard = &session.cardList[currentCardIndex];
    currentCard->count++;

    // Extract content
    QSqlQuery query("SELECT flds FROM notes WHERE id = :id", databases[currentCard->deckiD]);
    query.bindValue(0, currentCard->id);
    QString mainCard;
    if (query.exec() && query.next()) mainCard = query.value(0).toString();

    QString mediaPath = directories::deckStorage.filePath(session.core.deckPathList[currentCard->deckiD]) + QDir::separator() + "media";
    correctMainCard(&mainCard, mediaPath);

    QStringList parts = mainCard.split("\u001F");
    currentFrontHtml = parts.first(); currentBackHtml = (parts.size() > 1) ? parts.last() : "";
    processClozeDeletions(&currentFrontHtml, &currentBackHtml);

    if (sm2Reversed) std::swap(currentFrontHtml, currentBackHtml);
    if (session.deckOptions.inverted) std::swap(currentFrontHtml, currentBackHtml);

    frontText = qstrToStd(stripHtml(currentFrontHtml));
    backText  = qstrToStd(stripHtml(currentBackHtml));

    deckName = qstrToStd(session.core.deckPathList[currentCard->deckiD].split("/").last());
    if (deckName.length() > 20) deckName = truncate(deckName, 20);

    // Build stats text
    updateSm2Stats();
    std::string modeStr = modeToString(session.core.mode);
    if (session.core.mode == Boxes) {
        std::string bs;
        for (int i = 0; i < boxesCardCount.count(); i++) bs += std::to_string(boxesCardCount[i]) + " ";
        statsText = "Boxes: " + bs + "| " + std::to_string(currentCardIndex + 1) + "/" + std::to_string(session.cardList.count());
    } else if (session.core.mode == SM2) {
        std::string extra;
        auto& c = session.cardList[currentCardIndex];
        QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
        if (cardDataMap.contains(key)) {
            auto& d = cardDataMap[key];
            if (d.leeched) extra += " LEECH";
            else if (d.state == SM2_NEW) extra += " NEW";
            else if (d.state == SM2_LEARNING) extra += " LRN " + std::to_string(d.stepIndex + 1);
            else extra += " E:" + std::to_string((int)(d.easeFactor * 100)) + "%";
            if (d.lapses) extra += " L:" + std::to_string(d.lapses);
        }
        if (cramMode) extra += " [CRAM]";
        if (studyFilter == FILTER_NEW) extra += " [NEW]";
        else if (studyFilter == FILTER_REVIEW) extra += " [REV]";
        else if (studyFilter == FILTER_LEECH) extra += " [LEECH]";
        else if (studyFilter == FILTER_FLAGGED) extra += " [FLAG]";
        if (sm2Reversed && sm2config.reversedMode) extra += " [REV-CARD]";
        statsText += extra;
    } else {
        statsText = modeStr + " | " + std::to_string(currentCardIndex + 1) + "/" + std::to_string(session.cardList.count());
    }

    statusText = "[" + deckName + "] " + statsText + " | " + formatTime(session.time.played);
}

void TuiStudy::updateSm2Stats()
{
    if (session.core.mode != SM2) return;
    int tN = 0, tL = 0, tR = 0, tLeech = 0;
    for (int i = 0; i < session.cardList.count(); i++) {
        card& c = session.cardList[i];
        QString key = makeKey(c.id, c.deckiD);
        if (!cardDataMap.contains(key)) tN++;
        else {
            auto& d = cardDataMap[key];
            if (d.leeched) tLeech++;
            else if (d.state == SM2_LEARNING) tL++;
            else if (d.state == SM2_REVIEW) tR++;
            else tN++;
        }
    }
    statsText = "N:" + std::to_string(tN) + " L:" + std::to_string(tL) + " R:" + std::to_string(tR);
    if (tLeech > 0) statsText += " Leech:" + std::to_string(tLeech);
    if (sm2config.maxReviewsPerDay > 0) statsText += " Done:" + std::to_string(reviewsDoneToday) + "/" + std::to_string(sm2config.maxReviewsPerDay);
    if (cramMode) statsText += " CRAM";
}

// ─── SM-2 actions ───────────────────────────────────────────────────────────

void TuiStudy::sm2DoCram()
{
    cramMode = !cramMode;
    statsText = cramMode ? "Cram mode ON — all cards due now" : "Cram mode OFF";
}

void TuiStudy::sm2DoFilter()
{
    studyFilter = static_cast<StudyFilter>((studyFilter + 1) % 5);
    const char* names[] = {"All", "New only", "Review only", "Leeched only", "Flagged"};
    statsText = std::string("Filter: ") + names[studyFilter];
}

void TuiStudy::sm2DoSuspend()
{
    if (currentCardIndex < 0) return;
    card& c = session.cardList[currentCardIndex];
    QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
    if (cardDataMap.contains(key)) {
        cardDataMap[key].suspended = !cardDataMap[key].suspended;
        statsText = cardDataMap[key].suspended ? "Card suspended" : "Card unsuspended";
    }
}

void TuiStudy::sm2DoBury()
{
    if (currentCardIndex < 0) return;
    card& c = session.cardList[currentCardIndex];
    QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
    if (cardDataMap.contains(key)) {
        cardDataMap[key].buryUntil = QDate::currentDate();
        statsText = "Card buried until tomorrow";
    }
}

void TuiStudy::sm2DoFlag()
{
    if (currentCardIndex < 0) return;
    card& c = session.cardList[currentCardIndex];
    QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
    if (cardDataMap.contains(key)) {
        cardDataMap[key].flagged = !cardDataMap[key].flagged;
        statsText = cardDataMap[key].flagged ? "Card flagged" : "Card unflagged";
    }
}

void TuiStudy::sm2DoUndo()
{
    if (!undoActive || currentCardIndex < 0) return;
    undoActive = false;
    card& c = session.cardList[currentCardIndex];
    QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
    cardDataMap[key] = undoSavedData;

    // Redisplay same card
    QString mainCard;
    QSqlQuery q("SELECT flds FROM notes WHERE id = :id", databases[c.deckiD]);
    q.bindValue(0, c.id);
    if (q.exec() && q.next()) mainCard = q.value(0).toString();
    QString mp = directories::deckStorage.filePath(session.core.deckPathList[c.deckiD]) + QDir::separator() + "media";
    correctMainCard(&mainCard, mp);
    QStringList parts = mainCard.split("\u001F");
    currentFrontHtml = parts.first(); currentBackHtml = (parts.size() > 1) ? parts.last() : "";
    processClozeDeletions(&currentFrontHtml, &currentBackHtml);
    if (sm2Reversed) std::swap(currentFrontHtml, currentBackHtml);
    if (session.deckOptions.inverted) std::swap(currentFrontHtml, currentBackHtml);
    frontText = qstrToStd(stripHtml(currentFrontHtml));
    backText  = qstrToStd(stripHtml(currentBackHtml));
    backVisible = false;
    statsText = "UNDONE — card restored";
    updateSm2Stats();
}

void TuiStudy::sm2DoReverse()
{
    if (session.core.mode != SM2) return;
    sm2config.reversedMode = !sm2config.reversedMode;
    statsText = sm2config.reversedMode ? "Reversed mode ON (both directions)" : "Reversed mode OFF";
}

void TuiStudy::showSessionSummary()
{
    if (session.core.mode != SM2) return;
    int total = sessionAgain + sessionHard + sessionGood + sessionEasy;
    if (total == 0) return;
    qint64 elapsed = QDateTime::currentSecsSinceEpoch() - sessionStartTime;
    int minutes = elapsed / 60; int seconds = elapsed % 60;
    char buf[512];
    snprintf(buf, sizeof(buf),
        "=== Session Summary ===\n"
        "Cards studied: %d\nTime: %dm %ds\n\n"
        "Answers: Again:%d Hard:%d Good:%d Easy:%d\n"
        "%s%s",
        total, minutes, seconds,
        sessionAgain, sessionHard, sessionGood, sessionEasy,
        sessionGood > 0 ? ("Retention: " + std::to_string((sessionGood + sessionEasy) * 100 / total) + "%\n").c_str() : "",
        sessionLeeched > 0 ? ("Leeches: " + std::to_string(sessionLeeched) + "\n").c_str() : ""
    );
    statsText = std::string(buf);
}

// ─── Show back / answer ─────────────────────────────────────────────────────

void TuiStudy::showBack()
{
    if (undoActive) { sm2DoUndo(); return; }
    backVisible = true;
}

void TuiStudy::answer(int ease)
{
    if (!working || !currentCard) return;

    // Save undo snapshot
    if (session.core.mode == SM2) {
        card& c = session.cardList[currentCardIndex];
        QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
        if (cardDataMap.contains(key)) undoSavedData = cardDataMap[key];
        else { undoSavedData = sm2CardData(); undoSavedData.noteId = c.id; undoSavedData.deckId = c.deckiD; }
        undoCardIndex = currentCardIndex; undoActive = true;
    }

    if (session.core.mode == Boxes) {
        int mv = 0;
        switch (ease) { case 1: mv = theBox.againValue; break; case 2: mv = theBox.hardValue; break; case 3: mv = theBox.goodValue; break; case 4: mv = theBox.easyValue; break; }
        cardIndex saved = theBox.boxes[whichBox][whichCard];
        theBox.boxes[whichBox].removeAt(whichCard);
        if (whichBox < boxesCardCount.size()) {
            boxesCardCount[whichBox]--;
            int nb = whichBox + mv; if (nb < 0) nb = 0; if ((uint)nb >= theBox.howMuchBoxes) nb = theBox.howMuchBoxes - 1;
            if (nb < boxesCardCount.size()) boxesCardCount[nb]++;
        }
        int nbi = whichBox + mv; if (nbi < 0) nbi = 0; if ((uint)nbi > theBox.howMuchBoxes - 1) nbi = theBox.howMuchBoxes - 1;
        int skip = theBox.defaultSkipValue - theBox.boxes[nbi].count(); if (skip < 0) skip = 0;
        saved.skip = skip; theBox.boxes[nbi].append(saved);
    }

    if (session.core.mode == SM2) {
        card& c = session.cardList[currentCardIndex];
        QString key = sm2Reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
        if (!cardDataMap.contains(key)) {
            sm2CardData nd; nd.noteId = c.id; nd.deckId = c.deckiD; nd.easeFactor = sm2config.startingEase;
            cardDataMap[key] = nd;
        }
        sm2AnswerCard(&cardDataMap[key], ease);
    }

    saveSessionData(); nextCard();
}

// ─── Pomodoro ───────────────────────────────────────────────────────────────

void TuiStudy::pomodoroToggle()
{
    pomodoroActive = !pomodoroActive;
    if (pomodoroActive) {
        pomodoroSeconds = POMODORO_LEARN;
        pomodoroTotal = POMODORO_LEARN;
        pomodoroPhase = 0;
    }
}

void TuiStudy::pomodoroTick()
{
    if (!pomodoroActive) return;
    pomodoroSeconds--;
    if (pomodoroSeconds <= 0) {
        if (pomodoroPhase == 0) {
            // Learning session ended → break
            pomodoroSessionCount++;
            if (pomodoroSessionCount >= 4) {
                pomodoroPhase = 1; pomodoroSeconds = POMODORO_LONG_BREAK; pomodoroTotal = POMODORO_LONG_BREAK;
                pomodoroSessionCount = 0;
            } else {
                pomodoroPhase = 1; pomodoroSeconds = POMODORO_SHORT_BREAK; pomodoroTotal = POMODORO_SHORT_BREAK;
            }
            statsText = pomodoroPhase == 1 ? "TIME'S UP! Take a break..." : "BREAK OVER! Back to work!";
        } else {
            // Break ended → learning
            pomodoroPhase = 0; pomodoroSeconds = POMODORO_LEARN; pomodoroTotal = POMODORO_LEARN;
            statsText = "Back to studying!";
        }
    }
}

// ─── Render component ───────────────────────────────────────────────────────

void TuiStudy::buildComponent()
{
    component = t::Renderer([this] {
        // Pomodoro tick on every render (FTXUI renders ~30fps, so about 1 tick/sec)
        static auto lastTick = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTick).count() >= 1) {
            pomodoroTick(); lastTick = now;
        }
        if (!working) {
            return t::border(t::center(t::vbox({
                t::center(t::border(t::bold(t::text(" Sanki Study ")))),
                t::separator(),
                t::center(t::text(" No active session. Press Esc to return. ")),
            })));
        }

        auto hdr = t::border(t::center(t::hbox({
            t::bold(t::text(" Sanki Study ")),
            t::dim(t::text(" | " + deckName)),
            t::dim(t::text(" | " + modeToString(session.core.mode))),
            t::dim(t::text(" | Cards: " + std::to_string(session.cardList.count()))),
        })));

        t::Elements ca;
        ca.push_back(t::center(t::color(t::Color::Cyan, t::bold(t::text(" FRONT ")))));
        ca.push_back(t::separator());
        ca.push_back(t::border(t::center(t::size(t::HEIGHT, t::GREATER_THAN, 4)(t::paragraph(frontText)))));
        if (backVisible) {
            ca.push_back(t::center(t::color(t::Color::Green, t::bold(t::text(" BACK ")))));
            ca.push_back(t::separator());
            ca.push_back(t::border(t::center(t::size(t::HEIGHT, t::GREATER_THAN, 4)(t::paragraph(backText)))));
        }

        auto controls = t::center(t::hbox({
            t::color(t::Color::Cyan, t::bold(t::text(" [Space] Show "))),
            t::color(t::Color::Red, t::text(" [1] Again ")),
            t::color(t::Color::Yellow, t::text(" [2] Hard ")),
            t::color(t::Color::Green, t::text(" [3] Good ")),
            t::color(t::Color::Blue, t::text(" [4] Easy ")),
            t::bold(t::text(" [p] Pomodoro ")),
            t::bold(t::text(" [Esc] Back ")),
        }));

        auto sm2bar = t::hbox({});  // empty by default
        if (session.core.mode == SM2) {
            sm2bar = t::center(t::hbox({
                t::dim(t::text("[C]ram ")),
                t::dim(t::text("[F]ilter ")),
                t::dim(t::text("[S]uspend ")),
                t::dim(t::text("[B]ury ")),
                t::dim(t::text("[G] Flag ")),
                t::dim(t::text("[R]everse ")),
                t::dim(t::text("[U]ndo ")),
            }));
        }

        return t::border(t::vbox({
            hdr,
            t::flex(t::vbox(std::move(ca))),
            t::separator(),
            t::hbox({
                t::dim(t::text(" " + statsText + " ")),
                t::dim(t::text(" " + formatTime(session.time.played) + " ")),
                pomodoroActive ? t::color(pomodoroPhase ? t::Color::Green : t::Color::Red,
                    t::bold(t::text(std::string(pomodoroPhase ? "☕" : "⏳") + " " +
                        std::to_string(pomodoroSeconds / 60) + ":" +
                        (pomodoroSeconds % 60 < 10 ? "0" : "") + std::to_string(pomodoroSeconds % 60)
                ))) : t::text(""),
            }),
            t::separator(),
            controls,
            sm2bar,
        }));
    });

    component |= t::CatchEvent([this](t::Event e) -> bool {
        if (!working) {
            if (e == t::Event::Escape || e == t::Event::Character('q')) { app->navigateTo(TuiScreen::MENU); return true; }
            return false;
        }
        if (e == t::Event::Escape) { showSessionSummary(); closeDatabases(); app->navigateTo(TuiScreen::MENU); return true; }
        if (e == t::Event::Character(' ')) { if (!backVisible) showBack(); return true; }
        if (e == t::Event::Character('p') || e == t::Event::Character('P')) { pomodoroToggle(); return true; }
        if (backVisible) {
            if (e == t::Event::Character('1')) { answer(1); return true; }
            if (e == t::Event::Character('2')) { answer(2); return true; }
            if (e == t::Event::Character('3')) { answer(3); return true; }
            if (e == t::Event::Character('4')) { answer(4); return true; }
        }
        if (session.core.mode == SM2) {
            if (e == t::Event::Character('c') || e == t::Event::Character('C')) { sm2DoCram(); saveSessionData(); nextCard(); return true; }
            if (e == t::Event::Character('f') || e == t::Event::Character('F')) { sm2DoFilter(); saveSessionData(); nextCard(); return true; }
            if (e == t::Event::Character('s') || e == t::Event::Character('S')) { sm2DoSuspend(); saveSessionData(); nextCard(); return true; }
            if (e == t::Event::Character('b') || e == t::Event::Character('B')) { sm2DoBury(); saveSessionData(); nextCard(); return true; }
            if (e == t::Event::Character('g') || e == t::Event::Character('G')) { sm2DoFlag(); saveSessionData(); return true; }
            if (e == t::Event::Character('r') || e == t::Event::Character('R')) { sm2DoReverse(); saveSessionData(); return true; }
            if (e == t::Event::Character('u') || e == t::Event::Character('U')) { sm2DoUndo(); return true; }
        }
        return false;
    });
}

t::Component TuiStudy::getComponent() { return component; }
