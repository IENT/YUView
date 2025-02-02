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
#include <statistics/StatisticsType.h>
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
    this->ui.comboBoxPredefined->addItem(QString::fromStdString(std::string(typeName)));
  this->refreshComboBoxCustomMapFromStorage();
}

void StatisticsStyleControl::setStatsItem(stats::StatisticsType *item)
{
  DEBUG_STAT_STYLE("StatisticsStyleControl::setStatsItem %s", item->typeName.toStdString().c_str());
  this->currentItem = item;
  this->setWindowTitle("Edit statistics rendering: " +
                       QString::fromStdString(this->currentItem->typeName));

  if (this->currentItem->valueDataOptions)
  {
    this->ui.groupBoxBlockData->show();
    const auto &colorMapper = this->currentItem->valueDataOptions->colorMapper;

    this->ui.frameDataColor->setColorMapper(colorMapper);

    static const std::map<MappingType, int> MappingTypeToTabIndex(
        {{MappingType::Predefined, 0}, {MappingType::Gradient, 1}, {MappingType::Map, 2}});

    QSignalBlocker blockTabIndexChanged(this->ui.blockDataTab);
    auto           newIndex = MappingTypeToTabIndex.at(colorMapper->mappingType);
    this->ui.blockDataTab->setCurrentIndex(newIndex);
    this->on_blockDataTab_currentChanged(newIndex);
  }
  else
    this->ui.groupBoxBlockData->hide();

  if (this->currentItem->vectorDataOptions)
  {
    this->ui.groupBoxVector->show();

    const auto &options = *this->currentItem->vectorDataOptions;

    if (const auto penStyleIndex = vectorIndexOf(stats::AllPatterns, options.style->pattern))
      this->ui.comboBoxVectorLineStyle->setCurrentIndex(static_cast<int>(*penStyleIndex));
    this->ui.doubleSpinBoxVectorLineWidth->setValue(options.style->width);
    this->ui.checkBoxVectorScaleToZoom->setChecked(options.scaleToZoom);
    this->ui.comboBoxVectorHeadStyle->setCurrentIndex(int(*options.arrowHead));
    this->ui.checkBoxVectorMapToColor->setChecked(options.mapToColor);
    this->ui.colorFrameVectorColor->setPlainColor(functionsGui::toQColor(options.style->color));
    this->ui.colorFrameVectorColor->setEnabled(!options.mapToColor);
    this->ui.pushButtonEditVectorColor->setEnabled(!options.mapToColor);
  }
  else
    this->ui.groupBoxVector->hide();

  {
    const auto &options = this->currentItem->gridOptions;
    this->ui.frameGridColor->setPlainColor(functionsGui::toQColor(options.style->color));
    this->ui.doubleSpinBoxGridLineWidth->setValue(options.style->width);
    this->ui.checkBoxGridScaleToZoom->setChecked(options.scaleToZoom);
  }

  if (const auto penStyleIndex =
          vectorIndexOf(stats::AllPatterns, this->currentItem->vectorDataOptions->style->pattern))
    this->ui.comboBoxGridLineStyle->setCurrentIndex(static_cast<int>(*penStyleIndex));

  this->resize(sizeHint());
}

void StatisticsStyleControl::on_groupBoxVector_clicked(bool check)
{
  if (!this->currentItem || !this->currentItem->vectorDataOptions)
    return;

  this->currentItem->vectorDataOptions->render = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_groupBoxBlockData_clicked(bool check)
{
  if (!this->currentItem || !this->currentItem->valueDataOptions)
    return;

  this->currentItem->valueDataOptions->render = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxScaleValueToBlockSize_stateChanged(int val)
{
  if (!this->currentItem || !this->currentItem->valueDataOptions)
    return;

  this->currentItem->valueDataOptions->scaleToBlockSize = (val != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_blockDataTab_currentChanged(int index)
{
  auto &colorMapper = this->currentItem->valueDataOptions->colorMapper;
  if (index == 0)
  {
    colorMapper->mappingType = MappingType::Predefined;
    this->ui.comboBoxPredefined->setCurrentIndex(
        int(stats::color::PredefinedTypeMapper.indexOf(colorMapper->predefinedType)));
    this->ui.spinBoxPredefinedRangeMin->setValue(colorMapper->valueRange.min);
    this->ui.spinBoxPredefinedRangeMax->setValue(colorMapper->valueRange.max);
  }
  else if (index == 1)
  {
    colorMapper->mappingType = MappingType::Gradient;
    this->ui.frameGradientStartColor->setPlainColor(
        functionsGui::toQColor(colorMapper->gradientColorStart));
    this->ui.frameGradientEndColor->setPlainColor(
        functionsGui::toQColor(colorMapper->gradientColorEnd));
    this->ui.spinBoxGradientRangeMin->setValue(colorMapper->valueRange.min);
    this->ui.spinBoxGradientRangeMax->setValue(colorMapper->valueRange.max);
  }
  else if (index == 2)
  {
    if (colorMapper->mappingType != MappingType::Map)
    {
      colorMapper->colorMap = convertNonMapTypeToColorMap(colorMapper);
    }
    colorMapper->mappingType = MappingType::Map;
    if (auto customMapEntry = this->customColorMapStorage.indexOfColorMap(
            colorMapper->colorMap, colorMapper->colorMapOther))
      this->ui.comboBoxCustomMap->setCurrentIndex(int(*customMapEntry));
    else
      this->ui.comboBoxCustomMap->setCurrentIndex(-1);
  }
  this->ui.frameDataColor->setColorMapper(colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxPredefined_currentIndexChanged(int index)
{
  if (!this->currentItem || !this->currentItem->valueDataOptions ||
      this->currentItem->valueDataOptions->colorMapper->mappingType != MappingType::Predefined ||
      index < 0)
    return;

  if (auto newType = stats::color::PredefinedTypeMapper.getValueAt(static_cast<std::size_t>(index)))
  {
    this->currentItem->valueDataOptions->colorMapper->predefinedType = *newType;
    this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_spinBoxPredefinedRangeMin_valueChanged(int val)
{
  if (!this->currentItem || !this->currentItem->valueDataOptions ||
      this->currentItem->valueDataOptions->colorMapper->mappingType != MappingType::Predefined)
    return;

  this->currentItem->valueDataOptions->colorMapper->valueRange.min = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxPredefinedRangeMax_valueChanged(int val)
{
  if (!this->currentItem || !this->currentItem->valueDataOptions ||
      this->currentItem->valueDataOptions->colorMapper->mappingType != MappingType::Predefined)
    return;

  this->currentItem->valueDataOptions->colorMapper->valueRange.max = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameGradientStartColor_clicked()
{
  auto newQColor = QColorDialog::getColor(
      functionsGui::toQColor(this->currentItem->valueDataOptions->colorMapper->gradientColorStart),
      this,
      tr("Select color range minimum"),
      QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() &&
      this->currentItem->valueDataOptions->colorMapper->gradientColorStart != newColor)
  {
    this->currentItem->valueDataOptions->colorMapper->gradientColorStart = newColor;
    this->ui.frameGradientStartColor->setPlainColor(newQColor);
    this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonGradientEditStartColor_clicked()
{
  this->on_frameGradientStartColor_clicked();
};

void StatisticsStyleControl::on_frameGradientEndColor_clicked()
{
  auto newQColor = QColorDialog::getColor(
      functionsGui::toQColor(this->currentItem->valueDataOptions->colorMapper->gradientColorEnd),
      this,
      tr("Select color range maximum"),
      QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() &&
      this->currentItem->valueDataOptions->colorMapper->gradientColorEnd != newColor)
  {
    this->currentItem->valueDataOptions->colorMapper->gradientColorEnd = newColor;
    this->ui.frameGradientEndColor->setPlainColor(newQColor);
    this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonGradientEditEndColor_clicked()
{
  this->on_frameGradientEndColor_clicked();
};

void StatisticsStyleControl::on_spinBoxGradientRangeMin_valueChanged(int val)
{
  if (!this->currentItem ||
      this->currentItem->valueDataOptions->colorMapper->mappingType != MappingType::Gradient)
    return;

  this->currentItem->valueDataOptions->colorMapper->valueRange.min = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxGradientRangeMax_valueChanged(int val)
{
  if (!this->currentItem ||
      this->currentItem->valueDataOptions->colorMapper->mappingType != MappingType::Gradient)
    return;

  this->currentItem->valueDataOptions->colorMapper->valueRange.max = val;
  this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxCustomMap_currentIndexChanged(int index)
{
  if (!this->currentItem ||
      this->currentItem->valueDataOptions->colorMapper->mappingType != MappingType::Map ||
      index < 0)
    return;

  const auto customColormap = this->customColorMapStorage.at(size_t(index));
  this->currentItem->valueDataOptions->colorMapper->colorMap      = customColormap.colorMap;
  this->currentItem->valueDataOptions->colorMapper->colorMapOther = customColormap.other;
  this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
  emit StyleChanged();
}

void StatisticsStyleControl::on_pushButtonEditMap_clicked()
{
  const auto originalColorMap   = this->currentItem->valueDataOptions->colorMapper->colorMap;
  const auto originalOtherColor = this->currentItem->valueDataOptions->colorMapper->colorMapOther;

  StatisticsStyleControl_ColorMapEditor colorMapEditor(originalColorMap, originalOtherColor, this);

  connect(
      &colorMapEditor,
      &StatisticsStyleControl_ColorMapEditor::mapChanged,
      [&]()
      {
        this->currentItem->valueDataOptions->colorMapper->colorMap = colorMapEditor.getColorMap();
        this->currentItem->valueDataOptions->colorMapper->colorMapOther =
            colorMapEditor.getOtherColor();
        this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
        emit StyleChanged();
      });

  if (colorMapEditor.exec() == QDialog::Accepted)
  {
    auto somethingChanged = originalColorMap != colorMapEditor.getColorMap() ||
                            originalOtherColor != colorMapEditor.getOtherColor();
    if (somethingChanged)
    {
      this->ui.comboBoxCustomMap->setCurrentIndex(-1);
      this->currentItem->valueDataOptions->colorMapper->colorMap = colorMapEditor.getColorMap();
      this->currentItem->valueDataOptions->colorMapper->colorMapOther =
          colorMapEditor.getOtherColor();
    }
  }
  else
  {
    this->currentItem->valueDataOptions->colorMapper->colorMap      = originalColorMap;
    this->currentItem->valueDataOptions->colorMapper->colorMapOther = originalOtherColor;
    this->ui.frameDataColor->setColorMapper(this->currentItem->valueDataOptions->colorMapper);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_pushButtonSaveMap_clicked()
{
  if (!this->currentItem ||
      this->currentItem->valueDataOptions->colorMapper->mappingType != MappingType::Map)
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
    auto newIndex = this->customColorMapStorage.saveAndGetIndex(
        {name,
         this->currentItem->valueDataOptions->colorMapper->colorMap,
         this->currentItem->valueDataOptions->colorMapper->colorMapOther});
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
  auto pattern                                         = stats::AllPatterns.at(index);
  this->currentItem->vectorDataOptions->style->pattern = pattern;
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxVectorLineWidth_valueChanged(double width)
{
  this->currentItem->vectorDataOptions->style->width = width;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorScaleToZoom_stateChanged(int arg1)
{
  this->currentItem->vectorDataOptions->scaleToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxVectorHeadStyle_currentIndexChanged(int index)
{
  this->currentItem->vectorDataOptions->arrowHead = (stats::StatisticsType::ArrowHead)(index);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorMapToColor_stateChanged(int arg1)
{
  this->currentItem->vectorDataOptions->mapToColor = (arg1 != 0);
  ui.colorFrameVectorColor->setEnabled(!this->currentItem->vectorDataOptions->mapToColor);
  ui.pushButtonEditVectorColor->setEnabled(!this->currentItem->vectorDataOptions->mapToColor);
  emit StyleChanged();
}

void StatisticsStyleControl::on_colorFrameVectorColor_clicked()
{
  auto newQColor = QColorDialog::getColor(
      functionsGui::toQColor(this->currentItem->vectorDataOptions->style->color),
      this,
      tr("Select vector color"),
      QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && newColor != this->currentItem->vectorDataOptions->style->color)
  {
    this->currentItem->vectorDataOptions->style->color = newColor;
    this->ui.colorFrameVectorColor->setPlainColor(newQColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_groupBoxGrid_clicked(bool check)
{
  this->currentItem->gridOptions.render = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameGridColor_clicked()
{
  auto newQColor =
      QColorDialog::getColor(functionsGui::toQColor(this->currentItem->gridOptions.style->color),
                             this,
                             tr("Select grid color"),
                             QColorDialog::ShowAlphaChannel);

  auto newColor = functionsGui::toColor(newQColor);
  if (newQColor.isValid() && newColor != this->currentItem->gridOptions.style->color)
  {
    this->currentItem->gridOptions.style->color = newColor;
    this->ui.frameGridColor->setPlainColor(newQColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_comboBoxGridLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  auto pattern                                  = stats::AllPatterns.at(index);
  this->currentItem->gridOptions.style->pattern = pattern;
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxGridLineWidth_valueChanged(double width)
{
  this->currentItem->gridOptions.style->width = width;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxGridScaleToZoom_stateChanged(int arg1)
{
  this->currentItem->gridOptions.scaleToZoom = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::refreshComboBoxCustomMapFromStorage()
{
  QSignalBlocker blockerCustomMap(this->ui.comboBoxCustomMap);
  this->ui.comboBoxCustomMap->clear();
  for (const auto &customColorMap : this->customColorMapStorage.getCustomColorMaps())
    this->ui.comboBoxCustomMap->addItem(customColorMap.name);
}
