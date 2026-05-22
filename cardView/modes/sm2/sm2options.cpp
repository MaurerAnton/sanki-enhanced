#include "cardView/modes/sm2/sm2options.h"
#include "ui_sm2options.h"

#include <QMessageBox>

sm2OptionsDialog::sm2OptionsDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::sm2Options)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
    ui->infoButton->setIcon(QIcon(":/icons/info.svg"));
}

sm2OptionsDialog::~sm2OptionsDialog()
{
    delete ui;
}

void sm2OptionsDialog::start(sm2Config* configArg)
{
    config = configArg;

    ui->learningStepsEdit->setText(config->learningSteps);
    ui->graduatingSpinBox->setValue((int)(config->graduatingInterval / 86400));
    ui->easySpinBox->setValue((int)(config->easyInterval / 86400));
    ui->startingEaseSpinBox->setValue(config->startingEase);
    ui->easyBonusSpinBox->setValue(config->easyBonus);
    ui->intervalModifierSpinBox->setValue(config->intervalModifier);
    ui->hardIntervalSpinBox->setValue(config->hardIntervalMultiplier);
    ui->newIntervalSpinBox->setValue(config->newIntervalFactor);
    ui->maxIntervalSpinBox->setValue((int)(config->maxInterval / 86400));
    ui->leechSpinBox->setValue(config->leechThreshold);
    ui->maxReviewsSpinBox->setValue(config->maxReviewsPerDay);
    ui->reversedCheckBox->setChecked(config->reversedMode);
    ui->twoButtonCheckBox->setChecked(config->twoButtonMode);
}

void sm2OptionsDialog::on_acceptButton_clicked()
{
    this->close();
}

void sm2OptionsDialog::on_infoButton_clicked()
{
    QMessageBox::information(this, "SM-2 Algorithm",
        "Learning steps: space-separated seconds (e.g. '60 600' = 1min, 10min).\n"
        "Graduating interval: days until a card is considered 'learned'.\n"
        "Easy interval: days when Easy is pressed during learning.\n"
        "Starting ease: initial ease factor (2.50 = 250%).\n"
        "Easy bonus: added when Easy is pressed on a review.\n"
        "Interval modifier: multiplier for all intervals.\n"
        "Hard interval: multiplier for Hard button on reviews.\n"
        "New interval: % of old interval kept on lapse (0% = reset).");
}

void sm2OptionsDialog::on_learningStepsEdit_textChanged(const QString &arg1)
{
    config->learningSteps = arg1;
}

void sm2OptionsDialog::on_graduatingSpinBox_valueChanged(int arg1)
{
    config->graduatingInterval = (qint64)arg1 * 86400;
}

void sm2OptionsDialog::on_easySpinBox_valueChanged(int arg1)
{
    config->easyInterval = (qint64)arg1 * 86400;
}

void sm2OptionsDialog::on_startingEaseSpinBox_valueChanged(double arg1)
{
    config->startingEase = arg1;
}

void sm2OptionsDialog::on_easyBonusSpinBox_valueChanged(double arg1)
{
    config->easyBonus = arg1;
}

void sm2OptionsDialog::on_intervalModifierSpinBox_valueChanged(double arg1)
{
    config->intervalModifier = arg1;
}

void sm2OptionsDialog::on_hardIntervalSpinBox_valueChanged(double arg1)
{
    config->hardIntervalMultiplier = arg1;
}

void sm2OptionsDialog::on_newIntervalSpinBox_valueChanged(double arg1)
{
    config->newIntervalFactor = arg1;
}

void sm2OptionsDialog::on_maxIntervalSpinBox_valueChanged(int arg1)
{
    config->maxInterval = (qint64)arg1 * 86400;
}

void sm2OptionsDialog::on_leechSpinBox_valueChanged(int arg1)
{
    config->leechThreshold = arg1;
}

void sm2OptionsDialog::on_maxReviewsSpinBox_valueChanged(int arg1)
{
    config->maxReviewsPerDay = arg1;
}

void sm2OptionsDialog::on_reversedCheckBox_stateChanged(int arg1)
{
    config->reversedMode = (arg1 == Qt::Checked);
}

void sm2OptionsDialog::on_twoButtonCheckBox_stateChanged(int arg1)
{
    config->twoButtonMode = (arg1 == Qt::Checked);
}
