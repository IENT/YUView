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

#pragma once

#include "common/saveUi.h"
#include "common/typedef.h"
#include "common/YUViewDomElement.h"
#include "statistics/StatisticsType.h"
#include "ui/statisticsstylecontrol.h"

#include <QMutex>
#include <QPointer>
#include <QVector>

#include "ui_statisticUIHandler.h"

namespace stats
{

class StatisticsData;

/* Handles the UI for statistics data
 */
class StatisticUIHandler : public QObject
{
  Q_OBJECT

public:
  StatisticUIHandler();

  void setStatisticsData(StatisticsData *statisticsData);

  // Create all the check boxes/sliders and so on. If recreateControlsOnly is set, the UI is assumed
  // to be already initialized. Only all the controls are created.
  QLayout *createStatisticsHandlerControls(bool recreateControlsOnly = false);
  // The statsTypeList might have changed. Update the controls. Maybe a statistics type was
  // removed/added
  void updateStatisticsHandlerControls();

  // For the overlay items, a secondary set of controls can be created which also control drawing of
  // the statistics.
  QWidget *getSecondaryStatisticsHandlerControls(bool recreateControlsOnly = false);
  void     deleteSecondaryStatisticsHandlerControls();

  // The statistic with the given frameIdx/typeIdx could not be found in the cache.
  // Load it to the cache. This has to be handled by the child classes.
  // virtual void loadStatisticToCache(int frameIdx, int typeIdx) = 0;

  // Clear the statistics type list.
  void clearStatTypes();

  // Update the settings. For the statistics this means updating the icons for editing statistic.
  void updateSettings();

signals:
  // Update the item (and maybe redraw it)
  void updateItem(bool redraw);

private:
  StatisticsData *            statisticsData{};
  std::vector<StatisticsType> statsTypeListBackup;

  // Primary controls for the statistics
  SafeUi<Ui::StatisticUIHandler> ui;

  // Secondary controls. These can be set up it the item is used in an overlay item so that the
  // properties of the statistics item can be controlled from the properties panel of the overlay
  // item. The primary and secondary controls are linked and always show/control the same thing.
  SafeUi<Ui::StatisticUIHandler> ui2;
  QPointer<QWidget>            secondaryControlsWidget;

  StatisticsStyleControl statisticsStyleUI;

  // Pointers to the primary and (if created) secondary controls that we added to the properties
  // panel per item
  QList<QCheckBox *>   itemNameCheckBoxes[2];
  QList<QSlider *>     itemOpacitySliders[2];
  QList<QPushButton *> itemStyleButtons[2];
  QSpacerItem *        spacerItems[2] {nullptr};

private slots:

  // This slot is toggled whenever one of the controls for the statistics is changed
  void onStatisticsControlChanged();
  void onSecondaryStatisticsControlChanged();
  void onStyleButtonClicked(unsigned id);
  void updateStatisticItem() { emit updateItem(true); }
};

} // namespace stats
