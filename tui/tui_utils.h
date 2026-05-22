#ifndef TUI_UTILS_H
#define TUI_UTILS_H

#include "global.h"

#include <QString>
#include <QStringList>
#include <vector>
#include <string>

QString stripHtml(const QString& html);
std::string qstrToStd(const QString& qstr);
QString stdToQstr(const std::string& str);
std::string truncate(const std::string& str, size_t maxLen);
std::string padRight(const std::string& str, size_t width);
std::string formatTime(quint64 ms);
std::string modeToString(DeckModes mode);
bool fileExists(const QString& path);

#endif // TUI_UTILS_H
