#ifndef GLOBALS_H
#define GLOBALS_H

#include <QDir>
#include <QString>
#include <QObject>
#include <QFile>

extern bool debugEnabled;
extern bool warningsEnabled;

extern bool pc;
extern bool ereader;

extern bool grender;
// In TUI mode we don't have qGraphicsViewEvents, use void* instead
extern void* graphic_null;

extern bool disableFlashingEverywhere;
extern bool flashing;

namespace directories {
    extern QDir config;
    extern QDir deckStorage;
    extern QDir sessionSaves;
    extern QDir fileSelect;
    extern QFile globalSettings;
}
extern QString deckAddedFileName;

namespace ereaderVars {
    extern bool inkboxUserApp;
    extern bool nickelApp;
    extern QString buttonNoFlashStylesheet;
    extern int screenX;
    extern int screenY;
    extern bool playWaveFormSettings;
}

enum DeckModes {
    None,
    CompletlyRandomised,
    RandomisedNoRepeating,
    Boxes,
    SM2,
};
Q_DECLARE_METATYPE(DeckModes)

bool renameDir(QDir & dir, const QString & newName);
void checkEreaderModel();
void screenGeometry();
int checkBatteryLevel();
void setWhiteBrightnessAlias(int value);
int getWhiteBrightnessAlias();
QString exec(const char *cmd);
bool createDir(QString absolutePath);
void refreshRect(int, int, int, int);
void loadWaveFormSetting();

#endif // GLOBALS_H
