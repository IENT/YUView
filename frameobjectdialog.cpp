#include "frameobjectdialog.h"
#include "ui_frameobjectdialog.h"

FrameObjectDialog::FrameObjectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameObjectDialog)
{
    currentFont.setPointSize(20);
    currentText="Frame";
    currentDuration=3.0;
    ui->setupUi(this);
    ui->textEdit->setText(currentText);
    ui->doubleSpinBox->setValue(currentDuration);
}

FrameObjectDialog::~FrameObjectDialog()
{
    delete ui;
}

void FrameObjectDialog::editFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(
                    &ok, currentFont, this);
    if (ok) {
        currentFont=font;
    }
}

void FrameObjectDialog::loadItemStettings(PlaylistItemText* item)
{
    currentFont = item->displayObject()->getFont();
    currentText = item->displayObject()->getText();
    currentDuration=(double)(1.0/item->displayObject()->frameRate());
    ui->textEdit->setText(currentText);
    ui->doubleSpinBox->setValue(currentDuration);
}

void FrameObjectDialog::saveState()
{
    currentText=ui->textEdit->toPlainText();
    currentDuration=ui->doubleSpinBox->value();
}
