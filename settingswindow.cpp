#include "settingswindow.h"
#include "ui_settingswindow.h"
#include <QMessageBox>
#include <QColorDialog>

#ifdef Q_OS_MAC
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#elif defined(Q_OS_WIN32)
#include <windows.h>
#endif

#define MIN_CACHE_SIZE_IN_MB    20
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

SettingsWindow::SettingsWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SettingsWindow)
{
    // get size of main memory - assume 2 GB first
    p_memSizeInMB = 2 << 10;
#ifdef Q_OS_MAC
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    u_int namelen = sizeof(mib) / sizeof(mib[0]);
    uint64_t size;
    size_t len = sizeof(size);

    if (sysctl(mib, namelen, &size, &len, NULL, 0) == 0)
        p_memSizeInMB = size >> 20;
#elif defined Q_OS_UNIX
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    p_memSizeInMB = (pages * page_size) >> 20;
#elif defined Q_OS_WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    p_memSizeInMB = status.ullTotalPhys >> 20;
#endif

    ui->setupUi(this);

    loadSettings();
}

SettingsWindow::~SettingsWindow()
{
    delete ui;
}

unsigned int SettingsWindow::getCacheSizeInMB() {
    unsigned int useMem = p_memSizeInMB;
    // update video cache
    settings.beginGroup("VideoCache");
    if ( settings.value("Enabled", true).toBool() ) {
        if ( settings.value("UseThreshold", false).toBool() ) {
            useMem = useMem * settings.value("ThresholdValue", 49).toInt() / 100;
        }
    } else
        useMem = 0;
    settings.endGroup();

    return MAX(useMem, MIN_CACHE_SIZE_IN_MB);
}

void SettingsWindow::on_saveButton_clicked()
{
    if (!saveSettings()) {
        QMessageBox::warning(this, tr("Settings could not be saved"), tr("Sorry, but some settings could not be saved."), QMessageBox::Ok);
    }
    close();
}

bool SettingsWindow::saveSettings() {
    settings.beginGroup("VideoCache");
    settings.setValue("Enabled", ui->cacheCheckBox->checkState() == Qt::Checked);
    settings.setValue("UseThreshold", ui->cacheThresholdCheckBox->checkState() == Qt::Checked);
    settings.setValue("ThresholdValue", ui->cacheThresholdSlider->value());
    settings.endGroup();

    settings.setValue("Statistics/Simplify", ui->simplifyCheckBox->isChecked());
    settings.setValue("Statistics/SimplificationSize", ui->simplifySizeSpinBox->value());

    emit settingsChanged();

    return true;
}

bool SettingsWindow::loadSettings() {
    settings.beginGroup("VideoCache");
    ui->cacheCheckBox->setChecked( settings.value("Enabled", true).toBool() );
    ui->cacheThresholdCheckBox->setChecked( settings.value("UseThreshold", false).toBool() );
    ui->cacheThresholdSlider->setValue( settings.value("ThresholdValue", 49).toInt() );
    settings.endGroup();

    ui->simplifyCheckBox->setChecked(settings.value("Statistics/Simplify", false).toBool());
    ui->simplifySizeSpinBox->setValue(settings.value("Statistics/SimplificationSize", 32).toInt());
    return true;
}

void SettingsWindow::on_cacheThresholdSlider_valueChanged(int value)
{
    ui->cacheThresholdLabel->setText(QString("%1 % (%2 MB)").arg(value+1).arg(p_memSizeInMB * (value+1) / 100));
}

void SettingsWindow::on_cacheCheckBox_stateChanged(int)
{
    if (ui->cacheCheckBox->checkState() == Qt::Checked) {
        ui->cacheThresholdCheckBox->setEnabled(true);
        if (ui->cacheThresholdCheckBox->isChecked()) {
            ui->cacheThresholdSlider->setEnabled(true);
            ui->cacheThresholdLabel->setEnabled(true);
        } else {
            ui->cacheThresholdSlider->setEnabled(false);
            ui->cacheThresholdLabel->setEnabled(false);
        }
    } else {
        ui->cacheThresholdCheckBox->setEnabled(false);
        ui->cacheThresholdSlider->setEnabled(false);
        ui->cacheThresholdLabel->setEnabled(false);
    }
}

void SettingsWindow::on_gridColorButton_clicked()
{
    QColor curColor = settings.value("OverlayGrid/Color").value<QColor>();
    QColor newColor = QColorDialog::getColor(curColor, this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
    settings.setValue("OverlayGrid/Color", newColor);
}

void SettingsWindow::on_bgColorButton_clicked()
{
    QColor curColor = settings.value("Background/Color").value<QColor>();
    QColor newColor = QColorDialog::getColor(curColor, this, tr("Select background color"));
    settings.setValue("Background/Color", newColor);
}

void SettingsWindow::on_cancelButton_clicked()
{
    loadSettings();
    close();
}

void SettingsWindow::on_simplifyColorButton_clicked()
{
    QColor curColor = settings.value("Statistics/SimplificationColor").value<QColor>();
    QColor newColor = QColorDialog::getColor(curColor, this, tr("Select grid color for simplified statistics"));
    settings.setValue("Statistics/SimplificationColor", newColor);
}
