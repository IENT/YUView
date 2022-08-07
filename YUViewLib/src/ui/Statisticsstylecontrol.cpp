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

#include "Statisticsstylecontrol.h"

#include <common/FunctionsGui.h>
#include <common/Typedef.h>
#include <ui/StatisticsStyleControl_ColorMapEditor.h>

#include <QColorDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <algorithm>
#include <map>

namespace
{

#define STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT 0
#if STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_STAT_STYLE qDebug
#else
#define DEBUG_STAT_STYLE(fmt, ...) ((void)0)
#endif

using MappingType = stats::color::MappingType;
using ColorMapper = stats::color::ColorMapper;

ColorMap convertNonMapTypeToColorMap(const stats::color::ColorMapper &colorMapper)
{
  ColorMap colorMap;
  auto     lower  = std::min(colorMapper.valueRange.min, colorMapper.valueRange.max);
  auto     higher = std::max(colorMapper.valueRange.min, colorMapper.valueRange.max);
  for (int i = lower; i <= higher; i++)
    colorMap[i] = colorMapper.getColor(i);
  return colorMap;
}

} // namespace

StatisticsStyleControl::StatisticsStyleControl(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint)
{
  this->ui.setupUi(this);

  this->ui.frameDataColor->setRenderRangeValues(true);

  QSignalBlocker blockerPredefined(this->ui.comboBoxPredefined);
  for (auto typeName : stats::color::PredefinedTypeMapper.getNames())
    this->ui.comboBoxPredefined->addItem(QString::fromStdString(typeName));
  this->refreshComboBoxCustomMapFromStorage();
}

void StatisticsStyleControl::setStatsItem(stats::StatisticsType item)
{
  DEBUG_STAT_STYLE("StatisticsStyleControl::setStatsItem %s", item.typeName.toStdString().c_str());
  this->currentItem = item;
  this->setWindowTitle("Edit statistics rendering: " + this->currentItem.typeName);

  if (this->currentItem.hasValueData)
  {
    this->ui.groupBoxBlockData->show();
    const auto &colorMapper = this->currentItem.colorMapper;

    this->ui.frameDataColor->setColorMapper(colorMapper);

    static const std::map<MappingType, int> MappingTypeToTabIndex(
        {{MappingType::Predefined, 0}, {MappingType::Gradient, 1}, {MappingType::Map, 2}});

    QSignalBlocker blockTabIndexChanged(this->ui.blockDataTab);
    auto           newIndex = MappingTypeToTabIndex.at(colorMapper.mappingType);
    this->ui.blockDataTab->setCurrentIndex(newIndex);
    this->on_blockDataTab_currentChanged(newIndex);
  }
  else
    this->ui.groupBoxBlockData->hide();

  if (this->currentItem.hasVectorData)
  {
    this->ui.groupBoxVector->show();

    auto penStyleIndex = indexInVec(stats::AllPatterns, this->currentItem.vectorStyle.pattern);
    if (penStyleIndex != -1)
      this->ui.comboBoxVectorLineStyle->setCurrentIndex(penStyleIndex);
    this->ui.doubleSpinBoxVectorLineWidth->setValue(this->currentItem.vectorStyle.width);
    this->ui.checkBoxVectorScaleToZoom->setChecked(this->currentItem.scaleVectorToZoom);
    this->ui.comboBoxVectorHeadStyle->setCurrentIndex(int(this->currentItem.arrowHead));
    this->ui.checkBoxVectorMapToColor->setChecked(this->currentItem.mapVectorToColor);
    this->ui.colorFrameVectorColor->setPlainColor(
        functionsGui::toQColor(this->currentItem.vectorStyle.color));
    this->ui.colorFrameVectorColor->setEnabled(!this->currentItem.mapVectorToColor);
    this->ui.pushButtonEditVectorColor->setEnabled(!this->currentItem.mapVectorToColor);
  }
  else
    this->ui.groupBoxVector->hide();

  this->ui.frameGridColor->setPlainColor(functionsGui::toQColor(this->currentItem.gridStyle.color));
  this->ui.doubleSpinBoxGridLineWidth->setValue(this->currentItem.gridStyle.width);
  this->ui.checkBoxGridScaleToZoom->setChecked(this->currentItem.scaleGridToZoom);

  auto penStyleIndex = indexInVec(stats::AllPatterns, this->currentItem.vectorStyle.pattern);
  if (penStyleIndex != -1)
    this->ui.comboBoxGridLineStyle->setCurrentIndex(penStyleIndex);

  this->resize(sizeHint());
}

stats::StatisticsType StatisticsStyleControl::getStatsItem() const
{
  return this->currentItem;
}

void StatisticsStyleControl::on_groupBoxVector_clicked(bool check)
{
  this->currentItem.renderVectorData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_groupBoxBlockData_clicked(bool check)
{
  this->currentItem.renderValueData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxScaleValueToBlockSize_stateChanged(int val)
{
  this->currentItem.scaleValueToBlockSize = (val != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_blockDataTab_currentChanged(int index)
{
  auto &colorMapper = this->currentItem.colorMapper;
  if (index == 0)
  {
    colorMapper.mappingType = MappingType::Predefined;
    this->ui.comboBoxPredefined->setCurrentIndex(
        int(stats::color::PredefinedTypeMapper.indexOf(colorMapper.predefinedType)));
    this->ui.spinBoxPredefinedRangeMin->setValue(colorMapper.valueRange.min);
    this->ui.spinBoxPredefinedRangeMax->setValue(colorMapper.valueRange.max);
  }
  else if (index == 1)
  {
    colorMapper.mappingType = MappingType::Gradient;
    this->ui.frameGradientStartColor->setPlainColor(
        functionsGui::toQColor(colorMapper.gradientColorStart));
    this->ui.frameGradientEndColor->setPlainColor(
        functionsGui::toQColor(colorMapper.gradientColorEnd));
    this->ui.spinBoxGradientRangeMin->setValue(colorMapper.valueRange.min);
    this->ui.spinBoxGradientRangeMax->setValue(colorMapper.valueRange.max);
  }
  else if (index == 2)
  {
    if (colorMapper.mappingType != MappingType::Map)
    {
      colorMapper.colorMap = convertNonMapTypeToColorMap(colorMapper);
    }
    colorMapper.mappingType = MappingType::Map;
    if (auto customMapEntry = this->customColorMapStorage.indexOfColorMap(
            colorMapper.colorMap, colorMapper.colorMapOther))
      this->ui.comboBoxCustomMap->setCurrentIndex(int(*customMapEntry));
    else
      this->ui.comboBoxCustomMap->setCurrentIndex(-1);
  }
  this->ui.frameDataColor->setColorMapper(colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxPredefined_currentIndexChanged(int index)
{
  if (this->currentItem.colorMapper.mappingType != MappingType::Predefined)
    return;

  if (auto newType = stats::color::PredefinedTypeMapper.at(size_t(index)))
  {
    this->currentItem.colorMapper.predefinedType = *newType;
    this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_spinBoxPredefinedRangeMin_valueChanged(int val)
{
  if (this->currentItem.colorMapper.mappingType != MappingType::Predefined)
    return;

  this->currentItem.colorMapper.valueRange.min = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxPredefinedRangeMax_valueChanged(int val)
{
  if (this->currentItem.colorMapper.mappingType != MappingType::Predefined)
    return;

  this->currentItem.colorMapper.valueRange.max = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameGradientStartColor_clicked()
{
  auto newQColor = QColorDialog::getColor(
      functionsGui::toQColor(this->currentItem.colorMapper.gradientColorStart),
      this,
      tr("Select color range minimum"),
      QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && this->currentItem.colorMapper.gradientColorStart != newColor)
  {
    this->currentItem.colorMapper.gradientColorStart = newColor;
    this->ui.frameGradientStartColor->setPlainColor(newQColor);
    this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
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
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem.colorMapper.gradientColorEnd),
                             this,
                             tr("Select color range maximum"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && this->currentItem.colorMapper.gradientColorEnd != newColor)
  {
    this->currentItem.colorMapper.gradientColorEnd = newColor;
    this->ui.frameGradientEndColor->setPlainColor(newQColor);
    this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonGradientEditEndColor_clicked()
{
  this->on_frameGradientEndColor_clicked();
};

void StatisticsStyleControl::on_spinBoxGradientRangeMin_valueChanged(int val)
{
  if (this->currentItem.colorMapper.mappingType != MappingType::Gradient)
    return;

  this->currentItem.colorMapper.valueRange.min = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxGradientRangeMax_valueChanged(int val)
{
  if (this->currentItem.colorMapper.mappingType != MappingType::Gradient)
    return;

  this->currentItem.colorMapper.valueRange.max = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxCustomMap_currentIndexChanged(int index)
{
  if (this->currentItem.colorMapper.mappingType != MappingType::Map || index < 0)
    return;

  const auto customColormap                   = this->customColorMapStorage.at(size_t(index));
  this->currentItem.colorMapper.colorMap      = customColormap.colorMap;
  this->currentItem.colorMapper.colorMapOther = customColormap.other;
  this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_pushButtonEditMap_clicked()
{
  const auto originalColorMap   = this->currentItem.colorMapper.colorMap;
  const auto originalOtherColor = this->currentItem.colorMapper.colorMapOther;

  StatisticsStyleControl_ColorMapEditor colorMapEditor(originalColorMap, originalOtherColor, this);

  connect(&colorMapEditor, &StatisticsStyleControl_ColorMapEditor::mapChanged, [&]() {
    this->currentItem.colorMapper.colorMap      = colorMapEditor.getColorMap();
    this->currentItem.colorMapper.colorMapOther = colorMapEditor.getOtherColor();
    this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
    emit StyleChanged();
  });

  if (colorMapEditor.exec() == QDialog::Accepted)
  {
    auto somethingChanged = originalColorMap != colorMapEditor.getColorMap() ||
                            originalOtherColor != colorMapEditor.getOtherColor();
    if (somethingChanged)
    {
      this->ui.comboBoxCustomMap->setCurrentIndex(-1);
      this->currentItem.colorMapper.colorMap      = colorMapEditor.getColorMap();
      this->currentItem.colorMapper.colorMapOther = colorMapEditor.getOtherColor();
    }
  }
  else
  {
    this->currentItem.colorMapper.colorMap      = originalColorMap;
    this->currentItem.colorMapper.colorMapOther = originalOtherColor;
    this->ui.frameDataColor->setColorMapper(this->currentItem.colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonSaveMap_clicked()
{
  if (this->currentItem.colorMapper.mappingType != MappingType::Map)
    return;

  bool ok{};
  auto name = QInputDialog::getText(this,
                                    "Save custom map",
                                    "Please enter a name for the custom map.",
                                    QLineEdit::Normal,
                                    "",
                                    &ok);
  if (ok && !name.isEmpty())
  {
    if (this->customColorMapStorage.contains(name))
    {
      auto choice = QMessageBox::question(
          this,
          "Save custom map",
          "A custom map with the given name already exists. Do you want to overwrite it?",
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::Yes);
      if (choice != QMessageBox::Yes)
        return;
    }
    auto newIndex =
        this->customColorMapStorage.saveAndGetIndex({name,
                                                     this->currentItem.colorMapper.colorMap,
                                                     this->currentItem.colorMapper.colorMapOther});
    this->refreshComboBoxCustomMapFromStorage();
    QSignalBlocker blockerPredefined(this->ui.comboBoxCustomMap);
    this->ui.comboBoxCustomMap->setCurrentIndex(int(newIndex));
  }
}

void StatisticsStyleControl::on_pushButtonDeleteMap_clicked()
{
  auto itemName = this->ui.comboBoxCustomMap->currentText();
  if (itemName.isEmpty())
    return;

  this->customColorMapStorage.remove(itemName);
  this->refreshComboBoxCustomMapFromStorage();
  QSignalBlocker blockerPredefined(this->ui.comboBoxCustomMap);
  this->ui.comboBoxCustomMap->setCurrentIndex(-1);
}

void StatisticsStyleControl::on_comboBoxVectorLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  auto pattern                          = stats::AllPatterns.at(index);
  this->currentItem.vectorStyle.pattern = pattern;
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxVectorLineWidth_valueChanged(double width)
{
  this->currentItem.vectorStyle.width = width;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorScaleToZoom_stateChanged(int arg1)
{
  this->currentItem.scaleVectorToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxVectorHeadStyle_currentIndexChanged(int index)
{
  this->currentItem.arrowHead = (stats::StatisticsType::ArrowHead)(index);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorMapToColor_stateChanged(int arg1)
{
  this->currentItem.mapVectorToColor = (arg1 != 0);
  ui.colorFrameVectorColor->setEnabled(!this->currentItem.mapVectorToColor);
  ui.pushButtonEditVectorColor->setEnabled(!this->currentItem.mapVectorToColor);
  emit StyleChanged();
}

void StatisticsStyleControl::on_colorFrameVectorColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem.vectorStyle.color),
                             this,
                             tr("Select vector color"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && newColor != this->currentItem.vectorStyle.color)
  {
    this->currentItem.vectorStyle.color = newColor;
    this->ui.colorFrameVectorColor->setPlainColor(newQColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_groupBoxGrid_clicked(bool check)
{
  this->currentItem.renderGrid = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameGridColor_clicked()
{
  auto newQColor = QColorDialog::getColor(functionsGui::toQColor(this->currentItem.gridStyle.color),
                                          this,
                                          tr("Select grid color"),
                                          QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && newColor != this->currentItem.gridStyle.color)
  {
    this->currentItem.gridStyle.color = newColor;
    this->ui.frameGridColor->setPlainColor(newQColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_comboBoxGridLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  auto pattern                        = stats::AllPatterns.at(index);
  this->currentItem.gridStyle.pattern = pattern;
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxGridLineWidth_valueChanged(double width)
{
  this->currentItem.gridStyle.width = width;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxGridScaleToZoom_stateChanged(int arg1)
{
  this->currentItem.scaleGridToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::refreshComboBoxCustomMapFromStorage()
{
  QSignalBlocker blockerCustomMap(this->ui.comboBoxCustomMap);
  this->ui.comboBoxCustomMap->clear();
  for (const auto &customColorMap : this->customColorMapStorage.getCustomColorMaps())
    this->ui.comboBoxCustomMap->addItem(customColorMap.name);
}
