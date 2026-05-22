#include "tui_session_create.h"
#include "tui_app.h"
#include "tui_utils.h"
#include "tui_card_utils.h"
#include "global.h"

#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

namespace t = ftxui;

TuiSessionCreate::TuiSessionCreate() { modeNames = {"Random", "Boxes", "SM-2"}; }

void TuiSessionCreate::init(TuiApp* app) { this->app = app; reset(); }

void TuiSessionCreate::reset()
{
    sessionName = "New Session"; modeIndex = 0; inputtingName = false; statusMessage = "";
    loadAvailableDecks(); buildComponent();
}

void TuiSessionCreate::loadAvailableDecks()
{
    availableDecks.clear();
    QDir dir(directories::deckStorage);
    for (const QString& subdir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        DeckChoice dc; dc.name = subdir; dc.path = directories::deckStorage.filePath(subdir); dc.selected = false;
        availableDecks.push_back(dc);
    }
}

void TuiSessionCreate::createSession()
{
    QList<QString> sp;
    for (auto& dc : availableDecks) if (dc.selected) sp.append(dc.path);
    if (sp.isEmpty()) { statusMessage = "Error: Select at least one deck!"; return; }
    if (sessionName.empty()) { statusMessage = "Error: Enter a session name!"; return; }

    sessionStr s;
    s.core.name = stdToQstr(sessionName);
    s.core.deckPathList = sp;
    s.core.mode = (modeIndex == 0) ? CompletlyRandomised : (modeIndex == 1) ? Boxes : SM2;
    s.time.created = QDateTime::currentDateTime();
    s.time.lastUsed = QDateTime::currentDateTime();

    for (int i = 0; i < sp.count(); i++) {
        QString dbFile = findDatabaseFileTui(sp[i]);
        if (dbFile == "none") continue;
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "createsess_" + QString::number(i));
        db.setDatabaseName(dbFile);
        if (db.open()) {
            QSqlQuery q("SELECT id FROM notes ORDER BY RANDOM()", db);
            while (q.next()) { card c; c.id = q.value(0).toULongLong(); c.deckiD = (uint)i; s.cardList.append(c); }
            db.close();
        }
        QSqlDatabase::removeDatabase("createsess_" + QString::number(i));
    }

    QSettings settings(directories::sessionSaves.filePath(s.core.name), QSettings::IniFormat);
    settings.setValue("session", QVariant::fromValue(s));
    settings.sync();

    statusMessage = "Created: " + sessionName + " (" + std::to_string(s.cardList.count()) + " cards)";
    app->menuScreen.refresh();
    app->navigateTo(TuiScreen::MENU);
}

void TuiSessionCreate::buildComponent()
{
    component = t::Renderer([this] {
        t::Elements items;
        items.push_back(t::center(t::bold(t::text(" Create New Session "))));
        items.push_back(t::separator());

        t::Elements me;
        me.push_back(t::bold(t::text(" Mode: ")));
        for (size_t i = 0; i < modeNames.size(); i++) {
            if ((int)i == modeIndex) me.push_back(t::inverted(t::text(" > " + modeNames[i] + " < ")));
            else me.push_back(t::text("   " + modeNames[i] + "   "));
        }
        items.push_back(t::hbox(std::move(me)));
        items.push_back(t::separator());

        items.push_back(t::hbox({
            t::bold(t::text(" Name: ")),
            inputtingName ? t::inverted(t::text(sessionName + " ")) : t::text(sessionName),
            t::dim(t::text(" [r]ename ")),
        }));
        items.push_back(t::separator());

        items.push_back(t::bold(t::text(" Decks (1-9=toggle, Enter=create): ")));
        for (size_t i = 0; i < availableDecks.size(); i++) {
            auto& dc = availableDecks[i];
            std::string marker = dc.selected ? " [x] " : " [ ] ";
            std::string lbl = std::to_string(i + 1) + ":" + marker + qstrToStd(dc.name);
            items.push_back(t::text(lbl));
        }
        if (availableDecks.empty()) items.push_back(t::dim(t::text("  (no decks installed)")));
        items.push_back(t::separator());

        items.push_back(t::center(t::hbox({
            t::bold(t::text(" [Enter] Create  ")),
            t::bold(t::text(" [Esc] Back  ")),
            t::text(" [1/2/3] Mode  [r] Rename  "),
        })));

        if (!statusMessage.empty()) {
            items.push_back(t::separator());
            items.push_back(t::center(t::color(t::Color::Yellow, t::text(" " + statusMessage + " "))));
        }

        return t::center(t::size(t::WIDTH, t::GREATER_THAN, 55)(t::border(t::vbox(std::move(items)))));
    });

    component |= t::CatchEvent([this](t::Event e) -> bool {
        if (e == t::Event::Escape) { app->navigateTo(TuiScreen::MENU); return true; }
        if (e == t::Event::Return) { createSession(); return true; }
        if (e == t::Event::Character('1')) { modeIndex = 0; return true; }
        if (e == t::Event::Character('2')) { modeIndex = 1; return true; }
        if (e == t::Event::Character('3')) { modeIndex = 2; return true; }
        if (e == t::Event::Character('r') || e == t::Event::Character('R')) { inputtingName = true; return true; }
        // Toggle decks by number
        for (size_t i = 0; i < availableDecks.size() && i < 9; i++) {
            char c = '1' + (char)i;
            if (e == t::Event::Character(c)) {
                availableDecks[i].selected = !availableDecks[i].selected; return true;
            }
        }
        if (e == t::Event::Character(' ')) {
            // Toggle a deck - simplified
            for (auto& dc : availableDecks) dc.selected = !dc.selected;
            return true;
        }
        if (inputtingName && e.is_character()) {
            if (e == t::Event::Backspace) { if (!sessionName.empty()) sessionName.pop_back(); }
            else if (e == t::Event::Return) { inputtingName = false; }
            else if (e == t::Event::Escape) { inputtingName = false; }
            else { sessionName += e.character(); }
            return true;
        }
        return false;
    });
}

t::Component TuiSessionCreate::getComponent() { return component; }
