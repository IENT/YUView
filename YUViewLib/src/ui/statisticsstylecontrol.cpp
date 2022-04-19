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

#include <common/FunctionsGui.h>
#include <common/Typedef.h>
#include <statistics/StatisticsType.h>
#include <ui/statisticsStyleControl_ColorMapEditor.h>

#include <QColorDialog>
#include <algorithm>

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
  this->ui.setupUi(this);

  this->ui.frameGradientStartColor->setPlainColor(QColor(0, 0, 0));
  this->ui.frameGradientEndColor->setPlainColor(QColor(0, 0, 255));
  this->ui.frameDataColor->setRenderRangeValues(true);

  for (auto typeName : stats::color::PredefinedTypeMapper.getNames())
    this->ui.comboBoxPreset->addItem(QString::fromStdString(typeName));
}

void StatisticsStyleControl::setStatsItem(stats::StatisticsType *item)
{
  DEBUG_STAT_STYLE("StatisticsStyleControl::setStatsItem %s", item->typeName.toStdString().c_str());
  this->currentItem = item;
  this->setWindowTitle("Edit statistics rendering: " + this->currentItem->typeName);

  if (this->currentItem->hasValueData)
  {
    this->ui.groupBoxBlockData->show();
    const auto &colorMapper = this->currentItem->colorMapper;

    this->ui.frameDataColor->setColorMapper(colorMapper);

    this->ui.comboBoxPreset->setCurrentIndex(
        stats::color::PredefinedTypeMapper.indexOf(colorMapper.predefinedType));
    this->ui.spinBoxPresetRangeMin->setValue(colorMapper.valueRange.min);
    this->ui.spinBoxPresetRangeMax->setValue(colorMapper.valueRange.max);

    this->ui.frameGradientStartColor->setPlainColor(
        functionsGui::toQColor(colorMapper.gradientColorStart));
    this->ui.frameGradientEndColor->setPlainColor(
        functionsGui::toQColor(colorMapper.gradientColorEnd));
    this->ui.spinBoxGradientRangeMin->setValue(colorMapper.valueRange.min);
    this->ui.spinBoxGradientRangeMax->setValue(colorMapper.valueRange.max);
  }
  else
    this->ui.groupBoxBlockData->hide();

  // Does this statistics type have vector data to show?
  if (this->currentItem->hasVectorData)
  {
    this->ui.groupBoxVector->show();

    // Update all the values in the vector controls
    auto penStyleIndex = indexInVec(stats::AllPatterns, this->currentItem->vectorStyle.pattern);
    if (penStyleIndex != -1)
      this->ui.comboBoxVectorLineStyle->setCurrentIndex(penStyleIndex);
    this->ui.doubleSpinBoxVectorLineWidth->setValue(this->currentItem->vectorStyle.width);
    this->ui.checkBoxVectorScaleToZoom->setChecked(this->currentItem->scaleVectorToZoom);
    this->ui.comboBoxVectorHeadStyle->setCurrentIndex(int(this->currentItem->arrowHead));
    this->ui.checkBoxVectorMapToColor->setChecked(this->currentItem->mapVectorToColor);
    this->ui.colorFrameVectorColor->setPlainColor(
        functionsGui::toQColor(this->currentItem->vectorStyle.color));
    this->ui.colorFrameVectorColor->setEnabled(!this->currentItem->mapVectorToColor);
    this->ui.pushButtonEditVectorColor->setEnabled(!this->currentItem->mapVectorToColor);
  }
  else
    this->ui.groupBoxVector->hide();

  this->ui.frameGridColor->setPlainColor(
      functionsGui::toQColor(this->currentItem->gridStyle.color));
  this->ui.doubleSpinBoxGridLineWidth->setValue(this->currentItem->gridStyle.width);
  this->ui.checkBoxGridScaleToZoom->setChecked(this->currentItem->scaleGridToZoom);

  // Convert the current pen style to an index and set it in the comboBoxGridLineStyle
  auto penStyleIndex = indexInVec(stats::AllPatterns, this->currentItem->vectorStyle.pattern);
  if (penStyleIndex != -1)
    this->ui.comboBoxGridLineStyle->setCurrentIndex(penStyleIndex);

  this->resize(sizeHint());
}

void StatisticsStyleControl::on_groupBoxVector_clicked(bool check)
{
  this->currentItem->renderVectorData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_groupBoxBlockData_clicked(bool check)
{
  this->currentItem->renderValueData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxScaleValueToBlockSize_stateChanged(int val)
{
  this->currentItem->scaleValueToBlockSize = (val != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_blockDataTab_currentChanged(int index)
{
}

void StatisticsStyleControl::on_comboBoxPreset_currentIndexChanged(int index)
{
}

void StatisticsStyleControl::on_spinBoxPresetRangeMin_valueChanged(int val)
{
}

void StatisticsStyleControl::on_spinBoxPresetRangeMax_valueChanged(int val)
{
}

void StatisticsStyleControl::on_frameGradientStartColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem->gridStyle.color),
                             this,
                             tr("Select color range minimum"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && this->currentItem->colorMapper.gradientColorStart != newColor)
  {
    this->currentItem->colorMapper.gradientColorStart = newColor;
    this->ui.frameGradientStartColor->setPlainColor(newQColor);
    this->ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonGradientEditStartColor_clicked()
{
  this->on_frameGradientStartColor_clicked();
};

void StatisticsStyleControl::on_frameGradientEndColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem->gridStyle.color),
                             this,
                             tr("Select color range maximum"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && this->currentItem->colorMapper.gradientColorEnd != newColor)
  {
    this->currentItem->colorMapper.gradientColorEnd = newColor;
    this->ui.frameGradientEndColor->setPlainColor(newQColor);
    this->ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonGradientEditEndColor_clicked()
{
  this->on_frameGradientEndColor_clicked();
};

void StatisticsStyleControl::on_spinBoxGradientRangeMin_valueChanged(int val)
{
}

void StatisticsStyleControl::on_spinBoxGradientRangeMax_valueChanged(int val)
{
}

void StatisticsStyleControl::on_comboBoxCustomMap_currentIndexChanged(int index)
{
}

void StatisticsStyleControl::on_pushButtonEditMap_clicked()
{
  std::map<int, Color> colorMap;
  auto                 otherColor = this->currentItem->colorMapper.colorMapOther;

  if (this->currentItem->colorMapper.mappingType == stats::color::MappingType::Map)
    // Edit the currently set color map
    colorMap = this->currentItem->colorMapper.colorMap;
  else
  {
    // Convert the currently selected range to a map and let the user edit that
    auto lower  = std::min(this->currentItem->colorMapper.valueRange.min,
                          this->currentItem->colorMapper.valueRange.max);
    auto higher = std::max(this->currentItem->colorMapper.valueRange.min,
                           this->currentItem->colorMapper.valueRange.max);
    for (int i = lower; i <= higher; i++)
      colorMap[i] = this->currentItem->colorMapper.getColor(i);
  }

  StatisticsStyleControl_ColorMapEditor colorMapEditor(colorMap, otherColor, this);
  if (colorMapEditor.exec() == QDialog::Accepted)
  {
    // Set the new color map
    this->currentItem->colorMapper.colorMap      = colorMapEditor.getColorMap();
    this->currentItem->colorMapper.colorMapOther = colorMapEditor.getOtherColor();

    // // Select the color map (if not yet set)
    // if (this->ui.comboBoxDataColorMap->currentIndex() != 1)
    //   // This will also set the color map and emit the style change signal
    //   this->ui.comboBoxDataColorMap->setCurrentIndex(1);
    // else
    // {
    //   this->ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
    //   emit StyleChanged();
    // }
  }
}

void StatisticsStyleControl::on_pushButtonSaveMap_clicked()
{
}

void StatisticsStyleControl::on_pushButtonDeleteMap_clicked()
{
}

// void StatisticsStyleControl::on_comboBoxDataColorMap_currentIndexChanged(int index)
// {
//   const auto isCustomRange = (index == 0);
//   const auto isMap         = (index == 1);

//   this->ui.frameMinColor->setEnabled(isCustomRange);
//   this->ui.pushButtonEditMinColor->setEnabled(isCustomRange);
//   this->ui.frameMaxColor->setEnabled(isCustomRange);
//   this->ui.pushButtonEditMaxColor->setEnabled(isCustomRange);
//   this->ui.spinBoxRangeMin->setEnabled(!isMap);
//   this->ui.spinBoxRangeMax->setEnabled(!isMap);

//   // If a color map is selected, the button will edit it. If no color map is selected, the button
//   // will convert the color mapping to a map and edit/set that one.
//   const auto editButton = this->ui.pushButtonEditColorMap;
//   if (isMap)
//   {
//     editButton->setIcon(functionsGui::convertIcon(":img_convert.png"));
//     editButton->setToolTip("Edit colormap");
//     editButton->setWhatsThis("Edit colormap");
//   }
//   else
//   {
//     editButton->setIcon(functionsGui::convertIcon(":img_convert.png"));
//     editButton->setToolTip("Convert current mapping to a colormap");
//     editButton->setWhatsThis("Convert current mapping to a colormap");
//   }

//   if (isCustomRange)
//   {
//     this->currentItem->colorMapper.mappingType = stats::ColorMapper::MappingType::gradient;
//     this->currentItem->colorMapper.rangeMin    = this->ui.spinBoxRangeMin->value();
//     this->currentItem->colorMapper.rangeMax    = this->ui.spinBoxRangeMax->value();

//     this->currentItem->colorMapper.minColor =
//         functionsGui::toColor(ui.frameMinColor->getPlainColor());
//     this->currentItem->colorMapper.maxColor =
//         functionsGui::toColor(ui.frameMaxColor->getPlainColor());
//   }
//   else if (isMap)
//     this->currentItem->colorMapper.mappingType = stats::ColorMapper::MappingType::map;
//   else
//   {
//     if (index - 2 < stats::ColorMapper::supportedComplexTypes.length())
//     {
//       this->currentItem->colorMapper.mappingType = stats::ColorMapper::MappingType::complex;
//       this->currentItem->colorMapper.rangeMin    = this->ui.spinBoxRangeMin->value();
//       this->currentItem->colorMapper.rangeMax    = this->ui.spinBoxRangeMax->value();
//       this->currentItem->colorMapper.complexType =
//           stats::ColorMapper::supportedComplexTypes[index - 2];
//     }
//   }

//   this->ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
//   emit StyleChanged();
// }

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
    this->ui.colorFrameVectorColor->setPlainColor(newQColor);
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
    this->ui.frameGridColor->setPlainColor(newQColor);
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

// void StatisticsStyleControl::on_spinBoxRangeMin_valueChanged(int minVal)
// {
//   this->currentItem->colorMapper.rangeMin = minVal;
//   ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
//   emit StyleChanged();
// }

// void StatisticsStyleControl::on_spinBoxRangeMax_valueChanged(int maxVal)
// {
//   this->currentItem->colorMapper.rangeMax = maxVal;
//   ui.frameDataColor->setColorMapper(this->currentItem->colorMapper);
//   emit StyleChanged();
// }
