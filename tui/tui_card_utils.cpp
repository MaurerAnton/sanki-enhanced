#include "tui_card_utils.h"
#include "global.h"
#include <QSqlQuery>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QDebug>

QString cardExtractTui(card* acard, QList<QSqlDatabase>& databases)
{
    if (acard->deckiD >= (uint)databases.size()) return "";
    QSqlQuery query("SELECT flds FROM notes WHERE id = :id", databases[acard->deckiD]);
    query.bindValue(0, acard->id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "";
}

QString findDatabaseFileTui(QDir deckPath)
{
    QFile collection21 = deckPath.filePath("collection.anki21");
    QFile collection2 = deckPath.filePath("collection.anki2");
    if (collection21.exists()) return collection21.fileName();
    if (collection2.exists()) return collection2.fileName();
    return "none";
}

QString findMediaFileTui(card* acard, sessionStr* session)
{
    QDir baseDir(directories::deckStorage.filePath(session->core.deckPathList[acard->deckiD]));
    QString mediaFile = baseDir.filePath("media");
    if (QFile(mediaFile).exists()) return mediaFile;
    return QString();
}

void splitMainCardTui(QString mainCard, QString* frontCard, QString* backCard, bool reversed)
{
    QStringList cards = mainCard.split("\u001F");
    if (!reversed) {
        *frontCard = cards.first();
        *backCard = (cards.size() > 1) ? cards.last() : "";
    } else {
        *frontCard = (cards.size() > 1) ? cards.last() : "";
        *backCard = cards.first();
    }
}

// The following are copies from helperFunctions.cpp to avoid QtWidgets dependency

void correctMainCard(QString* mainCard, const QString& mediaPath)
{
    if (mainCard->contains("<img src=")) {
        QFile mediaFile(mediaPath);
        mediaFile.open(QIODevice::ReadOnly);
        QString mediaContent = mediaFile.readAll();
        mediaFile.close();
        mediaContent.replace("{", "").replace("}", "");
        mediaContent.replace(": ", ":").replace(", ", ",");
        QStringList split_dot = mediaContent.split(",");
        for (const QString& item : split_dot) {
            QString clean = item;
            clean.replace("\"", "");
            QStringList replace_items = clean.split(":");
            if (mainCard->contains(replace_items.last())) {
                mainCard->replace(replace_items.last(), replace_items.first());
            }
        }
    }
    mainCard->replace("&nbsp;", " ");
}

bool hasClozeDeletions(const QString& text)
{
    static QRegularExpression re("\\{\\{c\\d+::");
    return re.match(text).hasMatch();
}

void processClozeDeletions(QString* frontCard, QString* backCard)
{
    if (!hasClozeDeletions(*frontCard)) return;
    QString original = *frontCard;
    QString extra = *backCard;

    QRegularExpression reHint("\\{\\{c\\d+::(.+?)::(.+?)\\}\\}",
                              QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression reSimple("\\{\\{c\\d+::(.+?)\\}\\}",
                                QRegularExpression::DotMatchesEverythingOption);

    QString frontResult = original;
    frontResult.replace(reHint, "[\\1]");
    frontResult.replace(reSimple, "[...]");

    QString backResult = original;
    backResult.replace(reHint, "\\2");
    backResult.replace(reSimple, "\\1");

    if (!extra.isEmpty()) backResult += "\n---\n" + extra;
    *frontCard = frontResult;
    *backCard = backResult;
}

QString adjustImgSize(uint width, QString text)
{
    static QRegularExpression pattern("\\b(?:width|height)\\s*=\\s*\"\\d+\"\\s*");
    text.remove(pattern);
    QString sizes = "<img width=\"" + QString::number(width) + "\"";
    text.replace("<img", sizes);
    return text;
}

uint randomValue(uint min, uint max)
{
    return QRandomGenerator::global()->generate() % (max - min + 1) + min;
}
