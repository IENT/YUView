#include "tikzexportwindow.h"
#include "ui_tikzexportwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QBuffer>


TikzExportWindow::TikzExportWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TikzExportWindow)
{
    ui->setupUi(this);
}

void TikzExportWindow::on_exportButton_clicked()
{
    exportToTikZ();
    close();
}

void TikzExportWindow::on_toGrid_clicked()
{
    settings.snapToGrid = ui->toGrid->isChecked();
    updateSettings();
}

void TikzExportWindow::on_toBW_clicked()
{
    settings.setBW = ui->toBW->isChecked();
    updateSettings();
}

void TikzExportWindow::on_toPlain_clicked()
{
    settings.plainText = ui->toPlain->isChecked();
    updateSettings();
}

void TikzExportWindow::on_saveButton_clicked()
{
    QVariantMap globalSettings;
    //QString filename = QFileDialog::getSaveFileName(this, tr("Save Playlist"), "", tr("Playlist Files (*.yuvplaylist)"));

    QString filename = "settings.tikzexport";

    globalSettings["toGrid"] = settings.snapToGrid;
    globalSettings["toBW"] = settings.setBW;
    globalSettings["toPlain"] = settings.plainText;

    QString plistFileContents = PListSerializer::toPList(globalSettings);

    QFile file( filename );
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream outStream(&file);
    outStream << plistFileContents;
    file.close();


}

void TikzExportWindow::on_importButton_clicked()
{

    QFile file("settings.tikzexport");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray fileBytes = file.readAll();
    QBuffer buffer(&fileBytes);

    QVariantMap map = PListParser::parsePList(&buffer).toMap();

    bool bw = map["toBW"].toBool();
    bool grid = map["toGrid"].toBool();
    bool plain = map["toPlain"].toBool();

    settings.setBW = bw;
    settings.plainText = plain;
    settings.snapToGrid = grid;

    ui->toBW->setChecked(bw);
    ui->toPlain->setChecked(plain);
    ui->toGrid->setChecked(grid);


}
