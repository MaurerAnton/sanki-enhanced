#include "tui_utils.h"
#include <QRegularExpression>
#include <QFile>
#include <QTime>

QString stripHtml(const QString& html)
{
    QString text = html;
    text.replace(QRegularExpression("<br\\s*/?>"), "\n");
    text.replace(QRegularExpression("<[^>]*>"), "");
    text.replace("&nbsp;", " ");
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&quot;", "\"");
    text.replace("&#39;", "'");
    return text.trimmed();
}

std::string qstrToStd(const QString& qstr)
{
    return qstr.toStdString();
}

QString stdToQstr(const std::string& str)
{
    return QString::fromStdString(str);
}

std::string truncate(const std::string& str, size_t maxLen)
{
    if (str.length() <= maxLen) return str;
    return str.substr(0, maxLen - 3) + "...";
}

std::string padRight(const std::string& str, size_t width)
{
    if (str.length() >= width) return str;
    return str + std::string(width - str.length(), ' ');
}

std::string formatTime(quint64 ms)
{
    QTime t = QTime::fromString("0:0:0", "h:m:s");
    t = t.addMSecs(ms);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.hour(), t.minute(), t.second());
    return std::string(buf);
}

std::string modeToString(DeckModes mode)
{
    switch (mode) {
    case CompletlyRandomised: return "Random";
    case RandomisedNoRepeating: return "NoRepeat";
    case Boxes: return "Boxes";
    case SM2: return "SM-2";
    default: return "None";
    }
}

bool fileExists(const QString& path)
{
    return QFile::exists(path);
}
