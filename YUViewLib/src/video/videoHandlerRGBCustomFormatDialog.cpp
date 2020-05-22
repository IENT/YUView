/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "videoHandlerRGBCustomFormatDialog.h"

videoHandlerRGBCustomFormatDialog::videoHandlerRGBCustomFormatDialog(const QString &rgbFormat, int bitDepth, bool planar)
{
  setupUi(this);

  // Set the default (RGB no alpha)
  rgbOrderComboBox->setCurrentIndex(0);
  alphaChannelGroupBox->setChecked(false);
  afterRGBRadioButton->setChecked(false);

  QString f = rgbFormat.toLower();
  if (f.length() == 3 || f.length() == 4)
  {
    if (f.length() == 4 && f.at(0) == 'a')
    {
      alphaChannelGroupBox->setChecked(true);
      beforeRGBRadioButton->setChecked(true);
      f.remove(0, 1); // Remove the 'a'
    }
    else if (f.length() == 4 && f.at(3) == 'a')
    {
      alphaChannelGroupBox->setChecked(true);
      afterRGBRadioButton->setChecked(true);
      f.remove(3, 1); // Remove the 'a'
    }

    for (int i = 0; i < rgbOrderComboBox->count(); i++)
      if (rgbOrderComboBox->itemText(i).toLower() == f)
      {
        rgbOrderComboBox->setCurrentIndex(i);
        break;
      }
  }

  if (bitDepth == 0)
    bitDepth = 8;
  bitDepthSpinBox->setValue(bitDepth);

  planarCheckBox->setChecked(planar);
}

QString videoHandlerRGBCustomFormatDialog::getRGBFormat() const
{ 
  QString f = rgbOrderComboBox->currentText(); 
  if (alphaChannelGroupBox->isChecked())
  {
    if (beforeRGBRadioButton->isChecked())
      f.prepend("A");
    if (afterRGBRadioButton->isChecked())
      f.append("A");
  }
  return f;
}
