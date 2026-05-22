#include "tui_card_editor.h"
#include "tui_app.h"
#include "tui_utils.h"
#include "tui_card_utils.h"
#include "global.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QDebug>
#include <QUuid>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

namespace t = ftxui;

TuiCardEditor::TuiCardEditor() {}

void TuiCardEditor::init(TuiApp* app) { this->app = app; buildComponent(); }

void TuiCardEditor::setCard(sessionStr session, int ci)
{
    this->session = session; cardIndex = ci;
    editingFront = true; statusText = "";
    for (int i = 0; i < session.core.deckPathList.count(); i++) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "edit_" + QString::number(i));
        db.setDatabaseName(findDatabaseFileTui(directories::deckStorage.filePath(session.core.deckPathList[i])));
        db.open(); databases.append(db);
    }
    loadCardData();
}

void TuiCardEditor::loadCardData()
{
    if (cardIndex < 0 || cardIndex >= session.cardList.count()) {
        frontText = ""; backText = "";
        statusText = "New card (no existing data)";
        return;
    }
    card& c = session.cardList[cardIndex];
    QSqlQuery q("SELECT flds FROM notes WHERE id = :id", databases[c.deckiD]);
    q.bindValue(0, c.id);
    if (q.exec() && q.next()) {
        QString mc = q.value(0).toString();
        QString mp = directories::deckStorage.filePath(session.core.deckPathList[c.deckiD]) + QDir::separator() + "media";
        correctMainCard(&mc, mp);
        QStringList parts = mc.split("\u001F");
        frontText = qstrToStd(stripHtml(parts.first()));
        backText  = qstrToStd(stripHtml((parts.size() > 1) ? parts.last() : ""));
    }
    statusText = "Editing card " + std::to_string(cardIndex + 1) + "/" + std::to_string(session.cardList.count());
}

void TuiCardEditor::saveCard()
{
    if (cardIndex < 0 || cardIndex >= session.cardList.count()) return;
    card& c = session.cardList[cardIndex];
    QString nc = stdToQstr(frontText) + "\u001F" + stdToQstr(backText);
    QSqlQuery q(databases[c.deckiD]);
    q.prepare("UPDATE notes SET flds = :flds WHERE id = :id");
    q.bindValue(":flds", nc);
    q.bindValue(":id", c.id);
    if (q.exec()) statusText = "Card saved.";
    else statusText = "Error: " + qstrToStd(q.lastError().text());
}

void TuiCardEditor::addNewCard()
{
    // Generate a new unique ID
    quint64 newId = 0;
    for (auto& c : session.cardList) newId = std::max(newId, c.id);
    newId++;

    // Insert into the first deck's database
    int deckId = 0;
    if (!session.cardList.isEmpty()) deckId = (int)session.cardList.first().deckiD;
    if (deckId >= databases.size()) deckId = 0;

    QSqlQuery q(databases[deckId]);
    q.prepare("INSERT INTO notes (id, guid, mid, mod, usn, tags, flds, sfld, csum, flags, data) "
              "VALUES (:id, :guid, 1, 0, -1, '', :flds, '', 0, 0, '')");
    q.bindValue(":id", newId);
    q.bindValue(":guid", QUuid::createUuid().toString(QUuid::WithoutBraces));
    q.bindValue(":flds", stdToQstr(frontText) + "\u001F" + stdToQstr(backText));

    if (q.exec()) {
        card nc; nc.id = newId; nc.deckiD = (uint)deckId;
        session.cardList.append(nc);
        cardIndex = session.cardList.count() - 1;
        statusText = "New card added. ID: " + std::to_string(newId);
    } else {
        statusText = "Add error: " + qstrToStd(q.lastError().text());
    }
}

void TuiCardEditor::deleteCurrentCard()
{
    if (cardIndex < 0 || cardIndex >= session.cardList.count()) return;
    card& c = session.cardList[cardIndex];

    QSqlQuery q(databases[c.deckiD]);
    q.prepare("DELETE FROM notes WHERE id = :id");
    q.bindValue(":id", c.id);
    if (q.exec()) {
        statusText = "Card deleted. ID: " + std::to_string(c.id);
        session.cardList.removeAt(cardIndex);
        if (cardIndex >= session.cardList.count()) cardIndex = session.cardList.count() - 1;
        loadCardData();
    } else {
        statusText = "Delete error: " + qstrToStd(q.lastError().text());
    }
}

void TuiCardEditor::buildComponent()
{
    component = t::Renderer([this] {
        t::Elements items;
        items.push_back(t::center(t::border(t::hbox({
            t::bold(t::text(" Card Editor ")),
            t::dim(t::text(" | " + statusText)),
        }))));

        items.push_back(t::hbox({
            t::bold(t::text(" Front: ")),
            editingFront ? t::color(t::Color::Cyan, t::text(" << EDITING >> "))
                         : t::dim(t::text(" (viewing) ")),
        }));
        items.push_back(t::separator());
        items.push_back(t::border(t::size(t::HEIGHT, t::GREATER_THAN, 3)(t::paragraph(frontText))));

        items.push_back(t::hbox({
            t::bold(t::text(" Back: ")),
            !editingFront ? t::color(t::Color::Green, t::text(" << EDITING >> "))
                          : t::dim(t::text(" (viewing) ")),
        }));
        items.push_back(t::separator());
        items.push_back(t::border(t::size(t::HEIGHT, t::GREATER_THAN, 3)(t::paragraph(backText))));

        items.push_back(t::separator());
        items.push_back(t::center(t::hbox({
            t::bold(t::text(" [Tab] Switch  [s] Save  [a] Add card  [d] Delete  [Esc] Back  ")),
        })));

        return t::border(t::vbox(std::move(items)));
    });

    component |= t::CatchEvent([this](t::Event e) -> bool {
        if (e == t::Event::Escape) {
            for (int i = 0; i < databases.count(); i++) { databases[i].close(); QSqlDatabase::removeDatabase("edit_" + QString::number(i)); }
            databases.clear();
            app->navigateTo(TuiScreen::CARD_BROWSER); return true;
        }
        if (e == t::Event::Character('\t')) { editingFront = !editingFront; return true; }
        if (e == t::Event::Character('s') || e == t::Event::Character('S')) { saveCard(); return true; }
        if (e == t::Event::Character('a') || e == t::Event::Character('A')) { addNewCard(); return true; }
        if (e == t::Event::Character('d') || e == t::Event::Character('D')) { deleteCurrentCard(); return true; }
        if (e.is_character()) {
            std::string& target = editingFront ? frontText : backText;
            if (e == t::Event::Backspace) { if (!target.empty()) target.pop_back(); }
            else if (e.character() == "\n" || e.character() == "\r") { target += "\n"; }
            else { target += e.character(); }
            return true;
        }
        return false;
    });
}

t::Component TuiCardEditor::getComponent() { return component; }
