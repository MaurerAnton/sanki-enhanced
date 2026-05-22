#ifndef CARDBROWSER_H
#define CARDBROWSER_H

#include "mainMenu/sessions/sessionStruct.h"
#include "cardView/modes/sm2/sm2.h"
#include <QDialog>
#include <QFile>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QMap>

namespace Ui { class cardBrowser; }

class cardBrowser : public QDialog
{
    Q_OBJECT
public:
    explicit cardBrowser(QWidget *parent = nullptr);
    ~cardBrowser();
    void start(sessionStr session);

private slots:
    void on_searchEdit_textChanged(const QString &arg1);
    void on_tableWidget_cellDoubleClicked(int row, int column);
    void on_editButton_clicked();
    void on_resetButton_clicked();
    void on_suspendButton_clicked();
    void on_flagButton_clicked();
    void on_closeButton_clicked();
    void on_tagCombo_currentIndexChanged(int index);

private:
    Ui::cardBrowser *ui;
    sessionStr currentSession;
    QList<QSqlDatabase> databases;
    bool hasSm2Data = false;
    QMap<QString, sm2CardData> sm2CardMap;
    QMap<QString, QString> tagMap;

    void loadCards(const QString& filter = "", const QString& tagFilter = "");
    void loadSm2Data();
    void loadTags();
    QString stripHtml(const QString& html);
    QString stateName(int state);
    QString formatDue(const QDateTime& due, int state);
    QString formatInterval(qint64 seconds);
    int selectedRow = -1;
    QString currentTagFilter;

    struct cardRow {
        quint64 noteId;
        uint deckId;
        QString front;
        QString back;
        QString flds;
        int sm2State = -1;
        qreal sm2Ease = 0;
        qint64 sm2Interval = 0;
        QDateTime sm2Due;
        int sm2Lapses = 0;
        int sm2Leeched = 0;
        int sm2Suspended = 0;
        int sm2Flagged = 0;
        QString tags;
    };
    QList<cardRow> allCards;
};

#endif // CARDBROWSER_H
