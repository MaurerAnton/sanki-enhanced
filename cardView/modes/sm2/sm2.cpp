#include "cardView/modes/sm2/sm2.h"
#include "cardView/deckPlay.h"
#include "ui_deckPlay.h"
#include "cardView/functions/helperFunctions.h"
#include "cardView/modes/sm2/sm2options.h"
#include "cardView/buttons/fourOptionsNFlashy.h"

#include <QDebug>
#include <QTimer>
#include <QApplication>
#include <QSettings>

#include <algorithm>
#include <random>
#include <climits>

sm2Mode::sm2Mode(QObject *parent)
    : QObject{parent}
{
}

void sm2Mode::setup(DeckPlay* parentArg, Ui::DeckPlay* parentUiArg)
{
    parent = parentArg;
    parentUi = parentUiArg;
    frontText = parentUi->textFrontCard;
    backText = parentUi->textBackCard;

    if (parent->enabledTapGesture == false) {
        fourOptions* buttons = new fourOptions();
        buttonWidget = buttons;
        connect(buttons, &fourOptions::again, this, &sm2Mode::againClicked);
        connect(buttons, &fourOptions::hard, this, &sm2Mode::hardClicked);
        connect(buttons, &fourOptions::good, this, &sm2Mode::goodClicked);
        connect(buttons, &fourOptions::easy, this, &sm2Mode::easyClicked);
        connect(buttons, &fourOptions::show, this, &sm2Mode::showBack);
        parentUi->gridManageCard->addWidget(buttons);
    } else {
        fourOptionsNFlashy* buttons = new fourOptionsNFlashy();
        buttonWidget = buttons;
        connect(buttons, &fourOptionsNFlashy::again, this, &sm2Mode::againClicked);
        connect(buttons, &fourOptionsNFlashy::hard, this, &sm2Mode::hardClicked);
        connect(buttons, &fourOptionsNFlashy::good, this, &sm2Mode::goodClicked);
        connect(buttons, &fourOptionsNFlashy::easy, this, &sm2Mode::easyClicked);
        connect(parent, &DeckPlay::tapGesture, this, &sm2Mode::showBack);
        connect(parent, &DeckPlay::tapGesture, buttons, &fourOptionsNFlashy::enableButtons);
        parentUi->gridManageCard->addWidget(buttons);
    }

    if (parent->saveSession->value("sm2Mode/config").isNull() == true &&
        parent->saveSession->value("sm2Mode/config").isValid() == false) {
        config = sm2Config();
        sm2OptionsDialog* options = new sm2OptionsDialog();
        options->start(&config);
        options->exec();
    } else {
        config = parent->saveSession->value("sm2Mode/config").value<sm2Config>();
    }

    if (config.twoButtonMode && buttonWidget) {
        QMetaObject::invokeMethod(buttonWidget, "setTwoButtonMode",
            Q_ARG(bool, true));
    }

    QVariant savedData = parent->saveSession->value("sm2Mode/cardData");
    if (savedData.isValid() && !savedData.isNull()) {
        QMap<QString, QVariant> rawMap = savedData.toMap();
        for (auto it = rawMap.begin(); it != rawMap.end(); ++it) {
            cardDataMap[it.key()] = it.value().value<sm2CardData>();
        }
    }

    totalNew = parent->currectSession.cardList.count();
    totalLearning = 0;
    totalReview = 0;
    for (auto& d : cardDataMap) {
        if (d.state == SM2_LEARNING) totalLearning++;
        else if (d.state == SM2_REVIEW) totalReview++;
        else totalNew--;
    }

    sessionAgain = 0;
    sessionHard = 0;
    sessionGood = 0;
    sessionEasy = 0;
    sessionLeeched = 0;
    sessionStartTime = QDateTime::currentSecsSinceEpoch();

    readyToSave = true;
    updateStatsLabel();
    loop();
}

QString sm2Mode::makeKey(quint64 noteId, uint deckId)
{
    return QString::number(noteId) + "_" + QString::number(deckId);
}

QList<qint64> sm2Mode::parseLearningSteps()
{
    QList<qint64> steps;
    QStringList parts = config.learningSteps.split(" ", Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        qint64 val = p.toLongLong(&ok);
        if (ok && val > 0) {
            steps.append(val);
        }
    }
    if (steps.isEmpty()) {
        steps.append(60);
    }
    return steps;
}

qint64 sm2Mode::clampInterval(qint64 interval)
{
    if (interval < config.minInterval) interval = config.minInterval;
    if (interval > config.maxInterval) interval = config.maxInterval;
    return interval;
}

int sm2Mode::daysFromNow(const QDateTime& dt)
{
    if (!dt.isValid()) return -1;
    qint64 secs = QDateTime::currentDateTime().secsTo(dt);
    return (int)(secs / 86400);
}

void sm2Mode::answerCard(sm2CardData* data, int ease)
{
    QDateTime now = QDateTime::currentDateTime();
    data->lastReview = now;
    data->reps++;

    if (data->state == SM2_REVIEW) reviewsDoneToday++;
    else if (data->state == SM2_NEW) newDoneToday++;

    switch (ease) {
        case SM2_ANSWER_AGAIN: sessionAgain++; break;
        case SM2_ANSWER_HARD: sessionHard++; break;
        case SM2_ANSWER_GOOD: sessionGood++; break;
        case SM2_ANSWER_EASY: sessionEasy++; break;
    }

    if (data->state == SM2_NEW || data->state == SM2_LEARNING) {
        data->reps--;

        if (ease == SM2_ANSWER_EASY) {
            data->interval = config.easyInterval;
            data->easeFactor = config.startingEase;
            data->state = SM2_REVIEW;
            data->due = now.addSecs(data->interval);
        } else if (ease == SM2_ANSWER_GOOD) {
            QList<qint64> steps = parseLearningSteps();
            data->stepIndex++;
            if (data->stepIndex >= steps.size()) {
                data->interval = config.graduatingInterval;
                data->easeFactor = config.startingEase;
                data->state = SM2_REVIEW;
                data->due = now.addSecs(data->interval);
            } else {
                data->state = SM2_LEARNING;
                data->due = now.addSecs(steps[data->stepIndex]);
            }
        } else if (ease == SM2_ANSWER_HARD) {
            QList<qint64> steps = parseLearningSteps();
            if (data->stepIndex < steps.size()) {
                data->due = now.addSecs(steps[data->stepIndex]);
            }
            data->state = SM2_LEARNING;
        } else {
            QList<qint64> steps = parseLearningSteps();
            data->stepIndex = 0;
            data->due = now.addSecs(steps[0]);
            data->state = SM2_LEARNING;
        }
        return;
    }

    if (data->state == SM2_REVIEW) {
        if (ease == SM2_ANSWER_AGAIN) {
            data->lapses++;
            if (data->lapses >= config.leechThreshold && config.leechThreshold > 0) {
                if (!data->leeched) {
                    data->leeched = 1;
                    sessionLeeched++;
                }
            }
            data->easeFactor = std::max(1.3, data->easeFactor - 0.20);
            data->interval = (qint64)(data->interval * config.newIntervalFactor);
            data->stepIndex = 0;
            data->state = SM2_LEARNING;
            QList<qint64> steps = parseLearningSteps();
            data->due = now.addSecs(steps[0]);
        } else if (ease == SM2_ANSWER_HARD) {
            data->easeFactor = std::max(1.3, data->easeFactor - 0.15);
            data->interval = (qint64)(data->interval * config.hardIntervalMultiplier);
            data->interval = clampInterval(data->interval);
            data->interval = (qint64)(data->interval * config.intervalModifier);
            data->interval = clampInterval(data->interval);
            data->due = now.addSecs(data->interval);
        } else if (ease == SM2_ANSWER_GOOD) {
            data->interval = (qint64)(data->interval * data->easeFactor);
            data->interval = clampInterval(data->interval);
            data->interval = (qint64)(data->interval * config.intervalModifier);
            data->interval = clampInterval(data->interval);
            data->due = now.addSecs(data->interval);
        } else if (ease == SM2_ANSWER_EASY) {
            data->interval = (qint64)(data->interval * data->easeFactor * (1.0 + config.easyBonus));
            data->easeFactor += 0.15;
            data->interval = clampInterval(data->interval);
            data->interval = (qint64)(data->interval * config.intervalModifier);
            data->interval = clampInterval(data->interval);
            data->due = now.addSecs(data->interval);
        }
    }
}

QString sm2Mode::makeKeyRev(quint64 noteId, uint deckId)
{
    return makeKey(noteId, deckId) + "_rev";
}

void sm2Mode::loop()
{
    parentUi->lineMiddleText->setVisible(false);

    QDateTime now = QDateTime::currentDateTime();

    QDate currentDate = now.date();
    if (todayDate.isValid() && todayDate != currentDate) {
        reviewsDoneToday = 0;
        newDoneToday = 0;
    }
    if (!todayDate.isValid()) todayDate = currentDate;

    QList<int> dueIndices;
    int firstNewIndex = -1;

    auto checkCard = [&](int cardIndex, bool reversed) {
        card& c = parent->currectSession.cardList[cardIndex];
        QString key = reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
        int encodedIdx = cardIndex * 2 + (reversed ? 1 : 0);

        if (!cardDataMap.contains(key)) {
            if (!reversed && firstNewIndex == -1) {
                firstNewIndex = encodedIdx;
            }
            return;
        }

        sm2CardData& data = cardDataMap[key];

        bool skipDue = false;
        if (data.leeched) {
            skipDue = (studyFilter != FILTER_LEECH);
        } else if (studyFilter == FILTER_LEECH) {
            skipDue = true;
        } else if (studyFilter == FILTER_NEW && data.state != SM2_NEW) {
            skipDue = true;
        } else if (studyFilter == FILTER_REVIEW && data.state != SM2_REVIEW) {
            skipDue = true;
        } else if (studyFilter == FILTER_FLAGGED && !data.flagged) {
            skipDue = true;
        }

        if (!skipDue && data.state == SM2_REVIEW) {
            if (config.maxReviewsPerDay > 0 && reviewsDoneToday >= config.maxReviewsPerDay && !cramMode) return;
            if (cramMode || (data.due.isValid() && data.due <= now)) {
                dueIndices.append(encodedIdx);
            }
        } else if (!skipDue && data.state == SM2_LEARNING) {
            if (cramMode || (data.due.isValid() && data.due <= now)) {
                dueIndices.append(encodedIdx);
            }
        }
    };

    for (int i = 0; i < parent->currectSession.cardList.count(); i++) {
        checkCard(i, false);
        if (config.reversedMode) {
            checkCard(i, true);
        }
    }

    if (!dueIndices.isEmpty()) {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(dueIndices.begin(), dueIndices.end(), rng);
        currentCardListIndex = dueIndices.first() / 2;
        currentIsReversed = (dueIndices.first() % 2) == 1;
    } else if (firstNewIndex >= 0) {
        currentCardListIndex = firstNewIndex / 2;
        currentIsReversed = (firstNewIndex % 2) == 1;
    } else {
        int fewestDaysAway = INT_MAX;
        int bestEncoded = 0;

        auto findClosest = [&](int cardIndex, bool reversed) {
            card& c = parent->currectSession.cardList[cardIndex];
            QString key = reversed ? makeKeyRev(c.id, c.deckiD) : makeKey(c.id, c.deckiD);
            if (cardDataMap.contains(key)) {
        sm2CardData& data = cardDataMap[key];

        if (data.suspended) return;
        if (data.buryUntil.isValid() && data.buryUntil >= now.date()) return;
                if (data.leeched) return;
                if (data.due.isValid() && data.due > now) {
                    int days = daysFromNow(data.due);
                    if (days < fewestDaysAway) {
                        fewestDaysAway = days;
                        bestEncoded = cardIndex * 2 + (reversed ? 1 : 0);
                    }
                }
            }
        };

        for (int i = 0; i < parent->currectSession.cardList.count(); i++) {
            findClosest(i, false);
            if (config.reversedMode) findClosest(i, true);
        }

        if (fewestDaysAway < INT_MAX) {
            int bestIndex = bestEncoded / 2;
            bool bestRev = (bestEncoded % 2) == 1;
            QString key = bestRev ? makeKeyRev(
                parent->currectSession.cardList[bestIndex].id,
                parent->currectSession.cardList[bestIndex].deckiD) :
                makeKey(parent->currectSession.cardList[bestIndex].id,
                        parent->currectSession.cardList[bestIndex].deckiD);
            QList<qint64> steps = parseLearningSteps();
            cardDataMap[key].due = now.addSecs(steps[0]);
            currentCardListIndex = bestIndex;
            currentIsReversed = bestRev;
        } else {
            currentCardListIndex = 0;
            currentIsReversed = false;
        }
    }

    card& choosenCard = parent->currectSession.cardList[currentCardListIndex];
    QString key = currentIsReversed ? makeKeyRev(choosenCard.id, choosenCard.deckiD)
                                  : makeKey(choosenCard.id, choosenCard.deckiD);
    if (!cardDataMap.contains(key)) {
        sm2CardData newData;
        newData.noteId = choosenCard.id;
        newData.deckId = choosenCard.deckiD;
        newData.easeFactor = config.startingEase;
        cardDataMap[key] = newData;
    }

    QString deckName = parent->currectSession.core.deckPathList[choosenCard.deckiD];
    deckName = deckName.split("/").last();
    if (deckName.length() > 20) {
        deckName = deckName.left(20);
        deckName.append("...");
    }
    parent->changeStatusBarTextSlot(deckName);

    choosenCard.count += 1;

    QString mainCard = cardExtract(&choosenCard, parent);
    correctMainCard(&mainCard, findMediaFile(&choosenCard, &parent->currectSession));
    splitMainCard(mainCard, &frontCard, &backCard, parent);
    processClozeDeletions(&frontCard, &backCard);

    // Swap front/back for reversed direction
    if (currentIsReversed) {
        std::swap(frontCard, backCard);
    }
    QString searchPath = directories::deckStorage.filePath(
        parent->currectSession.core.deckPathList[choosenCard.deckiD]);
    frontText->setSearchPaths(QStringList(searchPath));
    backText->setSearchPaths(QStringList(searchPath));

    parent->resetScrollState();
    backText->hide();
    parent->setText(frontText, frontCard);

    updateStatsLabel();
    updateCardInfo();
}

void sm2Mode::saveUndoSnap()
{
    card& c = parent->currectSession.cardList[currentCardListIndex];
    QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                  : makeKey(c.id, c.deckiD);
    if (cardDataMap.contains(key)) {
        undoSavedData = cardDataMap[key];
    } else {
        undoSavedData = sm2CardData();
        undoSavedData.noteId = c.id;
        undoSavedData.deckId = c.deckiD;
    }
    undoCardIndex = currentCardListIndex;
}

void sm2Mode::performUndo()
{
    if (!undoActive) return;
    undoActive = false;

    if (undoCardIndex >= 0 && undoCardIndex < parent->currectSession.cardList.count()) {
        card& c = parent->currectSession.cardList[undoCardIndex];
        QString key = makeKey(c.id, c.deckiD);
        cardDataMap[key] = undoSavedData;
        currentCardListIndex = undoCardIndex;

        QString mainCard = cardExtract(&c, parent);
        correctMainCard(&mainCard, findMediaFile(&c, &parent->currectSession));
        splitMainCard(mainCard, &frontCard, &backCard, parent);
        processClozeDeletions(&frontCard, &backCard);

        QString searchPath = directories::deckStorage.filePath(
            parent->currectSession.core.deckPathList[c.deckiD]);
        frontText->setSearchPaths(QStringList(searchPath));
        backText->setSearchPaths(QStringList(searchPath));

        parent->resetScrollState();
        backText->hide();
        parent->setText(frontText, frontCard);

        parentUi->lineMiddleText->setVisible(false);
        updateStatsLabel();
        updateCardInfo();
    }
}

void sm2Mode::againClicked()
{
    saveUndoSnap();
    undoGeneration++;
    int gen = undoGeneration;
    undoActive = true;
    QTimer::singleShot(5000, this, [this, gen]() {
        if (undoGeneration == gen) undoActive = false;
    });

    card& c = parent->currectSession.cardList[currentCardListIndex];
    QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                  : makeKey(c.id, c.deckiD);
    sm2CardData& data = cardDataMap[key];
    answerCard(&data, SM2_ANSWER_AGAIN);
    loop();
}

void sm2Mode::hardClicked()
{
    saveUndoSnap();
    undoGeneration++;
    int gen = undoGeneration;
    undoActive = true;
    QTimer::singleShot(5000, this, [this, gen]() {
        if (undoGeneration == gen) undoActive = false;
    });

    card& c = parent->currectSession.cardList[currentCardListIndex];
    QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                  : makeKey(c.id, c.deckiD);
    sm2CardData& data = cardDataMap[key];
    answerCard(&data, SM2_ANSWER_HARD);
    loop();
}

void sm2Mode::goodClicked()
{
    saveUndoSnap();
    undoGeneration++;
    int gen = undoGeneration;
    undoActive = true;
    QTimer::singleShot(5000, this, [this, gen]() {
        if (undoGeneration == gen) undoActive = false;
    });

    card& c = parent->currectSession.cardList[currentCardListIndex];
    QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                  : makeKey(c.id, c.deckiD);
    sm2CardData& data = cardDataMap[key];
    answerCard(&data, SM2_ANSWER_GOOD);
    loop();
}

void sm2Mode::easyClicked()
{
    saveUndoSnap();
    undoGeneration++;
    int gen = undoGeneration;
    undoActive = true;
    QTimer::singleShot(5000, this, [this, gen]() {
        if (undoGeneration == gen) undoActive = false;
    });

    card& c = parent->currectSession.cardList[currentCardListIndex];
    QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                  : makeKey(c.id, c.deckiD);
    sm2CardData& data = cardDataMap[key];
    answerCard(&data, SM2_ANSWER_EASY);
    loop();
}

void sm2Mode::showBack()
{
    if (undoActive && parentUi->lineMiddleText->isVisible() == false) {
        performUndo();
        return;
    }
    if (parentUi->lineMiddleText->isVisible() == false) {
        parentUi->lineMiddleText->setVisible(true);
        backText->show();
        parent->setText(backText, backCard);
    }
}

void sm2Mode::saveData()
{
    if (readyToSave == true) {
        QVariant configVariant = QVariant::fromValue(config);
        parent->saveSession->setValue("sm2Mode/config", configVariant);

        QMap<QString, QVariant> rawMap;
        for (auto it = cardDataMap.begin(); it != cardDataMap.end(); ++it) {
            rawMap[it.key()] = QVariant::fromValue(it.value());
        }
        parent->saveSession->setValue("sm2Mode/cardData", QVariant::fromValue(rawMap));
    }
}

void sm2Mode::receiveCall(QString call)
{
    if (call == "cram") {
        cramMode = !cramMode;
        if (cramMode) {
            qInfo() << "Cram mode ON — all review/learning cards are due now";
        } else {
            qInfo() << "Cram mode OFF — normal scheduling";
        }
    } else if (call == "filter") {
        studyFilter = static_cast<StudyFilter>((studyFilter + 1) % 5);
        QString names[] = {"All", "New only", "Review only", "Leeched only", "Flagged"};
        qInfo() << QString("Study filter: %1").arg(names[studyFilter]);
    } else if (call == "suspend") {
        if (currentCardListIndex >= 0) {
            card& c = parent->currectSession.cardList[currentCardListIndex];
            QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                          : makeKey(c.id, c.deckiD);
            if (cardDataMap.contains(key)) {
                cardDataMap[key].suspended = !cardDataMap[key].suspended;
                qInfo() << (cardDataMap[key].suspended ?
                    "Card suspended" : "Card unsuspended");
            }
        }
    } else if (call == "bury") {
        if (currentCardListIndex >= 0) {
            card& c = parent->currectSession.cardList[currentCardListIndex];
            QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                          : makeKey(c.id, c.deckiD);
            if (cardDataMap.contains(key)) {
                cardDataMap[key].buryUntil = QDate::currentDate();
                qInfo() << "Card buried until tomorrow";
            }
        }
    } else if (call == "flag") {
        if (currentCardListIndex >= 0) {
            card& c = parent->currectSession.cardList[currentCardListIndex];
            QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                          : makeKey(c.id, c.deckiD);
            if (cardDataMap.contains(key)) {
                cardDataMap[key].flagged = !cardDataMap[key].flagged;
                qInfo() << (cardDataMap[key].flagged ? "★ Card flagged" : "☆ Card unflagged");
            }
        }
    } else if (call == "minimal") {
        bool hidden = parentUi->cardStatsLabel->isVisible();
        parentUi->cardStatsLabel->setVisible(!hidden);
        qInfo() << (!hidden ? "Minimal mode ON" : "Minimal mode OFF");
    }
}

QString sm2Mode::filterName() const
{
    switch (studyFilter) {
        case FILTER_ALL: return "All";
        case FILTER_NEW: return "New";
        case FILTER_REVIEW: return "Review";
        case FILTER_LEECH: return "Leech";
        case FILTER_FLAGGED: return "Flagged";
    }
    return "All";
}

void sm2Mode::showSessionSummary()
{
    int total = sessionAgain + sessionHard + sessionGood + sessionEasy;
    if (total == 0) return;

    qint64 elapsed = QDateTime::currentSecsSinceEpoch() - sessionStartTime;
    int minutes = elapsed / 60;
    int seconds = elapsed % 60;

    QString summary;
    summary += "<b>Session Summary</b><br><br>";
    summary += QString("Cards studied: <b>%1</b><br>").arg(total);
    summary += QString("Time: %1m %2s<br><br>").arg(minutes).arg(seconds);

    summary += "<b>Answers:</b><br>";
    if (sessionAgain > 0) summary += QString("Again: %1<br>").arg(sessionAgain);
    if (sessionHard > 0) summary += QString("Hard: %1<br>").arg(sessionHard);
    summary += QString("Good: %1<br>").arg(sessionGood);
    if (sessionEasy > 0) summary += QString("Easy: %1<br>").arg(sessionEasy);

    if (sessionGood > 0) {
        int rate = (sessionGood + sessionEasy) * 100 / total;
        summary += QString("Retention: %1%<br>").arg(rate);
    }

    if (sessionLeeched > 0) {
        summary += QString("<br><b>⚠ New leeches: %1</b><br>").arg(sessionLeeched);
    }

    qInfo() << summary;
}

void sm2Mode::updateCardInfo()
{
    if (currentCardListIndex < 0 || currentCardListIndex >= parent->currectSession.cardList.count()) return;

    card& c = parent->currectSession.cardList[currentCardListIndex];
    QString key = currentIsReversed ? makeKeyRev(c.id, c.deckiD)
                                  : makeKey(c.id, c.deckiD);
    if (!cardDataMap.contains(key)) return;

    sm2CardData& data = cardDataMap[key];
    QString info;

    if (data.leeched) {
        info = "LEECHED";
    } else if (data.state == SM2_NEW) {
        info = "New card";
    } else if (data.state == SM2_LEARNING) {
        QList<qint64> steps = parseLearningSteps();
        info = QString("Learn %1/%2")
                   .arg(data.stepIndex + 1).arg(steps.size());
    } else if (data.state == SM2_REVIEW) {
        info = QString("Ease: %1% | Int: ")
                   .arg(data.easeFactor * 100, 0, 'f', 0);

        qint64 secs = data.interval;
        if (secs < 3600) info += QString("%1m").arg(secs / 60);
        else if (secs < 86400) info += QString("%1h").arg(secs / 3600);
        else if (secs < 2592000) info += QString("%1d").arg(secs / 86400);
        else info += QString("%1mo").arg(secs / 2592000);

        if (data.lapses > 0) {
            info += QString(" | Lapses: %1").arg(data.lapses);
        }
    }

    QString current = parentUi->cardStatsLabel->text();
    int brPos = current.indexOf("<br>");
    if (brPos >= 0) current = current.left(brPos);
    if (!info.isEmpty()) current += "<br>" + info;
    parentUi->cardStatsLabel->setText(current);
}

void sm2Mode::updateStatsLabel()
{
    newCardsToday = 0;
    reviewCardsToday = 0;
    totalNew = 0;
    totalLearning = 0;
    totalReview = 0;

    int totalLeech = 0;

    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < parent->currectSession.cardList.count(); i++) {
        card& c = parent->currectSession.cardList[i];
        QString key = makeKey(c.id, c.deckiD);
        if (!cardDataMap.contains(key)) {
            totalNew++;
        } else {
            sm2CardData& data = cardDataMap[key];
            if (data.leeched) { totalLeech++; continue; }
            if (data.state == SM2_LEARNING) totalLearning++;
            else if (data.state == SM2_REVIEW) totalReview++;
            else totalNew++;
        }
    }

    QString html = QString("New: %1 | Learn: %2 | Review: %3")
                       .arg(totalNew).arg(totalLearning).arg(totalReview);
    if (studyFilter != FILTER_ALL) {
        html += QString(" | [%1]").arg(filterName());
    }
    if (cramMode) {
        html += " | <b>CRAM</b>";
    }
    if (config.maxReviewsPerDay > 0) {
        html += QString(" | Done: %1/%2").arg(reviewsDoneToday).arg(config.maxReviewsPerDay);
    }
    if (totalLeech > 0) {
        html += QString(" | Leech: %1").arg(totalLeech);
    }
    parentUi->cardStatsLabel->setText(html);
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

QDebug operator<<(QDebug dbg, const sm2CardData& c)
{
    dbg.nospace() << "sm2CardData(noteId=" << c.noteId << ", deckId=" << c.deckId
                  << ", ease=" << c.easeFactor << ", interval=" << c.interval
                  << ", state=" << c.state << ", due=" << c.due << ")";
    return dbg.space();
}

QDataStream& operator<<(QDataStream& out, const sm2Config& v)
{
    out << v.learningSteps << v.graduatingInterval << v.easyInterval
        << v.startingEase << v.easyBonus << v.intervalModifier
        << v.hardIntervalMultiplier << v.newIntervalFactor
        << v.maxInterval << v.minInterval << v.leechThreshold
        << v.maxReviewsPerDay << v.reversedMode
        << v.twoButtonMode;
    return out;
}

QDataStream& operator>>(QDataStream& in, sm2Config& v)
{
    in >> v.learningSteps >> v.graduatingInterval >> v.easyInterval
       >> v.startingEase >> v.easyBonus >> v.intervalModifier
       >> v.hardIntervalMultiplier >> v.newIntervalFactor
       >> v.maxInterval >> v.minInterval >> v.leechThreshold
       >> v.maxReviewsPerDay >> v.reversedMode
       >> v.twoButtonMode;
    return in;
}

QDebug operator<<(QDebug dbg, const sm2Config& c)
{
    dbg.nospace() << "sm2Config(steps=" << c.learningSteps
                  << ", gradInterval=" << c.graduatingInterval
                  << ", easyInterval=" << c.easyInterval << ")";
    return dbg.space();
}
