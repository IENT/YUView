/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

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
