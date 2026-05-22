#include "components/other/cardBrowser.h"
#include "ui_cardBrowser.h"
#include "global.h"
#include "cardView/functions/helperFunctions.h"
#include "components/other/cardEditor.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QHeaderView>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QDateTime>
#include <QTimer>

cardBrowser::cardBrowser(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::cardBrowser)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);

    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->editButton->setEnabled(false);

    if (ereader) {
        ui->tableWidget->setStyleSheet(
            "QTableWidget { font-size: 7pt; } "
            "QHeaderView::section { font-size: 7pt; padding: 4px; }");
    }
}

cardBrowser::~cardBrowser()
{
    for (int i = 0; i < databases.count(); i++) {
        databases[i].close();
        QSqlDatabase::removeDatabase(QString("browser_%1").arg(i));
    }
    delete ui;
}

QString cardBrowser::stripHtml(const QString& html)
{
    QString text = html;
    text.replace(QRegularExpression("<[^>]*>"), "");
    text.replace("&nbsp;", " ");
    text.replace("&gt;", ">");
    text.replace("&lt;", "<");
    text.replace("&amp;", "&");
    return text.trimmed().left(80);
}

QString cardBrowser::stateName(int state)
{
    switch (state) {
        case SM2_NEW: return "New";
        case SM2_LEARNING: return "Learn";
        case SM2_REVIEW: return "Review";
        default: return "-";
    }
}

QString cardBrowser::formatDue(const QDateTime& due, int state)
{
    if (state == SM2_NEW || !due.isValid()) return "-";
    qint64 secs = QDateTime::currentDateTime().secsTo(due);
    if (secs <= 0) return "now";
    if (secs < 3600) return QString("%1m").arg(secs / 60);
    if (secs < 86400) return QString("%1h").arg(secs / 3600);
    return QString("%1d").arg(secs / 86400);
}

QString cardBrowser::formatInterval(qint64 seconds)
{
    if (seconds == 0) return "-";
    if (seconds < 3600) return QString("%1m").arg(seconds / 60);
    if (seconds < 86400) return QString("%1h").arg(seconds / 3600);
    double days = seconds / 86400.0;
    if (days < 30) return QString("%1d").arg((int)days);
    if (days < 365) return QString("%1mo").arg((int)(days / 30));
    return QString("%1y").arg(days / 365.0, 0, 'f', 1);
}

void cardBrowser::loadSm2Data()
{
    sm2CardMap.clear();
    hasSm2Data = false;

    QSettings sessionSettings(
        directories::sessionSaves.filePath(currentSession.core.name),
        QSettings::IniFormat);

    QVariant sm2Raw = sessionSettings.value("sm2Mode/cardData");
    if (!sm2Raw.isValid() || sm2Raw.isNull()) return;

    QMap<QString, QVariant> rawMap = sm2Raw.toMap();
    for (auto it = rawMap.begin(); it != rawMap.end(); ++it) {
        sm2CardMap[it.key()] = it.value().value<sm2CardData>();
    }

    hasSm2Data = true;
}

void cardBrowser::loadTags()
{
    tagMap.clear();
    for (int i = 0; i < databases.count(); i++) {
        QSqlQuery query(databases[i]);
        query.exec("SELECT id, tags FROM notes WHERE tags != ''");
        while (query.next()) {
            QString key = QString::number(query.value(0).toULongLong())
                          + "_" + QString::number(i);
            tagMap[key] = query.value(1).toString();
        }
    }
}

void cardBrowser::start(sessionStr session)
{
    currentSession = session;

    for (int i = 0; i < session.core.deckPathList.count(); i++) {
        QString dir = session.core.deckPathList[i];
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE",
            QString("browser_%1").arg(i));

        QString databaseFile = findDatabaseFile(
            directories::deckStorage.filePath(dir));
        if (databaseFile == "none") continue;

        db.setDatabaseName(databaseFile);
        if (!db.open()) continue;
        databases.append(db);
    }

    loadTags();
    loadSm2Data();

    ui->tagCombo->clear();
    ui->tagCombo->addItem("All tags", "");
    QSet<QString> uniqueTags;
    for (auto it = tagMap.begin(); it != tagMap.end(); ++it) {
        QStringList tagList = it.value().split(" ", Qt::SkipEmptyParts);
        for (const QString& t : tagList) {
            QString cleanTag = t.trimmed();
            if (!cleanTag.isEmpty()) uniqueTags.insert(cleanTag);
        }
    }
    QStringList sortedTags = uniqueTags.values();
    sortedTags.sort();
    for (const QString& t : sortedTags) {
        ui->tagCombo->addItem(t, t);
    }

    loadCards();
}

void cardBrowser::loadCards(const QString& filter, const QString& tagFilter)
{
    allCards.clear();
    ui->tableWidget->setRowCount(0);
    currentTagFilter = tagFilter;

    for (int i = 0; i < databases.count(); i++) {
        QString sql = "SELECT id, flds, tags FROM notes WHERE 1=1";
        if (!filter.isEmpty()) {
            sql += " AND (flds LIKE '%" + filter + "%' OR sfld LIKE '%" + filter + "%')";
        }
        if (!tagFilter.isEmpty()) {
            sql += " AND (tags LIKE '%" + tagFilter + "%')";
        }
        sql += " ORDER BY id";

        QSqlQuery query(databases[i]);
        query.exec(sql);
        while (query.next()) {
            cardRow row;
            row.noteId = query.value(0).toULongLong();
            row.deckId = i;
            QString flds = query.value(1).toString();

            QStringList parts = flds.split("\u001F");
            row.front = parts.value(0);
            row.back = parts.value(1);
            row.flds = flds;
            row.tags = query.value(2).toString();

            QString sm2Key = QString::number(row.noteId) + "_"
                             + QString::number(row.deckId);
            if (hasSm2Data && sm2CardMap.contains(sm2Key)) {
                sm2CardData& d = sm2CardMap[sm2Key];
                row.sm2State = d.state;
                row.sm2Ease = d.easeFactor;
                row.sm2Interval = d.interval;
                row.sm2Due = d.due;
                row.sm2Lapses = d.lapses;
                row.sm2Leeched = d.leeched;
                row.sm2Suspended = d.suspended;
                row.sm2Flagged = d.flagged;
            }

            QString tagKey = QString::number(row.noteId) + "_"
                             + QString::number(row.deckId);
            row.tags = tagMap.value(tagKey, "");

            allCards.append(row);
        }
    }

    QStringList headers;
    headers << "Front" << "State" << "Due" << "Ease" << "Lapses" << "Tags";

    ui->tableWidget->setColumnCount(headers.count());
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->setColumnWidth(0, 220);
    ui->tableWidget->setColumnWidth(1, 55);
    ui->tableWidget->setColumnWidth(2, 55);
    ui->tableWidget->setColumnWidth(3, 50);
    ui->tableWidget->setColumnWidth(4, 55);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

    if (!hasSm2Data) {
        ui->tableWidget->setColumnHidden(1, true);
        ui->tableWidget->setColumnHidden(2, true);
        ui->tableWidget->setColumnHidden(3, true);
        ui->tableWidget->setColumnHidden(4, true);
    }

    ui->tableWidget->setRowCount(allCards.count());
    for (int r = 0; r < allCards.count(); r++) {
        QString frontText = stripHtml(allCards[r].front);
        if (allCards[r].sm2Flagged) {
            frontText = "★ " + frontText;
        }
        if (allCards[r].sm2Leeched) {
            frontText = "[L] " + frontText;
        }

        ui->tableWidget->setItem(r, 0,
            new QTableWidgetItem(frontText));

        if (hasSm2Data) {
            ui->tableWidget->setItem(r, 1,
                new QTableWidgetItem(stateName(allCards[r].sm2State)));
            ui->tableWidget->setItem(r, 2,
                new QTableWidgetItem(
                    allCards[r].sm2Leeched ? "leeched" :
                        formatDue(allCards[r].sm2Due, allCards[r].sm2State)));
            ui->tableWidget->setItem(r, 3,
                new QTableWidgetItem(
                    allCards[r].sm2Ease > 0 ?
                        QString::number(allCards[r].sm2Ease * 100, 'f', 0) + "%" : "-"));
            ui->tableWidget->setItem(r, 4,
                new QTableWidgetItem(
                    allCards[r].sm2State >= SM2_LEARNING ?
                        QString::number(allCards[r].sm2Lapses) : "-"));
            ui->tableWidget->setItem(r, 5,
                new QTableWidgetItem(allCards[r].tags));
        } else {
            ui->tableWidget->setItem(r, 5,
                new QTableWidgetItem(allCards[r].tags));
        }
    }

    selectedRow = -1;
    ui->editButton->setEnabled(false);
    ui->resetButton->setEnabled(false);
    ui->suspendButton->setEnabled(false);
    ui->flagButton->setEnabled(false);

    QString stats;
    stats += QString("Total: %1").arg(allCards.count());
    if (hasSm2Data) {
        int newCnt = 0, learnCnt = 0, reviewCnt = 0, leechCnt = 0;
        for (auto& cr : allCards) {
            if (cr.sm2Leeched) { leechCnt++; continue; }
            if (cr.sm2State == SM2_NEW) newCnt++;
            else if (cr.sm2State == SM2_LEARNING) learnCnt++;
            else if (cr.sm2State == SM2_REVIEW) reviewCnt++;
            else newCnt++;
        }
        stats += QString(" | New: %1  Learn: %2  Review: %3").arg(newCnt)
                 .arg(learnCnt).arg(reviewCnt);
        if (leechCnt > 0) stats += QString("  Leech: %1").arg(leechCnt);
    }
    ui->statsLabel->setText(stats);
}

void cardBrowser::on_searchEdit_textChanged(const QString &arg1)
{
    loadCards(arg1, currentTagFilter);
}

void cardBrowser::on_tagCombo_currentIndexChanged(int index)
{
    QString tag = ui->tagCombo->itemData(index).toString();
    loadCards(ui->searchEdit->text(), tag);
}

void cardBrowser::on_tableWidget_cellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= allCards.count()) return;
    selectedRow = row;
    ui->editButton->setEnabled(true);
    ui->resetButton->setEnabled(hasSm2Data);
    ui->suspendButton->setEnabled(hasSm2Data);
    ui->flagButton->setEnabled(hasSm2Data);

    cardRow& cr = allCards[row];

    QString info;
    info += QString("<b>Card %1</b><br><br>").arg(cr.noteId);

    QString frontClean = stripHtml(cr.front);
    info += "<b>Front:</b><br>" + frontClean + "<br><br>";

    if (hasSm2Data && cr.sm2State >= 0) {
        info += "<b>SM-2 Info:</b><br>";
        info += "State: " + stateName(cr.sm2State) + "<br>";
        if (cr.sm2Leeched) {
            info += "Status: <b>LEECHED</b><br>";
        }
        info += "Ease factor: " + QString::number(cr.sm2Ease * 100, 'f', 0) + "%<br>";
        info += "Interval: " + formatInterval(cr.sm2Interval) + "<br>";
        info += "Due: " + formatDue(cr.sm2Due, cr.sm2State) + "<br>";
        info += "Lapses: " + QString::number(cr.sm2Lapses) + "<br>";
    }

    if (!cr.tags.isEmpty()) {
        info += "<br><b>Tags:</b> " + cr.tags + "<br>";
    }

    QString deckName = currentSession.core.deckPathList
        .value(cr.deckId).split("/").last();
    info += "<br><b>Deck:</b> " + deckName;

    qInfo() << info;
}

void cardBrowser::on_resetButton_clicked()
{
    if (selectedRow < 0 || selectedRow >= allCards.count()) return;
    if (!hasSm2Data) return;

    cardRow& cr = allCards[selectedRow];
    QString sm2Key = QString::number(cr.noteId) + "_" + QString::number(cr.deckId);

    if (!sm2CardMap.contains(sm2Key)) {
        sm2Key += "_rev";
        if (!sm2CardMap.contains(sm2Key)) return;
    }

    sm2CardMap.remove(sm2Key);

    QSettings sessionSettings(
        directories::sessionSaves.filePath(currentSession.core.name),
        QSettings::IniFormat);

    QMap<QString, QVariant> rawMap;
    for (auto it = sm2CardMap.begin(); it != sm2CardMap.end(); ++it) {
        rawMap[it.key()] = QVariant::fromValue(it.value());
    }
    sessionSettings.setValue("sm2Mode/cardData", QVariant::fromValue(rawMap));

    loadCards(ui->searchEdit->text(), currentTagFilter);
}

void cardBrowser::on_suspendButton_clicked()
{
    if (selectedRow < 0 || selectedRow >= allCards.count()) return;
    if (!hasSm2Data) return;

    cardRow& cr = allCards[selectedRow];
    QString sm2Key = QString::number(cr.noteId) + "_" + QString::number(cr.deckId);

    if (!sm2CardMap.contains(sm2Key)) {
        sm2Key += "_rev";
        if (!sm2CardMap.contains(sm2Key)) return;
    }

    sm2CardMap[sm2Key].suspended = !sm2CardMap[sm2Key].suspended;

    QSettings sessionSettings(
        directories::sessionSaves.filePath(currentSession.core.name),
        QSettings::IniFormat);

    QMap<QString, QVariant> rawMap;
    for (auto it = sm2CardMap.begin(); it != sm2CardMap.end(); ++it) {
        rawMap[it.key()] = QVariant::fromValue(it.value());
    }
    sessionSettings.setValue("sm2Mode/cardData", QVariant::fromValue(rawMap));

    loadCards(ui->searchEdit->text(), currentTagFilter);
}

void cardBrowser::on_flagButton_clicked()
{
    if (selectedRow < 0 || selectedRow >= allCards.count()) return;
    if (!hasSm2Data) return;

    cardRow& cr = allCards[selectedRow];
    QString sm2Key = QString::number(cr.noteId) + "_" + QString::number(cr.deckId);

    if (!sm2CardMap.contains(sm2Key)) {
        sm2Key += "_rev";
        if (!sm2CardMap.contains(sm2Key)) return;
    }

    sm2CardMap[sm2Key].flagged = !sm2CardMap[sm2Key].flagged;

    QSettings sessionSettings(
        directories::sessionSaves.filePath(currentSession.core.name),
        QSettings::IniFormat);

    QMap<QString, QVariant> rawMap;
    for (auto it = sm2CardMap.begin(); it != sm2CardMap.end(); ++it) {
        rawMap[it.key()] = QVariant::fromValue(it.value());
    }
    sessionSettings.setValue("sm2Mode/cardData", QVariant::fromValue(rawMap));

    loadCards(ui->searchEdit->text(), currentTagFilter);
}

void cardBrowser::on_editButton_clicked()
{
    if (selectedRow < 0 || selectedRow >= allCards.count()) return;

    cardRow& cr = allCards[selectedRow];
    cardEditor* editor = new cardEditor(this);
    if (ereader) {
        editor->show();
    }
    editor->editCard(cr.noteId, cr.deckId, cr.flds);
    editor->exec();

    loadCards(ui->searchEdit->text());
}

void cardBrowser::on_closeButton_clicked()
{
    this->accept();
}
