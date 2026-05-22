#include "tui_menu.h"
#include "tui_app.h"
#include "tui_utils.h"
#include "tui_card_utils.h"
#include "tui_modes.h"
#include "global.h"

#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/event.hpp>
#include <algorithm>
#include <QProcess>

namespace t = ftxui;

static std::vector<std::string> g_sessionNames;
static std::vector<std::string> g_deckNames;

TuiMenu::TuiMenu() {}

void TuiMenu::init(TuiApp* app) { this->app = app; loadSessions(); loadDecks(); buildComponent(); }
void TuiMenu::refresh() { loadSessions(); loadDecks(); buildComponent(); }

void TuiMenu::loadSessions()
{
    sessions.clear();
    QDir dir(directories::sessionSaves);
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& file : files) {
        QSettings s(file.absoluteFilePath(), QSettings::IniFormat);
        QVariant var = s.value("session");
        if (var.isValid() && !var.isNull()) {
            SessionInfo info; info.filename = file.fileName(); info.data = var.value<sessionStr>();
            sessions.push_back(info);
        }
    }
}

void TuiMenu::loadDecks()
{
    decks.clear();
    QDir dir(directories::deckStorage);
    for (const QString& subdir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        DeckInfo info; info.name = subdir; info.path = directories::deckStorage.filePath(subdir);
        QString dbFile = findDatabaseFileTui(info.path);
        if (dbFile != "none") {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "deckmenu_" + subdir);
            db.setDatabaseName(dbFile);
            if (db.open()) { QSqlQuery q("SELECT COUNT(id) FROM notes", db); if (q.exec() && q.next()) info.cardCount = q.value(0).toInt(); db.close(); }
            QSqlDatabase::removeDatabase("deckmenu_" + subdir);
        }
        decks.push_back(info);
    }
}

void TuiMenu::deleteSession(const QString& filename)
{
    QFile::remove(directories::sessionSaves.filePath(filename));
    statusMessage = "Session deleted: " + qstrToStd(filename);
    refresh();
}

void TuiMenu::importDeck()
{
    if (!importPath.empty()) {
        QString path = stdToQstr(importPath);
        QFile zipFile(path);
        if (!zipFile.exists()) { statusMessage = "File not found: " + importPath; importPath.clear(); inputtingImport = false; return; }

        QFileInfo info(path);
        QString deckName = info.baseName();
        QString destPath = directories::deckStorage.filePath(deckName);

        if (QDir(destPath).exists()) { statusMessage = "Deck already exists: " + qstrToStd(deckName); importPath.clear(); inputtingImport = false; return; }

        directories::deckStorage.mkdir(deckName);

        QProcess unzip;
        unzip.start("unzip", QStringList() << "-o" << path << "-d" << destPath);
        unzip.waitForFinished(30000);

        if (unzip.exitCode() == 0) {
            QFile ctf(QDir(destPath).filePath(deckAddedFileName));
            ctf.open(QIODevice::WriteOnly);
            ctf.write(QDateTime::currentDateTime().toString("dd.MM.yyyy - hh:mm").toUtf8());
            ctf.close();
            statusMessage = "Imported: " + qstrToStd(deckName);
        } else {
            QString err = unzip.readAllStandardError();
            statusMessage = "Import error: " + qstrToStd(err).substr(0, 50);
        }
        importPath.clear(); inputtingImport = false;
        refresh();
    } else {
        inputtingImport = true;
        importPath = "";
        statusMessage = "Enter path to .apkg file, then Enter:";
    }
}

void TuiMenu::editSession()
{
    if (sessionEditActive) {
        if (sessionSelected >= 0 && sessionSelected < (int)sessions.size()) {
            auto& si = sessions[sessionSelected];
            if (!editSessionName.empty()) {
                QString oldPath = directories::sessionSaves.filePath(si.filename);
                QString newName = stdToQstr(editSessionName);
                si.data.core.name = newName;
                si.data.core.mode = (editModeIndex == 0) ? CompletlyRandomised : (editModeIndex == 1) ? Boxes : SM2;
                QSettings ns(directories::sessionSaves.filePath(newName), QSettings::IniFormat);
                ns.setValue("session", QVariant::fromValue(si.data));
                ns.sync();
                if (si.filename != newName) QFile::remove(oldPath);
                statusMessage = "Session updated: " + editSessionName;
                refresh();
            }
        }
        sessionEditActive = false; inputtingSessionEdit = false;
    } else {
        if (sessionSelected >= 0 && sessionSelected < (int)sessions.size()) {
            auto& si = sessions[sessionSelected];
            editSessionName = qstrToStd(si.data.core.name);
            editModeIndex = (si.data.core.mode == CompletlyRandomised) ? 0 : (si.data.core.mode == Boxes) ? 1 : 2;
            sessionEditActive = true;
            inputtingSessionEdit = false;
            statusMessage = "Edit session — [r] name, [1/2/3] mode, [Enter] save, [Esc] cancel";
        }
    }
}

void TuiMenu::deleteDeck()
{
    if (deckSelected >= 0 && deckSelected < (int)decks.size()) {
        auto& di = decks[deckSelected];
        QDir d(di.path);
        if (d.removeRecursively()) { statusMessage = "Deck deleted: " + qstrToStd(di.name); refresh(); }
        else { statusMessage = "Failed to delete deck: " + qstrToStd(di.name); }
    }
}

// ─── Due Forecast ───────────────────────────────────────────────────────────

std::string TuiMenu::getDueForecast()
{
    if (sessionSelected < 0 || sessionSelected >= (int)sessions.size()) return "";
    auto& si = sessions[sessionSelected];
    if (si.data.core.mode != SM2) return "";
    if (si.data.cardList.isEmpty()) return "";

    // Load SM-2 data
    QSettings sf(directories::sessionSaves.filePath(si.filename), QSettings::IniFormat);
    QVariant dv = sf.value("sm2Mode/cardData");
    if (!dv.isValid() || dv.isNull()) return "";

    QMap<QString, sm2CardData> cdMap;
    QMap<QString, QVariant> raw = dv.toMap();
    for (auto it = raw.begin(); it != raw.end(); ++it) cdMap[it.key()] = it.value().value<sm2CardData>();

    QDateTime now = QDateTime::currentDateTime();
    int dueNow = 0, dueToday = 0, dueTomorrow = 0, dueWeek = 0, newCards = 0;
    for (auto& d : cdMap) {
        if (d.state == SM2_NEW) { newCards++; continue; }
        if (d.leeched || d.suspended) continue;
        if (d.due.isValid() && d.due <= now) dueNow++;
        else if (d.due.isValid()) {
            qint64 secs = now.secsTo(d.due);
            if (secs < 86400) dueToday++;
            else if (secs < 172800) dueTomorrow++;
            else if (secs < 604800) dueWeek++;
        }
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "Due:%d Today:%d Tom:%d Wk:%d New:%d",
             dueNow, dueToday, dueTomorrow, dueWeek, newCards);
    return std::string(buf);
}

// ─── SubSession ──────────────────────────────────────────────────────────────

void TuiMenu::createSubSession()
{
    if (sessionSelected < 0 || sessionSelected >= (int)sessions.size()) return;
    auto& si = sessions[sessionSelected];
    if (si.data.cardList.isEmpty()) { statusMessage = "Session has no cards"; return; }

    // Prompt user for percentage (default 50%)
    statusMessage = "Enter top % of most reviewed cards (e.g. 50):";
    // For simplicity, use 50% top cards
    int percent = 50;
    QList<card> sorted = si.data.cardList;
    std::sort(sorted.begin(), sorted.end(), [](const card& a, const card& b) {
        return a.count > b.count;
    });

    int topN = sorted.size() * percent / 100;
    if (topN < 1) topN = 1;
    QList<card> topCards = sorted.mid(0, topN);

    sessionStr ns = si.data;
    ns.cardList = topCards;
    ns.core.name = si.data.core.name + "_sub" + QString::number(percent);
    ns.time.created = QDateTime::currentDateTime();
    ns.time.lastUsed = QDateTime::currentDateTime();
    ns.time.played = 0;
    ns.time.playedCount = 0;

    // Remove any existing sub-session with same name
    QFile::remove(directories::sessionSaves.filePath(ns.core.name));

    QSettings s(directories::sessionSaves.filePath(ns.core.name), QSettings::IniFormat);
    s.setValue("session", QVariant::fromValue(ns));
    s.sync();

    statusMessage = "SubSession created: " + qstrToStd(ns.core.name) +
                    " (" + std::to_string(topCards.size()) + " cards, top " +
                    std::to_string(percent) + "%)";
    refresh();
}

void TuiMenu::buildComponent()
{
    g_sessionNames.clear(); g_deckNames.clear();

    for (auto& s : sessions) {
        std::string n = truncate(qstrToStd(s.data.core.name), 22);
        std::string m = modeToString(s.data.core.mode);
        std::string cnt = std::to_string(s.data.cardList.count()) + "c";
        std::string t = formatTime(s.data.time.played);
        g_sessionNames.push_back(padRight(n, 23) + padRight(m, 8) + padRight(cnt, 8) + t);
    }
    if (g_sessionNames.empty()) g_sessionNames.push_back("  (no sessions)");

    for (auto& d : decks) {
        std::string n = truncate(qstrToStd(d.name), 30);
        std::string cnt = std::to_string(d.cardCount) + " cards";
        g_deckNames.push_back(padRight(n, 32) + cnt);
    }
    if (g_deckNames.empty()) g_deckNames.push_back("  (no decks)");

    t::MenuOption sessionOpt;
    sessionOpt.on_enter = [this] {
        if (sessionSelected >= 0 && sessionSelected < (int)sessions.size()) app->startStudy(sessions[sessionSelected].data);
    };

    auto sList = t::Menu(&g_sessionNames, &sessionSelected, sessionOpt);
    auto dList = t::Menu(&g_deckNames, &deckSelected);
    auto main = t::Container::Horizontal({t::Container::Vertical({sList}), t::Container::Vertical({dList})});

    component = t::Renderer(main, [this, sList, dList] {
        auto sessionsTitle = sessionEditActive
            ? t::inverted(t::text(" Edit Session "))
            : t::bold(t::text(" Sessions "));

        t::Elements se;
        se.push_back(t::center(sessionsTitle));
        se.push_back(t::separator());
        se.push_back(t::dim(t::text(" Name                   Mode     Cards    Time")));
        se.push_back(t::separator());
        se.push_back(t::size(t::HEIGHT, t::EQUAL, 8)(t::frame(t::vscroll_indicator(sList->Render()))));

        // Due forecast for selected SM-2 session
        std::string forecast = getDueForecast();
        if (!forecast.empty()) {
            se.push_back(t::separator());
            se.push_back(t::center(t::color(t::Color::Yellow, t::text(forecast))));
        }
        auto sp = t::border(t::flex(t::vbox(std::move(se))));

        t::Elements de;
        de.push_back(t::center(t::bold(t::text(" Decks "))));
        de.push_back(t::separator());
        de.push_back(t::dim(t::text(" Name                             Cards")));
        de.push_back(t::separator());
        de.push_back(t::size(t::HEIGHT, t::EQUAL, 8)(t::frame(t::vscroll_indicator(dList->Render()))));
        de.push_back(t::separator());
        de.push_back(t::dim(t::text(" [K] delete deck  [Tab] switch panel")));
        auto dp = t::border(t::flex(t::vbox(std::move(de))));

        auto header = t::border(t::center(t::bold(t::text(" SANKI - Terminal SRS "))));

        // Build help line based on state
        std::string helpText;
        if (sessionEditActive) {
            helpText = "  [r]ename  [1/2/3] mode  [Enter] save  [Esc] cancel  — " + editSessionName + " mode=" + std::to_string(editModeIndex);
        } else if (inputtingImport) {
            helpText = "  Import path: " + importPath + "_  [Enter] confirm  [Esc] cancel";
        } else {
            helpText = " Enter:study  n:new  x:edit  d:del  s:subsession  b:browse  e:card  i:import  K:del-deck  c:config  q:quit ";
        }
        auto help = t::center(t::dim(t::text(helpText)));

        auto statusBar = t::inverted(t::text(
            " " + statusMessage +
            " | " + std::to_string(sessions.size()) + " sessions" +
            " | " + std::to_string(decks.size()) + " decks  "));

        return t::vbox({header, help, t::flex(t::hbox({sp, dp})), statusBar});
    });

    component |= t::CatchEvent([this](t::Event e) -> bool {
        if (e == t::Event::Escape) {
            if (sessionEditActive) { sessionEditActive = false; inputtingSessionEdit = false; statusMessage = "Edit cancelled."; return true; }
            if (inputtingImport) { inputtingImport = false; importPath.clear(); statusMessage = "Import cancelled."; return true; }
            app->navigateTo(TuiScreen::QUIT); return true;
        }
        if (e == t::Event::Character('q') || e == t::Event::Character('Q')) {
            if (sessionEditActive || inputtingImport) return false;
            app->navigateTo(TuiScreen::QUIT); return true;
        }

        // Session edit mode
        if (sessionEditActive) {
            if (e == t::Event::Return) { editSession(); return true; }
            if (e == t::Event::Character('r') || e == t::Event::Character('R')) { inputtingSessionEdit = true; return true; }
            if (e == t::Event::Character('1')) { editModeIndex = 0; return true; }
            if (e == t::Event::Character('2')) { editModeIndex = 1; return true; }
            if (e == t::Event::Character('3')) { editModeIndex = 2; return true; }
            if (inputtingSessionEdit && e.is_character()) {
                if (e == t::Event::Backspace) { if (!editSessionName.empty()) editSessionName.pop_back(); }
                else if (e == t::Event::Return) { inputtingSessionEdit = false; editSession(); return true; }
                else if (e == t::Event::Escape) { inputtingSessionEdit = false; }
                else { editSessionName += e.character(); }
                return true;
            }
            return false;
        }

        // Import mode
        if (inputtingImport) {
            if (e == t::Event::Return) { importDeck(); return true; }
            if (e.is_character()) {
                if (e == t::Event::Backspace) { if (!importPath.empty()) importPath.pop_back(); }
                else { importPath += e.character(); }
                return true;
            }
            return false;
        }

        // Normal mode
        if (e == t::Event::Character('n') || e == t::Event::Character('N')) { app->navigateTo(TuiScreen::SESSION_CREATE); return true; }
        if (e == t::Event::Character('c') || e == t::Event::Character('C')) { app->navigateTo(TuiScreen::SETTINGS); return true; }
        if (e == t::Event::Character('x') || e == t::Event::Character('X')) { editSession(); return true; }
        if (e == t::Event::Character('i') || e == t::Event::Character('I')) { importDeck(); return true; }
        if (e == t::Event::Character('s') || e == t::Event::Character('S')) { createSubSession(); return true; }
        if (e == t::Event::Character('d') || e == t::Event::Character('D')) {
            if (sessionSelected >= 0 && sessionSelected < (int)sessions.size()) deleteSession(sessions[sessionSelected].filename);
            return true;
        }
        if (e == t::Event::Character('K')) { deleteDeck(); return true; }
        if (e == t::Event::Character('b') || e == t::Event::Character('B')) {
            if (sessionSelected >= 0 && sessionSelected < (int)sessions.size()) app->browseCards(sessions[sessionSelected].data);
            return true;
        }
        if (e == t::Event::Character('e') || e == t::Event::Character('E')) {
            if (sessionSelected >= 0 && sessionSelected < (int)sessions.size()) app->editCard(sessions[sessionSelected].data, 0);
            return true;
        }
        if (e == t::Event::Character('\n') || e == t::Event::Character('\r')) {
            if (sessionSelected >= 0 && sessionSelected < (int)sessions.size()) app->startStudy(sessions[sessionSelected].data);
            return true;
        }
        return false;
    });
}

t::Component TuiMenu::getComponent() { return component; }
