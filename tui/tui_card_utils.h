#ifndef TUI_CARD_UTILS_H
#define TUI_CARD_UTILS_H

#include "mainMenu/sessions/sessionStruct.h"
#include <QString>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>

// Card extraction without QtWidgets dependency
QString cardExtractTui(card* acard, QList<QSqlDatabase>& databases);
QString findDatabaseFileTui(QDir deckPath);
QString findMediaFileTui(card* acard, sessionStr* session);
void splitMainCardTui(QString mainCard, QString* frontCard, QString* backCard, bool reversed);

// From helperFunctions.h - already portable
void correctMainCard(QString* mainCard, const QString& mediaPath);
bool hasClozeDeletions(const QString& text);
void processClozeDeletions(QString* frontCard, QString* backCard);
QString adjustImgSize(uint width, QString text);
uint randomValue(uint min, uint max);

#endif // TUI_CARD_UTILS_H
