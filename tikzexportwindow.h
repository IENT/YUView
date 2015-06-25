#ifndef TIKZEXPORTWINDOW_H
#define TIKZEXPORTWINDOW_H

#include <QWidget>
#include <QVariant>

#include <plistserializer.h>
#include <plistparser.h>
#include <tikzextensions.h>


namespace Ui {
class TikzExportWindow;
}

class TikzExportWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TikzExportWindow(QWidget *parent = 0);
    TikzDrawSettings getSettings(){return settings;}
    
signals:
    void exportToTikZ();
    void updateSettings();
    
public slots:

private slots:
    void on_exportButton_clicked();
    void on_saveButton_clicked();
    void on_importButton_clicked();
    void on_toGrid_clicked();
    void on_toPlain_clicked();
    void on_toBW_clicked();


private:
    Ui::TikzExportWindow *ui;
    TikzDrawSettings settings;
    
};

#endif // TIKZEXPORTWINDOW_H
