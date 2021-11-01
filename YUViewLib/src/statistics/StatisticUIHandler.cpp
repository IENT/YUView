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

#include "StatisticUIHandler.h"

#include <QPainter>
#include <QtGlobal>
#include <cmath>
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#include <QPainterPath>
#endif
#include <QtMath>

#include <statistics/StatisticsData.h>
#include <statistics/StatisticsType.h>
#include <common/FunctionsGui.h>

namespace stats
{

// Activate this if you want to know when what is loaded.
#define STATISTICS_DEBUG_LOADING 0
#if STATISTICS_DEBUG_LOADING && !NDEBUG
#define DEBUG_STATUI qDebug
#else
#define DEBUG_STATUI(fmt, ...) ((void)0)
#endif

StatisticUIHandler::StatisticUIHandler()
{
  connect(&statisticsStyleUI,
          &StatisticsStyleControl::StyleChanged,
          this,
          &StatisticUIHandler::updateStatisticItem,
          Qt::QueuedConnection);
}

void StatisticUIHandler::setStatisticsData(StatisticsData *data) { this->statisticsData = data; }

QLayout *StatisticUIHandler::createStatisticsHandlerControls(bool recreateControlsOnly)
{
  if (!recreateControlsOnly)
  {
    // Absolutely always only do this once
    Q_ASSERT_X(
        !ui.created(), Q_FUNC_INFO, "The primary statistics controls must only be created once.");
    ui.setupUi();
  }

  if (!this->statisticsData)
  {
    DEBUG_STATUI("StatisticUIHandler::createStatisticsHandlerControls statisticsData not set");
    return {};
  }

  // Add the controls to the gridLayer
  auto &statTypes = this->statisticsData->getStatisticsTypes();
  for (unsigned row = 0; row < statTypes.size(); ++row)
  {
    auto &statType = statTypes.at(row);

    // Append the name (with the check box to enable/disable the statistics item)
    QCheckBox *itemNameCheck = new QCheckBox(statType.typeName, ui.scrollAreaWidgetContents);
    itemNameCheck->setChecked(statType.render);
    itemNameCheck->setToolTip(statType.description);
    ui.gridLayout->addWidget(itemNameCheck, int(row + 2), 0);
    connect(itemNameCheck,
            &QCheckBox::stateChanged,
            this,
            &StatisticUIHandler::onStatisticsControlChanged);
    itemNameCheckBoxes[0].push_back(itemNameCheck);

    // Append the opacity slider
    QSlider *opacitySlider = new QSlider(Qt::Horizontal);
    opacitySlider->setMinimum(0);
    opacitySlider->setMaximum(100);
    opacitySlider->setValue(statType.alphaFactor);
    ui.gridLayout->addWidget(opacitySlider, int(row + 2), 1);
    connect(opacitySlider,
            &QSlider::valueChanged,
            this,
            &StatisticUIHandler::onStatisticsControlChanged);
    itemOpacitySliders[0].push_back(opacitySlider);

    // Append the change style buttons
    QPushButton *pushButton = new QPushButton(
        functionsGui::convertIcon(":img_edit.png"), QString(), ui.scrollAreaWidgetContents);
    ui.gridLayout->addWidget(pushButton, int(row + 2), 2);
    connect(pushButton, &QPushButton::released, this, [=] { onStyleButtonClicked(row); });
    itemStyleButtons[0].push_back(pushButton);
  }

  // Add a spacer at the very bottom
  QSpacerItem *verticalSpacer = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
  ui.gridLayout->addItem(verticalSpacer, int(statTypes.size() + 2), 0, 1, 1);
  spacerItems[0] = verticalSpacer;

  // Update all controls
  onStatisticsControlChanged();

  return ui.verticalLayout;
}

QWidget *StatisticUIHandler::getSecondaryStatisticsHandlerControls(bool recreateControlsOnly)
{
  if (!ui2.created() || recreateControlsOnly)
  {
    if (!ui2.created())
    {
      secondaryControlsWidget = new QWidget;
      ui2.setupUi();
      secondaryControlsWidget->setLayout(ui2.verticalLayout);
    }

    if (!this->statisticsData)
    {
      DEBUG_STATUI(
          "StatisticUIHandler::getSecondaryStatisticsHandlerControls statisticsData not set");
      return {};
    }

    // Add the controls to the gridLayer
    auto &statTypes = this->statisticsData->getStatisticsTypes();
    for (unsigned row = 0; row < statTypes.size(); ++row)
    {
      auto &statType = statTypes.at(row);

      // Append the name (with the check box to enable/disable the statistics item)
      QCheckBox *itemNameCheck = new QCheckBox(statType.typeName, ui2.scrollAreaWidgetContents);
      itemNameCheck->setChecked(statType.render);
      ui2.gridLayout->addWidget(itemNameCheck, int(row + 2), 0);
      connect(itemNameCheck,
              &QCheckBox::stateChanged,
              this,
              &StatisticUIHandler::onSecondaryStatisticsControlChanged);
      itemNameCheckBoxes[1].push_back(itemNameCheck);

      // Append the opacity slider
      QSlider *opacitySlider = new QSlider(Qt::Horizontal);
      opacitySlider->setMinimum(0);
      opacitySlider->setMaximum(100);
      opacitySlider->setValue(statType.alphaFactor);
      ui2.gridLayout->addWidget(opacitySlider, int(row + 2), 1);
      connect(opacitySlider,
              &QSlider::valueChanged,
              this,
              &StatisticUIHandler::onSecondaryStatisticsControlChanged);
      itemOpacitySliders[1].push_back(opacitySlider);

      // Append the change style buttons
      QPushButton *pushButton = new QPushButton(
          functionsGui::convertIcon(":img_edit.png"), QString(), ui2.scrollAreaWidgetContents);
      ui2.gridLayout->addWidget(pushButton, int(row + 2), 2);
      connect(pushButton, &QPushButton::released, this, [=] { onStyleButtonClicked(row); });
      itemStyleButtons[1].push_back(pushButton);
    }

    // Add a spacer at the very bottom
    // TODO FIXME Should we always add the spacer or only when
    // the controls were created?
    if (true || ui2.created())
    {
      QSpacerItem *verticalSpacer =
          new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
      ui2.gridLayout->addItem(verticalSpacer, int(statTypes.size() + 2), 0, 1, 1);
      spacerItems[1] = verticalSpacer;
    }

    // Update all controls
    onSecondaryStatisticsControlChanged();
  }

  return secondaryControlsWidget;
}

// One of the primary controls changed. Update the secondary controls (if they were created) without
// emitting further signals and of course update the statsTypeList to render the stats correctly.
void StatisticUIHandler::onStatisticsControlChanged()
{
  if (!this->statisticsData)
  {
    DEBUG_STATUI("StatisticUIHandler::onStatisticsControlChanged statisticsData not set");
    return;
  }

  auto &statTypes = this->statisticsData->getStatisticsTypes();
  for (unsigned row = 0; row < statTypes.size(); ++row)
  {
    auto &statType = statTypes.at(row);

    // Get the values of the statistics type from the controls
    statType.render      = itemNameCheckBoxes[0][row]->isChecked();
    statType.alphaFactor = itemOpacitySliders[0][row]->value();

    // Enable/disable the slider and grid check box depending on the item name check box
    bool enable = itemNameCheckBoxes[0][row]->isChecked();
    itemOpacitySliders[0][row]->setEnabled(enable);
    itemStyleButtons[0][row]->setEnabled(enable);

    // Update the secondary controls if they were created
    if (ui2.created() && itemNameCheckBoxes[1].size() > 0)
    {
      // Update the controls that changed
      if (itemNameCheckBoxes[0][row]->isChecked() != itemNameCheckBoxes[1][row]->isChecked())
      {
        const QSignalBlocker blocker(itemNameCheckBoxes[1][row]);
        itemNameCheckBoxes[1][row]->setChecked(itemNameCheckBoxes[0][row]->isChecked());
      }

      if (itemOpacitySliders[0][row]->value() != itemOpacitySliders[1][row]->value())
      {
        const QSignalBlocker blocker(itemOpacitySliders[1][row]);
        itemOpacitySliders[1][row]->setValue(itemOpacitySliders[0][row]->value());
      }
    }
  }

  emit updateItem(true);
}

// One of the secondary controls changed. Perform the inverse thing to onStatisticsControlChanged().
// Update the primary controls without emitting further signals and of course update the
// statsTypeList to render the stats correctly.
void StatisticUIHandler::onSecondaryStatisticsControlChanged()
{
  if (!this->statisticsData)
  {
    DEBUG_STATUI("StatisticUIHandler::onSecondaryStatisticsControlChanged statisticsData not set");
    return;
  }

  auto &statTypes = this->statisticsData->getStatisticsTypes();
  for (unsigned row = 0; row < statTypes.size(); ++row)
  {
    auto &statType = statTypes.at(row);

    // Get the values of the statistics type from the controls
    statType.render      = itemNameCheckBoxes[1][row]->isChecked();
    statType.alphaFactor = itemOpacitySliders[1][row]->value();

    // Enable/disable the slider and grid check box depending on the item name check box
    bool enable = itemNameCheckBoxes[1][row]->isChecked();
    itemOpacitySliders[1][row]->setEnabled(enable);
    itemStyleButtons[1][row]->setEnabled(enable);

    if (itemNameCheckBoxes[0].size() > row)
    {
      // Update the primary controls that changed
      if (itemNameCheckBoxes[0][row]->isChecked() != itemNameCheckBoxes[1][row]->isChecked())
      {
        const QSignalBlocker blocker(itemNameCheckBoxes[0][row]);
        itemNameCheckBoxes[0][row]->setChecked(itemNameCheckBoxes[1][row]->isChecked());
      }

      if (itemOpacitySliders[0][row]->value() != itemOpacitySliders[1][row]->value())
      {
        const QSignalBlocker blocker(itemOpacitySliders[0][row]);
        itemOpacitySliders[0][row]->setValue(itemOpacitySliders[1][row]->value());
      }
    }
  }

  emit updateItem(true);
}

void StatisticUIHandler::deleteSecondaryStatisticsHandlerControls()
{
  secondaryControlsWidget->deleteLater();
  ui2.clear();
  itemNameCheckBoxes[1].clear();
  itemOpacitySliders[1].clear();
  itemStyleButtons[1].clear();
}

void StatisticUIHandler::updateSettings()
{
  for (unsigned row = 0; row < itemStyleButtons[0].size(); ++row)
  {
    itemStyleButtons[0][row]->setIcon(functionsGui::convertIcon(":img_edit.png"));
    if (secondaryControlsWidget)
      itemStyleButtons[1][row]->setIcon(functionsGui::convertIcon(":img_edit.png"));
  }
}

void StatisticUIHandler::updateStatisticsHandlerControls()
{
  if (!this->statisticsData)
  {
    DEBUG_STATUI("StatisticUIHandler::updateStatisticsHandlerControls statisticsData not set");
    return;
  }

  // First run a check if all statisticsTypes are identical
  bool controlsStillValid = true;
  auto &statTypes = this->statisticsData->getStatisticsTypes();
  if (statTypes.size() != itemNameCheckBoxes[0].size())
    // There are more or less statistics types as before
    controlsStillValid = false;
  else
  {
    for (unsigned row = 0; row < statTypes.size(); row++)
    {
      if (itemNameCheckBoxes[0][row]->text() != statTypes[row].typeName)
      {
        // One of the statistics types changed it's name or the order of statistics types changed.
        // Either way, we will create new controls.
        controlsStillValid = false;
        break;
      }
    }
  }

  if (controlsStillValid)
  {
    // Update the controls from the current settings in statsTypeList
    onStatisticsControlChanged();
    if (ui2.created())
      onSecondaryStatisticsControlChanged();
  }
  else
  {
    // Delete all old controls
    for (unsigned i = 0; i < itemNameCheckBoxes[0].size(); i++)
    {
      Q_ASSERT(itemNameCheckBoxes[0].size() == itemOpacitySliders[0].size());
      Q_ASSERT(itemStyleButtons[0].size() == itemOpacitySliders[0].size());

      // Delete the primary controls
      delete itemNameCheckBoxes[0][i];
      delete itemOpacitySliders[0][i];
      delete itemStyleButtons[0][i];

      if (ui2.created())
      {
        Q_ASSERT(itemNameCheckBoxes[1].size() == itemOpacitySliders[1].size());
        Q_ASSERT(itemStyleButtons[1].size() == itemOpacitySliders[1].size());

        // Delete the secondary controls
        delete itemNameCheckBoxes[1][i];
        delete itemOpacitySliders[1][i];
        delete itemStyleButtons[1][i];
      }
    }

    // Delete the spacer items at the bottom.
    assert(spacerItems[0] != nullptr);
    ui.gridLayout->removeItem(spacerItems[0]);
    delete spacerItems[0];
    spacerItems[0] = nullptr;

    // Delete all pointers to the widgets. The layout has the ownership and removing the
    // widget should delete it.
    itemNameCheckBoxes[0].clear();
    itemOpacitySliders[0].clear();
    itemStyleButtons[0].clear();

    if (ui2.created())
    {
      // Delete all pointers to the widgets. The layout has the ownership and removing the
      // widget should delete it.
      itemNameCheckBoxes[1].clear();
      itemOpacitySliders[1].clear();
      itemStyleButtons[1].clear();

      // Delete the spacer items at the bottom.
      assert(spacerItems[1] != nullptr);
      ui2.gridLayout->removeItem(spacerItems[1]);
      delete spacerItems[1];
      spacerItems[1] = nullptr;
    }

    // We have a backup of the old statistics types. Maybe some of the old types (with the same
    // name) are still in the new list. If so, we can update the status of those statistics types
    // (are they drawn, transparency ...).
    for (unsigned i = 0; i < statsTypeListBackup.size(); i++)
    {
      for (unsigned j = 0; j < statTypes.size(); j++)
      {
        if (statsTypeListBackup[i].typeName == statTypes[j].typeName)
        {
          // In the new list of statistics types we found one that has the same name as this one.
          // This is enough indication. Apply the old settings to this new type.
          statTypes[j].render           = statsTypeListBackup[i].render;
          statTypes[j].renderValueData  = statsTypeListBackup[i].renderValueData;
          statTypes[j].renderVectorData = statsTypeListBackup[i].renderVectorData;
          statTypes[j].renderGrid       = statsTypeListBackup[i].renderGrid;
          statTypes[j].alphaFactor      = statsTypeListBackup[i].alphaFactor;
        }
      }
    }

    // Create new controls
    createStatisticsHandlerControls(true);
    if (ui2.created())
      getSecondaryStatisticsHandlerControls(true);
  }
}

void StatisticUIHandler::clearStatTypes()
{
  if (!this->statisticsData)
  {
    DEBUG_STATUI("StatisticUIHandler::clearStatTypes statisticsData not set");
    return;
  }

  // Create a backup of the types list. This backup is used if updateStatisticsHandlerControls is
  // called to revert the new controls. This way we can see which statistics were drawn / how.
  this->statsTypeListBackup = this->statisticsData->getStatisticsTypes();

  // Clear the old list. New items can be added now.
  this->statisticsData->clear();
}

void StatisticUIHandler::onStyleButtonClicked(unsigned id)
{
  if (!this->statisticsData)
  {
    DEBUG_STATUI("StatisticUIHandler::onStyleButtonClicked statisticsData not set");
    return;
  }

  auto &statTypes = this->statisticsData->getStatisticsTypes();
  statisticsStyleUI.setStatsItem(&statTypes[id]);
  statisticsStyleUI.show();
}

} // namespace stats
