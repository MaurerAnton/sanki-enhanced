#ifndef SETTINGSMENU_H
#define SETTINGSMENU_H

#include <QWidget>
#include "components/other/pomodoro.h"
#include "components/statusBarC.h"

namespace Ui {
class settingsMenu;
}

class settingsMenu : public QWidget
{
    Q_OBJECT

public:
    explicit settingsMenu(QWidget *parent = nullptr);
    ~settingsMenu();
    void start(pomodoro* pomodoroWidgetParent, statusBarC* statusBarParent);
    void exitWidgetCheck();

private slots:
    void on_settingsButton_clicked();

    void on_pomodoroButton_clicked();

    void on_refreshButton_clicked();

    void on_reverseButton_clicked();

    void on_cramButton_clicked();
    void on_filterButton_clicked();
    void on_suspendButton_clicked();
    void on_buryButton_clicked();
    void on_flagButton_clicked();
    void on_minimalButton_clicked();

signals:
    void tellDeck(QString call);

private:
    Ui::settingsMenu *ui;
    pomodoro* pomodoroWidget;
    statusBarC* statusBar;
};

#endif // SETTINGSMENU_H
