#include "tui_card_browser.h"
#include "tui_app.h"
#include "tui_utils.h"
#include "tui_card_utils.h"
#include "global.h"

#include <QSqlQuery>
#include <QDir>
#include <QDebug>
#include <climits>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

namespace t = ftxui;

TuiCardBrowser::TuiCardBrowser() {}

void TuiCardBrowser::init(TuiApp* app) { this->app = app; buildComponent(); }

void TuiCardBrowser::startSession(sessionStr session)
{
    this->session = session;
    selectedIndex = 0; showBack = false; searchFilter.clear(); searchActive = false;
    for (int i = 0; i < session.core.deckPathList.count(); i++) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "browse_" + QString::number(i));
        db.setDatabaseName(findDatabaseFileTui(directories::deckStorage.filePath(session.core.deckPathList[i])));
        db.open(); databases.append(db);
    }
    loadCards();
}

void TuiCardBrowser::loadCards()
{
    if (selectedIndex < 0 || selectedIndex >= session.cardList.count()) { currentFront = "(no card)"; currentBack = ""; return; }
    card& c = session.cardList[selectedIndex];

    QSqlQuery q("SELECT flds FROM notes WHERE id = :id", databases[c.deckiD]);
    q.bindValue(0, c.id);
    QString mc;
    if (q.exec() && q.next()) mc = q.value(0).toString();

    QString mp = directories::deckStorage.filePath(session.core.deckPathList[c.deckiD]) + QDir::separator() + "media";
    correctMainCard(&mc, mp);

    QStringList parts = mc.split("\u001F");
    QString fh = parts.first(), bh = (parts.size() > 1) ? parts.last() : "";
    processClozeDeletions(&fh, &bh);
    currentFront = qstrToStd(stripHtml(fh));
    currentBack  = qstrToStd(stripHtml(bh));

    // SM-2 info if available
    QString deckName = session.core.deckPathList[c.deckiD].split("/").last();
    statusText = "Card " + std::to_string(selectedIndex + 1) + "/" + std::to_string(session.cardList.count())
                 + " | Deck: " + qstrToStd(deckName);

    if (searchActive && !searchFilter.empty()) {
        statusText += " | Filter: \"" + searchFilter + "\"";
    }
}

void TuiCardBrowser::buildComponent()
{
    component = t::Renderer([this] {
        t::Elements items;
        items.push_back(t::center(t::border(t::hbox({
            t::bold(t::text(" Card Browser ")),
            t::dim(t::text(" | " + std::to_string(session.cardList.count()) + " cards ")),
            searchActive ? t::inverted(t::text(" Search: " + searchFilter + " ")) : t::text(""),
        }))));

        items.push_back(t::center(t::color(t::Color::Cyan, t::bold(t::text(" FRONT ")))));
        items.push_back(t::separator());
        items.push_back(t::border(t::size(t::HEIGHT, t::GREATER_THAN, 3)(t::paragraph(currentFront))));
        if (showBack) {
            items.push_back(t::center(t::color(t::Color::Green, t::bold(t::text(" BACK ")))));
            items.push_back(t::separator());
            items.push_back(t::border(t::size(t::HEIGHT, t::GREATER_THAN, 3)(t::paragraph(currentBack))));
        }

        items.push_back(t::separator());
        items.push_back(t::center(t::dim(t::text(" " + statusText + " "))));
        items.push_back(t::separator());
        items.push_back(t::center(t::hbox({
            t::bold(t::text(" [j/k] Nav  [Space] Show  [/] Search  [n/N] Next match  [e] Edit  [Esc] Back ")),
        })));

        return t::border(t::vbox(std::move(items)));
    });

    component |= t::CatchEvent([this](t::Event e) -> bool {
        if (e == t::Event::Escape) {
            if (searchActive) { searchActive = false; searchFilter.clear(); loadCards(); return true; }
            for (int i = 0; i < databases.count(); i++) { databases[i].close(); QSqlDatabase::removeDatabase("browse_" + QString::number(i)); }
            databases.clear();
            app->navigateTo(TuiScreen::MENU); return true;
        }

        if (searchActive) {
            if (e.is_character()) {
                if (e == t::Event::Backspace) { if (!searchFilter.empty()) searchFilter.pop_back(); }
                else if (e == t::Event::Return) { searchActive = false; }
                else { searchFilter += e.character(); }
                return true;
            }
            return false;
        }

        if (e == t::Event::Character('/')) { searchActive = true; searchFilter.clear(); return true; }
        if (e == t::Event::Character('j')) { if (selectedIndex > 0) { selectedIndex--; loadCards(); } return true; }
        if (e == t::Event::Character('k')) { if (selectedIndex < session.cardList.count() - 1) { selectedIndex++; loadCards(); } return true; }
        if (e == t::Event::Character(' ')) { showBack = !showBack; return true; }
        if (e == t::Event::Character('n')) {
            // Find next card matching search
            if (!searchFilter.empty()) {
                std::string sf = searchFilter;
                std::transform(sf.begin(), sf.end(), sf.begin(), ::tolower);
                for (int i = selectedIndex + 1; i < session.cardList.count(); i++) {
                    selectedIndex = i; loadCards();
                    std::string ft = currentFront; std::transform(ft.begin(), ft.end(), ft.begin(), ::tolower);
                    std::string bt = currentBack;  std::transform(bt.begin(), bt.end(), bt.begin(), ::tolower);
                    if (ft.find(sf) != std::string::npos || bt.find(sf) != std::string::npos) return true;
                }
                statusText = "No more matches for: " + searchFilter;
                loadCards();
            }
            return true;
        }
        if (e == t::Event::Character('N')) {
            if (!searchFilter.empty()) {
                std::string sf = searchFilter;
                std::transform(sf.begin(), sf.end(), sf.begin(), ::tolower);
                for (int i = selectedIndex - 1; i >= 0; i--) {
                    selectedIndex = i; loadCards();
                    std::string ft = currentFront; std::transform(ft.begin(), ft.end(), ft.begin(), ::tolower);
                    std::string bt = currentBack;  std::transform(bt.begin(), bt.end(), bt.begin(), ::tolower);
                    if (ft.find(sf) != std::string::npos || bt.find(sf) != std::string::npos) return true;
                }
                statusText = "No previous matches for: " + searchFilter;
                loadCards();
            }
            return true;
        }
        if (e == t::Event::Character('e') || e == t::Event::Character('E')) { app->editCard(session, selectedIndex); return true; }
        return false;
    });
}

t::Component TuiCardBrowser::getComponent() { return component; }
