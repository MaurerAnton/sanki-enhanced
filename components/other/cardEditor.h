#ifndef CARDEDITOR_H
#define CARDEDITOR_H

#include "mainMenu/sessions/sessionStruct.h"
#include <QDialog>
#include <QSqlDatabase>
#include <QTextEdit>
#include <QLineEdit>

namespace Ui { class cardEditor; }

class cardEditor : public QDialog
{
    Q_OBJECT
public:
    explicit cardEditor(QWidget *parent = nullptr);
    ~cardEditor();
    void start(sessionStr session);
    void editCard(quint64 noteId, uint deckId, const QString& flds);

signals:
    void cardsChanged();

private slots:
    void on_saveButton_clicked();
    void on_deleteButton_clicked();
    void on_cancelButton_clicked();
    void on_deckCombo_currentIndexChanged(int index);

private:
    Ui::cardEditor *ui;
    sessionStr currentSession;
    QList<QSqlDatabase> databases;
    bool editingExisting = false;
    quint64 editingNoteId = 0;
    uint editingDeckId = 0;

    void openDatabases();
    void closeDatabases();
    quint64 generateNoteId(uint deckId);
    qint64 currentTimestamp();
};

#endif // CARDEDITOR_H
