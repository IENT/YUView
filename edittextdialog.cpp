#include "edittextdialog.h"
#include "ui_edittextdialog.h"

EditTextDialog::EditTextDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditTextDialog)
{
    ui->setupUi(this);
}

EditTextDialog::~EditTextDialog()
{
    delete ui;
}

void EditTextDialog::editFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(
                    &ok, currentFont, this);
    if (ok) {
        currentFont=font;
    }
}

void EditTextDialog::loadItemStettings(PlaylistItemText* item)
{
    currentFont = item->displayObject()->font();
    currentText = item->displayObject()->text();
    currentDuration=item->displayObject()->duration();
    currentColor=item->displayObject()->color();
    ui->textEdit->setText(currentText);
    ui->doubleSpinBox->setValue(currentDuration);
}

void EditTextDialog::saveState()
{
    currentText=ui->textEdit->toPlainText();
    currentDuration=ui->doubleSpinBox->value();

}

void EditTextDialog::on_editColor_clicked()
{
    QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select font color"), QColorDialog::ShowAlphaChannel);
    currentColor = newColor;
}

void EditTextDialog::on_textEdit_textChanged()
{
    currentText = ui->textEdit->toPlainText();
}

void EditTextDialog::on_doubleSpinBox_valueChanged(double arg1)
{
    currentDuration=arg1;
}
