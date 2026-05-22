#include "components/other/cardEditor.h"
#include "ui_cardEditor.h"
#include "global.h"
#include "cardView/functions/helperFunctions.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDateTime>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QDebug>

cardEditor::cardEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::cardEditor)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);

    if (ereader) {
        ui->frontEdit->setStyleSheet("font-size: 9pt;");
        ui->backEdit->setStyleSheet("font-size: 9pt;");
    }
}

cardEditor::~cardEditor()
{
    closeDatabases();
    delete ui;
}

void cardEditor::openDatabases()
{
    closeDatabases();
    for (int i = 0; i < currentSession.core.deckPathList.count(); i++) {
        QString dbFile = findDatabaseFile(
            directories::deckStorage.filePath(
                currentSession.core.deckPathList[i]));
        if (dbFile == "none") continue;

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE",
            QString("editor_%1").arg(i));
        db.setDatabaseName(dbFile);
        if (db.open()) {
            databases.append(db);
        }
    }
}

void cardEditor::closeDatabases()
{
    warningsEnabled = false;
    for (int i = 0; i < databases.count(); i++) {
        databases[i].close();
        QSqlDatabase::removeDatabase(QString("editor_%1").arg(i));
    }
    warningsEnabled = true;
    databases.clear();
}

qint64 cardEditor::currentTimestamp()
{
    return QDateTime::currentSecsSinceEpoch() * 1000;
}

quint64 cardEditor::generateNoteId(uint deckId)
{
    if (deckId >= (uint)databases.count()) return 0;

    quint64 maxId = 0;
    QSqlQuery query(databases[deckId]);
    query.exec("SELECT MAX(id) FROM notes");
    if (query.next()) {
        maxId = query.value(0).toULongLong();
    }

    quint64 ts = (quint64)currentTimestamp();
    if (ts <= maxId) {
        ts = maxId + 1;
    }
    return ts;
}

void cardEditor::start(sessionStr session)
{
    currentSession = session;
    openDatabases();

    ui->deckCombo->clear();
    for (const QString& path : currentSession.core.deckPathList) {
        ui->deckCombo->addItem(path.split("/").last());
    }

    editingExisting = false;
    ui->titleLabel->setText("New Card");
    ui->deleteButton->setVisible(false);
    ui->frontEdit->clear();
    ui->backEdit->clear();
}

void cardEditor::editCard(quint64 noteId, uint deckId, const QString& flds)
{
    currentSession = currentSession;
    openDatabases();

    ui->deckCombo->clear();
    for (const QString& path : currentSession.core.deckPathList) {
        ui->deckCombo->addItem(path.split("/").last());
    }

    editingExisting = true;
    editingNoteId = noteId;
    editingDeckId = deckId;
    ui->titleLabel->setText(QString("Edit Card %1").arg(noteId));
    ui->deleteButton->setVisible(true);

    if (deckId < (uint)ui->deckCombo->count()) {
        ui->deckCombo->setCurrentIndex(deckId);
    }

    QStringList parts = flds.split("\u001F");
    ui->frontEdit->setHtml(parts.value(0));
    ui->backEdit->setHtml(parts.value(1));
}

void cardEditor::on_saveButton_clicked()
{
    QString front = ui->frontEdit->toHtml();
    QString back = ui->backEdit->toHtml();

    if (front.trimmed().isEmpty() && back.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Empty card", "Both front and back are empty.");
        return;
    }

    uint deckId = ui->deckCombo->currentIndex();
    if (deckId >= (uint)databases.count()) {
        QMessageBox::warning(this, "Error", "No database open for selected deck.");
        return;
    }

    QString flds = front + "\u001F" + back;
    qint64 ts = currentTimestamp();

    if (editingExisting) {
        QSqlQuery query(databases[editingDeckId]);
        query.prepare("UPDATE notes SET flds = :flds, mod = :mod, usn = -1 WHERE id = :id");
        query.bindValue(":flds", flds);
        query.bindValue(":mod", ts);
        query.bindValue(":id", editingNoteId);
        if (!query.exec()) {
            qWarning() << "Update failed:" << query.lastError().text();
            QMessageBox::warning(this, "Error", "Failed to update card.");
            return;
        }
    } else {
        quint64 newId = generateNoteId(deckId);
        if (newId == 0) {
            QMessageBox::warning(this, "Error", "Failed to generate card ID.");
            return;
        }

        QString guid = QCryptographicHash::hash(
            (QString::number(newId) + flds).toUtf8(),
            QCryptographicHash::Sha1).toHex();

        QString sfld = front;
        sfld.replace(QRegularExpression("<[^>]*>"), "");
        sfld = sfld.trimmed().left(100);

        QByteArray fldsUtf8 = flds.toUtf8();
        quint16 crc = qChecksum(fldsUtf8.constData(), fldsUtf8.size());

        QSqlQuery query(databases[deckId]);
        query.prepare(
            "INSERT INTO notes (id, guid, mid, mod, usn, tags, flds, sfld, csum, flags, data) "
            "VALUES (:id, :guid, 0, :mod, -1, '', :flds, :sfld, :csum, 0, '')");
        query.bindValue(":id", newId);
        query.bindValue(":guid", guid);
        query.bindValue(":mod", ts);
        query.bindValue(":flds", flds);
        query.bindValue(":sfld", sfld);
        query.bindValue(":csum", crc);

        if (!query.exec()) {
            qWarning() << "Insert failed:" << query.lastError().text();
            QMessageBox::warning(this, "Error", "Failed to add card: " + query.lastError().text());
            return;
        }
    }

    emit cardsChanged();
    this->accept();
}

void cardEditor::on_deleteButton_clicked()
{
    if (!editingExisting) return;

    int ret = QMessageBox::question(this, "Delete Card",
        QString("Delete card %1?").arg(editingNoteId),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QSqlQuery query(databases[editingDeckId]);
    query.prepare("DELETE FROM notes WHERE id = :id");
    query.bindValue(":id", editingNoteId);
    if (!query.exec()) {
        qWarning() << "Delete failed:" << query.lastError().text();
        QMessageBox::warning(this, "Error", "Failed to delete card.");
        return;
    }

    emit cardsChanged();
    this->accept();
}

void cardEditor::on_cancelButton_clicked()
{
    this->reject();
}

void cardEditor::on_deckCombo_currentIndexChanged(int index)
{
    Q_UNUSED(index);
}
