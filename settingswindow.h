#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QWidget>
#include <QSettings>

namespace Ui {
class SettingsWindow;
}

class SettingsWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit SettingsWindow(QWidget *parent = 0);
    ~SettingsWindow();
    unsigned int getCacheSizeInMB();

signals:
    void settingsChanged();

private slots:
    void on_saveButton_clicked();

    void on_cacheThresholdSlider_valueChanged(int value);

    void on_cacheCheckBox_stateChanged(int);

    void on_gridColorButton_clicked();

    void on_cancelButton_clicked();

    void on_simplifyColorButton_clicked();

private:
    bool saveSettings();
    bool loadSettings();

    unsigned int p_memSizeInMB;

    Ui::SettingsWindow *ui;
    QSettings settings;
};

#endif // SETTINGSWINDOW_H
