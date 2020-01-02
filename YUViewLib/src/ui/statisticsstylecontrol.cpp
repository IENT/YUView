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

#include "statisticsstylecontrol.h"

#include <algorithm>
#include <QColorDialog>

#include "statisticsStyleControl_ColorMapEditor.h"
#include "common/functions.h"
#include "common/typedef.h"

#define STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT 0
#if STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_STAT_STYLE qDebug
#else
#define DEBUG_STAT_STYLE(fmt,...) ((void)0)
#endif

const QList<Qt::PenStyle> StatisticsStyleControl::penStyleList = QList<Qt::PenStyle>() << Qt::SolidLine << Qt::DashLine << Qt::DotLine << Qt::DashDotLine << Qt::DashDotDotLine;

StatisticsStyleControl::StatisticsStyleControl(QWidget *parent) :
  QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint)
{
  ui.setupUi(this);
  ui.pushButtonEditMinColor->setIcon(functions::convertIcon(":img_edit.png"));
  ui.pushButtonEditMaxColor->setIcon(functions::convertIcon(":img_edit.png"));
  ui.pushButtonEditVectorColor->setIcon(functions::convertIcon(":img_edit.png"));
  ui.pushButtonEditGridColor->setIcon(functions::convertIcon(":img_edit.png"));

  // The default custom range is black to blue
  ui.frameMinColor->setPlainColor(QColor(0, 0, 0));
  ui.frameMaxColor->setPlainColor(QColor(0, 0, 255));

  // For the preview, render the value range
  ui.frameDataColor->setRenderRangeValues(true);
}

void StatisticsStyleControl::setStatsItem(StatisticsType *item)
{
  DEBUG_STAT_STYLE("StatisticsStyleControl::setStatsItem %s", item->typeName.toStdString().c_str());
  currentItem = item;
  setWindowTitle("Edit statistics rendering: " + currentItem->typeName);

  // Does this statistics type have any value data to show? Show the controls only if it does.
  if (currentItem->hasValueData)
  {
    ui.groupBoxBlockData->show();

    ui.spinBoxRangeMin->setValue(currentItem->colMapper.getMinVal());
    ui.spinBoxRangeMax->setValue(currentItem->colMapper.getMaxVal());
    ui.frameDataColor->setColorMapper(currentItem->colMapper);

    // Update all the values in the block data controls.
    ui.comboBoxDataColorMap->setCurrentIndex((int)currentItem->colMapper.getID());
    if (currentItem->colMapper.getID() == 0)
    {
      // The color range is a custom range
      // Enable/setup the controls for the minimum and maximum color
      ui.frameMinColor->setEnabled(true);
      ui.pushButtonEditMinColor->setEnabled(true);
      ui.frameMinColor->setPlainColor(currentItem->colMapper.minColor);
      ui.frameMaxColor->setEnabled(true);
      ui.pushButtonEditMaxColor->setEnabled(true);
      ui.frameMaxColor->setPlainColor(currentItem->colMapper.maxColor);
    }
    else
    {
      // The color range is one of the predefined default color maps
      // Disable the color min/max controls
      ui.frameMinColor->setEnabled(false);
      ui.pushButtonEditMinColor->setEnabled(false);
      ui.frameMaxColor->setEnabled(false);
      ui.pushButtonEditMaxColor->setEnabled(false);
    }
  }
  else
    ui.groupBoxBlockData->hide();

  // Does this statistics type have vector data to show?
  if (currentItem->hasVectorData)
  {
    ui.groupBoxVector->show();

    // Update all the values in the vector controls
    int penStyleIndex = penStyleList.indexOf(currentItem->vectorPen.style());
    if (penStyleIndex != -1)
      ui.comboBoxVectorLineStyle->setCurrentIndex(penStyleIndex);
    ui.doubleSpinBoxVectorLineWidth->setValue(currentItem->vectorPen.widthF());
    ui.checkBoxVectorScaleToZoom->setChecked(currentItem->scaleVectorToZoom);
    ui.comboBoxVectorHeadStyle->setCurrentIndex((int)currentItem->arrowHead);
    ui.checkBoxVectorMapToColor->setChecked(currentItem->mapVectorToColor);
    ui.colorFrameVectorColor->setPlainColor(currentItem->vectorPen.color());
    ui.colorFrameVectorColor->setEnabled(!currentItem->mapVectorToColor);
    ui.pushButtonEditVectorColor->setEnabled(!currentItem->mapVectorToColor);
  }
  else
    ui.groupBoxVector->hide();

  ui.frameGridColor->setPlainColor(currentItem->gridPen.color());
  ui.doubleSpinBoxGridLineWidth->setValue(currentItem->gridPen.widthF());
  ui.checkBoxGridScaleToZoom->setChecked(currentItem->scaleGridToZoom);

  // Convert the current pen style to an index and set it in the comboBoxGridLineStyle
  int penStyleIndex = penStyleList.indexOf(currentItem->gridPen.style());
  if (penStyleIndex != -1)
    ui.comboBoxGridLineStyle->setCurrentIndex(penStyleIndex);

  resize(sizeHint());
}

void StatisticsStyleControl::on_groupBoxBlockData_clicked(bool check)
{
  currentItem->renderValueData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_groupBoxVector_clicked(bool check)
{
  currentItem->renderVectorData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxDataColorMap_currentIndexChanged(int index)
{
  // Enable/Disable the color min/max controls
  ui.frameMinColor->setEnabled(index == 0);
  ui.pushButtonEditMinColor->setEnabled(index == 0);
  ui.frameMaxColor->setEnabled(index == 0);
  ui.pushButtonEditMaxColor->setEnabled(index == 0);
  ui.spinBoxRangeMin->setEnabled(index != 1);
  ui.spinBoxRangeMax->setEnabled(index != 1);

  // If a color map is selected, the button will edit it. If no color map is selected, the button
  // will convert the color mapping to a map and edit/set that one.
  if (index == 1)
    ui.pushButtonEditColorMap->setText("Edit Color Map");
  else
    ui.pushButtonEditColorMap->setText("Convert to Color Map");

  if (index == 0)
  {
    // A custom range is selected
    currentItem->colMapper.type = colorMapper::mappingType::gradient;
    currentItem->colMapper.rangeMin = ui.spinBoxRangeMin->value();
    currentItem->colMapper.rangeMax = ui.spinBoxRangeMax->value();
    currentItem->colMapper.minColor = ui.frameMinColor->getPlainColor();
    currentItem->colMapper.maxColor = ui.frameMaxColor->getPlainColor();
  }
  else if (index == 1)
    // A map is selected
    currentItem->colMapper.type = colorMapper::mappingType::map;
  else
  {
    if (index-2 < colorMapper::supportedComplexTypes.length())
    {
      currentItem->colMapper.type = colorMapper::mappingType::complex;
      currentItem->colMapper.rangeMin = ui.spinBoxRangeMin->value();
      currentItem->colMapper.rangeMax = ui.spinBoxRangeMax->value();
      currentItem->colMapper.complexType = colorMapper::supportedComplexTypes[index-2];
    }
  }

  ui.frameDataColor->setColorMapper(currentItem->colMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameMinColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select color range minimum"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentItem->colMapper.minColor != newColor)
  {
    currentItem->colMapper.minColor = newColor;
    ui.frameMinColor->setPlainColor(newColor);
    ui.frameDataColor->setColorMapper(currentItem->colMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_frameMaxColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select color range maximum"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentItem->colMapper.maxColor != newColor)
  {
    currentItem->colMapper.maxColor = newColor;
    ui.frameMaxColor->setPlainColor(newColor);
    ui.frameDataColor->setColorMapper(currentItem->colMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonEditColorMap_clicked()
{
  QMap<int, QColor> colorMap;
  QColor otherColor = currentItem->colMapper.colorMapOther;

  if (currentItem->colMapper.type == colorMapper::mappingType::map)
    // Edit the currently set color map
    colorMap = currentItem->colMapper.colorMap;
  else
  {
    // Convert the currently selected range to a map and let the user edit that
    int lower = std::min(currentItem->colMapper.getMinVal(), currentItem->colMapper.getMaxVal());
    int higher = std::max(currentItem->colMapper.getMinVal(), currentItem->colMapper.getMaxVal());
    for (int i = lower; i <= higher; i++)
      colorMap.insert(i, currentItem->colMapper.getColor(i));
  }

  StatisticsStyleControl_ColorMapEditor *colorMapEditor = new StatisticsStyleControl_ColorMapEditor(colorMap, otherColor, this);
  if (colorMapEditor->exec() == QDialog::Accepted)
  {
    // Set the new color map
    currentItem->colMapper.colorMap = colorMapEditor->getColorMap();
    currentItem->colMapper.colorMapOther = colorMapEditor->getOtherColor();

    // Select the color map (if not yet set)
    if (ui.comboBoxDataColorMap->currentIndex() != 1)
      // This will also set the color map and emit the style change signal
      ui.comboBoxDataColorMap->setCurrentIndex(1);
    else
    {
      ui.frameDataColor->setColorMapper(currentItem->colMapper);
      emit StyleChanged();
    }
  }
}

void StatisticsStyleControl::on_checkBoxScaleValueToBlockSize_stateChanged(int arg1)
{
  currentItem->scaleValueToBlockSize = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxVectorLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  Qt::PenStyle penStyle = penStyleList.at(index);
  currentItem->vectorPen.setStyle(penStyle);
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxVectorLineWidth_valueChanged(double arg1)
{
  currentItem->vectorPen.setWidthF(arg1);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorScaleToZoom_stateChanged(int arg1)
{
  currentItem->scaleVectorToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxVectorHeadStyle_currentIndexChanged(int index)
{
  currentItem->arrowHead = (StatisticsType::arrowHead_t)(index);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorMapToColor_stateChanged(int arg1)
{
  currentItem->mapVectorToColor = (arg1 != 0);
  ui.colorFrameVectorColor->setEnabled(!currentItem->mapVectorToColor);
  ui.pushButtonEditVectorColor->setEnabled(!currentItem->mapVectorToColor);
  emit StyleChanged();
}

void StatisticsStyleControl::on_colorFrameVectorColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select vector color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && newColor != currentItem->vectorPen.color())
  {
    currentItem->vectorPen.setColor(newColor);
    ui.colorFrameVectorColor->setPlainColor(currentItem->vectorPen.color());
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_groupBoxGrid_clicked(bool check)
{
  currentItem->renderGrid = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameGridColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && newColor != currentItem->gridPen.color())
  {
    currentItem->gridPen.setColor(newColor);
    ui.frameGridColor->setPlainColor(currentItem->gridPen.color());
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_comboBoxGridLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  Qt::PenStyle penStyle = penStyleList.at(index);
  currentItem->gridPen.setStyle(penStyle);
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxGridLineWidth_valueChanged(double arg1)
{
  currentItem->gridPen.setWidthF(arg1);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxGridScaleToZoom_stateChanged(int arg1)
{
  currentItem->scaleGridToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxRangeMin_valueChanged(int arg1)
{
  currentItem->colMapper.rangeMin = arg1;
  ui.frameDataColor->setColorMapper(currentItem->colMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxRangeMax_valueChanged(int arg1)
{
  currentItem->colMapper.rangeMax = arg1;
  ui.frameDataColor->setColorMapper(currentItem->colMapper);
  emit StyleChanged();
}
