#include "global.h"
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <stdio.h>
#include <array>
#include <memory>

bool debugEnabled = false;
bool warningsEnabled = true;
bool pc = true;
bool ereader = false;
bool grender = false;
void* graphic_null = nullptr;
bool disableFlashingEverywhere = false;
bool flashing = true;
QString deckAddedFileName = "creationTime";

namespace directories {
QDir config = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
              + QDir::separator() + "sanki";
QDir deckStorage = config.path() + QDir::separator() + "decks";
QDir sessionSaves = config.path() + QDir::separator() + "sessions";
QDir fileSelect = QDir::homePath();
QFile globalSettings = config.filePath("sanki.ini");
}

namespace ereaderVars {
bool inkboxUserApp = false;
bool nickelApp = false;
QString buttonNoFlashStylesheet;
int screenX = 80;
int screenY = 24;
bool playWaveFormSettings = false;
}

bool renameDir(QDir& dir, const QString& newName)
{
    auto src = QDir::cleanPath(dir.filePath("."));
    auto dst = QDir::cleanPath(
        dir.filePath(QStringLiteral("..%1%2").arg(QDir::separator()).arg(newName)));
    auto rc = QFile::rename(src, dst);
    if (rc) dir.setPath(dst);
    return rc;
}

void checkEreaderModel() { ereader = false; }

void screenGeometry() { /* no-op in TUI */ }

int checkBatteryLevel() { return -1; }

void setWhiteBrightnessAlias(int) { }

int getWhiteBrightnessAlias() { return -1; }

QString exec(const char* cmd)
{
    std::array<char, 128> buffer;
    QString result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return QString();
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result.trimmed();
}

bool createDir(QString absolutePath)
{
    if (!QDir(absolutePath).exists()) {
        return QDir::root().mkpath(absolutePath);
    }
    return true;
}

void refreshRect(int, int, int, int) { /* no-op in TUI */ }

void loadWaveFormSetting() { /* no-op in TUI */ }
