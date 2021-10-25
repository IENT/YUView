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

#include <QColorDialog>
#include <algorithm>

#include <common/FunctionsGui.h>
#include <common/typedef.h>
#include <statistics/StatisticsType.h>
#include <ui/statisticsStyleControl_ColorMapEditor.h>

namespace
{

#define STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT 0
#if STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_STAT_STYLE qDebug
#else
#define DEBUG_STAT_STYLE(fmt, ...) ((void)0)
#endif

} // namespace

StatisticsStyleControl::StatisticsStyleControl(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint)
{
  ui.setupUi(this);
  ui.pushButtonEditMinColor->setIcon(functionsGui::convertIcon(":img_edit.png"));
  ui.pushButtonEditMaxColor->setIcon(functionsGui::convertIcon(":img_edit.png"));
  ui.pushButtonEditVectorColor->setIcon(functionsGui::convertIcon(":img_edit.png"));
  ui.pushButtonEditGridColor->setIcon(functionsGui::convertIcon(":img_edit.png"));

  // The default custom range is black to blue
  ui.frameMinColor->setPlainColor(QColor(0, 0, 0));
  ui.frameMaxColor->setPlainColor(QColor(0, 0, 255));

  // For the preview, render the value range
  ui.frameDataColor->setRenderRangeValues(true);
}

void StatisticsStyleControl::setStatsItem(stats::StatisticsType *item)
{
  DEBUG_STAT_STYLE("StatisticsStyleControl::setStatsItem %s", item->typeName.toStdString().c_str());
  currentItem = item;
  setWindowTitle("Edit statistics rendering: " + this->currentItem->typeName);

  // Does this statistics type have any value data to show? Show the controls only if it does.
  if (this->currentItem->hasValueData)
  {
    ui.groupBoxBlockData->show();

    ui.spinBoxRangeMin->setValue(this->currentItem->colorMapper.getMinVal());
    ui.spinBoxRangeMax->setValue(this->currentItem->colorMapper.getMaxVal());
    ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);

    // Update all the values in the block data controls.
    ui.comboBoxDataColorMap->setCurrentIndex((int)this->currentItem->colorMapper.getID());
    if (this->currentItem->colorMapper.getID() == 0)
    {
      // The color range is a custom range
      // Enable/setup the controls for the minimum and maximum color
      ui.frameMinColor->setEnabled(true);
      ui.pushButtonEditMinColor->setEnabled(true);
      ui.frameMinColor->setPlainColor(
          functionsGui::toQColor(this->currentItem->colorMapper.minColor));
      ui.frameMaxColor->setEnabled(true);
      ui.pushButtonEditMaxColor->setEnabled(true);
      ui.frameMaxColor->setPlainColor(
          functionsGui::toQColor(this->currentItem->colorMapper.maxColor));
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
  if (this->currentItem->hasVectorData)
  {
    ui.groupBoxVector->show();

    // Update all the values in the vector controls
    auto penStyleIndex = indexInVec(stats::AllPatterns, this->currentItem->vectorStyle.pattern);
    if (penStyleIndex != -1)
      ui.comboBoxVectorLineStyle->setCurrentIndex(penStyleIndex);
    ui.doubleSpinBoxVectorLineWidth->setValue(this->currentItem->vectorStyle.width);
    ui.checkBoxVectorScaleToZoom->setChecked(this->currentItem->scaleVectorToZoom);
    ui.comboBoxVectorHeadStyle->setCurrentIndex(int(this->currentItem->arrowHead));
    ui.checkBoxVectorMapToColor->setChecked(this->currentItem->mapVectorToColor);
    ui.colorFrameVectorColor->setPlainColor(
        functionsGui::toQColor(this->currentItem->vectorStyle.color));
    ui.colorFrameVectorColor->setEnabled(!this->currentItem->mapVectorToColor);
    ui.pushButtonEditVectorColor->setEnabled(!this->currentItem->mapVectorToColor);
  }
  else
    ui.groupBoxVector->hide();

  ui.frameGridColor->setPlainColor(functionsGui::toQColor(this->currentItem->gridStyle.color));
  ui.doubleSpinBoxGridLineWidth->setValue(this->currentItem->gridStyle.width);
  ui.checkBoxGridScaleToZoom->setChecked(this->currentItem->scaleGridToZoom);

  // Convert the current pen style to an index and set it in the comboBoxGridLineStyle
  auto penStyleIndex = indexInVec(stats::AllPatterns, this->currentItem->vectorStyle.pattern);
  if (penStyleIndex != -1)
    ui.comboBoxGridLineStyle->setCurrentIndex(penStyleIndex);

  this->resize(sizeHint());
}

void StatisticsStyleControl::on_groupBoxBlockData_clicked(bool check)
{
  this->currentItem->renderValueData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_groupBoxVector_clicked(bool check)
{
  this->currentItem->renderVectorData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxDataColorMap_currentIndexChanged(int index)
{
  const auto isCustomRange = (index == 0);
  const auto isMap         = (index == 1);

  // Enable/Disable the color min/max controls
  ui.frameMinColor->setEnabled(isCustomRange);
  ui.pushButtonEditMinColor->setEnabled(isCustomRange);
  ui.frameMaxColor->setEnabled(isCustomRange);
  ui.pushButtonEditMaxColor->setEnabled(isCustomRange);
  ui.spinBoxRangeMin->setEnabled(!isMap);
  ui.spinBoxRangeMax->setEnabled(!isMap);

  // If a color map is selected, the button will edit it. If no color map is selected, the button
  // will convert the color mapping to a map and edit/set that one.
  if (isMap)
    ui.pushButtonEditColorMap->setText("Edit Color Map");
  else
    ui.pushButtonEditColorMap->setText("Convert to Color Map");

  if (isCustomRange)
  {
    this->currentItem->colorMapper.mappingType = stats::ColorMapper::MappingType::gradient;
    this->currentItem->colorMapper.rangeMin    = ui.spinBoxRangeMin->value();
    this->currentItem->colorMapper.rangeMax    = ui.spinBoxRangeMax->value();

    this->currentItem->colorMapper.minColor =
        functionsGui::toColor(ui.frameMinColor->getPlainColor());
    this->currentItem->colorMapper.maxColor =
        functionsGui::toColor(ui.frameMaxColor->getPlainColor());
  }
  else if (isMap)
    this->currentItem->colorMapper.mappingType = stats::ColorMapper::MappingType::map;
  else
  {
    if (index - 2 < stats::ColorMapper::supportedComplexTypes.length())
    {
      this->currentItem->colorMapper.mappingType = stats::ColorMapper::MappingType::complex;
      this->currentItem->colorMapper.rangeMin    = ui.spinBoxRangeMin->value();
      this->currentItem->colorMapper.rangeMax    = ui.spinBoxRangeMax->value();
      this->currentItem->colorMapper.complexType =
          stats::ColorMapper::supportedComplexTypes[index - 2];
    }
  }

  ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameMinColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem->gridStyle.color),
                             this,
                             tr("Select color range minimum"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && this->currentItem->colorMapper.minColor != newColor)
  {
    this->currentItem->colorMapper.minColor = newColor;
    ui.frameMinColor->setPlainColor(newQColor);
    ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_frameMaxColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem->gridStyle.color),
                             this,
                             tr("Select color range maximum"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && this->currentItem->colorMapper.maxColor != newColor)
  {
    this->currentItem->colorMapper.maxColor = newColor;
    ui.frameMaxColor->setPlainColor(newQColor);
    ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonEditColorMap_clicked()
{
  std::map<int, Color> colorMap;
  auto                 otherColor = this->currentItem->colorMapper.colorMapOther;

  if (this->currentItem->colorMapper.mappingType == stats::ColorMapper::MappingType::map)
    // Edit the currently set color map
    colorMap = this->currentItem->colorMapper.colorMap;
  else
  {
    // Convert the currently selected range to a map and let the user edit that
    auto lower  = std::min(this->currentItem->colorMapper.getMinVal(),
                          this->currentItem->colorMapper.getMaxVal());
    auto higher = std::max(this->currentItem->colorMapper.getMinVal(),
                           this->currentItem->colorMapper.getMaxVal());
    for (int i = lower; i <= higher; i++)
      colorMap[i] = this->currentItem->colorMapper.getColor(i);
  }

  StatisticsStyleControl_ColorMapEditor colorMapEditor(colorMap, otherColor, this);
  if (colorMapEditor.exec() == QDialog::Accepted)
  {
    // Set the new color map
    this->currentItem->colorMapper.colorMap      = colorMapEditor.getColorMap();
    this->currentItem->colorMapper.colorMapOther = colorMapEditor.getOtherColor();

    // Select the color map (if not yet set)
    if (ui.comboBoxDataColorMap->currentIndex() != 1)
      // This will also set the color map and emit the style change signal
      ui.comboBoxDataColorMap->setCurrentIndex(1);
    else
    {
      ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
      emit StyleChanged();
    }
  }
}

void StatisticsStyleControl::on_checkBoxScaleValueToBlockSize_stateChanged(int arg1)
{
  this->currentItem->scaleValueToBlockSize = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxVectorLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  auto pattern                           = stats::AllPatterns.at(index);
  this->currentItem->vectorStyle.pattern = pattern;
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxVectorLineWidth_valueChanged(double width)
{
  this->currentItem->vectorStyle.width = width;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorScaleToZoom_stateChanged(int arg1)
{
  this->currentItem->scaleVectorToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxVectorHeadStyle_currentIndexChanged(int index)
{
  this->currentItem->arrowHead = (stats::StatisticsType::ArrowHead)(index);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorMapToColor_stateChanged(int arg1)
{
  this->currentItem->mapVectorToColor = (arg1 != 0);
  ui.colorFrameVectorColor->setEnabled(!this->currentItem->mapVectorToColor);
  ui.pushButtonEditVectorColor->setEnabled(!this->currentItem->mapVectorToColor);
  emit StyleChanged();
}

void StatisticsStyleControl::on_colorFrameVectorColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem->vectorStyle.color),
                             this,
                             tr("Select vector color"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && newColor != this->currentItem->vectorStyle.color)
  {
    this->currentItem->vectorStyle.color = newColor;
    ui.colorFrameVectorColor->setPlainColor(newQColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_groupBoxGrid_clicked(bool check)
{
  this->currentItem->renderGrid = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameGridColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem->gridStyle.color),
                             this,
                             tr("Select grid color"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && newColor != this->currentItem->gridStyle.color)
  {
    this->currentItem->gridStyle.color = newColor;
    ui.frameGridColor->setPlainColor(newQColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_comboBoxGridLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  auto pattern                         = stats::AllPatterns.at(index);
  this->currentItem->gridStyle.pattern = pattern;
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxGridLineWidth_valueChanged(double width)
{
  this->currentItem->gridStyle.width = width;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxGridScaleToZoom_stateChanged(int arg1)
{
  this->currentItem->scaleGridToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxRangeMin_valueChanged(int minVal)
{
  this->currentItem->colorMapper.rangeMin = minVal;
  ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxRangeMax_valueChanged(int maxVal)
{
  this->currentItem->colorMapper.rangeMax = maxVal;
  ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
  emit StyleChanged();
}
