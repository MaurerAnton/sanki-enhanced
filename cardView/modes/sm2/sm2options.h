#ifndef SM2OPTIONS_H
#define SM2OPTIONS_H

#include "cardView/modes/sm2/sm2.h"
#include <QWidget>
#include <QDialog>

namespace Ui { class sm2Options; }

class sm2OptionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit sm2OptionsDialog(QWidget *parent = nullptr);
    ~sm2OptionsDialog();
    void start(sm2Config* configArg);
    sm2Config* config;
private slots:
    void on_acceptButton_clicked();
    void on_infoButton_clicked();
    void on_learningStepsEdit_textChanged(const QString &arg1);
    void on_graduatingSpinBox_valueChanged(int arg1);
    void on_easySpinBox_valueChanged(int arg1);
    void on_startingEaseSpinBox_valueChanged(double arg1);
    void on_easyBonusSpinBox_valueChanged(double arg1);
    void on_intervalModifierSpinBox_valueChanged(double arg1);
    void on_hardIntervalSpinBox_valueChanged(double arg1);
    void on_newIntervalSpinBox_valueChanged(double arg1);
    void on_maxIntervalSpinBox_valueChanged(int arg1);
    void on_leechSpinBox_valueChanged(int arg1);
    void on_maxReviewsSpinBox_valueChanged(int arg1);
    void on_reversedCheckBox_stateChanged(int arg1);
    void on_twoButtonCheckBox_stateChanged(int arg1);
private:
    Ui::sm2Options *ui;
};

#endif // SM2OPTIONS_H
