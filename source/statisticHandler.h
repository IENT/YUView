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

#ifndef STATISTICSOURCE_H
#define STATISTICSOURCE_H

#include <QPixmap>
#include <QList>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalMapper>
#include <QSettings>
#include "typedef.h"
#include "statisticsExtensions.h"
#include "statisticsstylecontrol.h"
#include "ui_statisticHandler.h"

typedef QList<StatisticsItem> StatisticsItemList;
typedef QVector<StatisticsType> StatisticsTypeList;

#define STATISTICS_DRAW_VALUES_ZOOM 16

/** Virtual class.
* The Statistics source can be anything that provides statistics data. Every statistics source should provide
*  functions for getting
*/

class statisticHandler : public QObject
{
  Q_OBJECT

public:
  statisticHandler();
  virtual ~statisticHandler();

  // Get the statistics values under the curso pos (if they are visible)
  ValuePairList getValuesAt(QPoint pos);

  // Get the list of all statistics that this source can provide
  StatisticsTypeList getStatisticsTypeList() { return statsTypeList; }
  // Set the attributes of the statistics that this source can provide (rendered, drawGrid...)
  bool setStatisticsTypeList(StatisticsTypeList typeList);
  // Return true if any of the statistics are actually rendered
  bool anyStatisticsRendered();

  // Create all the checkboxes/spliders and so on. If recreateControlsOnly is set, the ui is assumed to be already
  // initialized. Only all the controls are created.
  QLayout *createStatisticsHandlerControls(QWidget *parentWidget, bool recreateControlsOnly=false);
  // The statsTypeList might have changed. Update the controls. Maybe a statistics type was removed/added
  void updateStatisticsHandlerControls();
  
  // For the overlay items, a secondary set of controls can be created which also control drawing of the statistics.
  QWidget *getSecondaryStatisticsHandlerControls(bool recreateControlsOnly=false);
  void deleteSecondaryStatisticsHandlerControls();

  // The statistic with the given frameIdx/typeIdx could not be found in the cache.
  // Load it to the cache. This has to be handeled by the child classes.
  //virtual void loadStatisticToCache(int frameIdx, int typeIdx) = 0;

  // Draw the given list of statistics to the painter
  void paintStatistics(QPainter *painter, int frameIdx, double zoomFactor);

  // Get the statisticsType with the given typeID from p_statsTypeList
  StatisticsType* getStatisticsType(int typeID);

  int lastFrameIdx;
  QSize statFrameSize;

  // Add new statistics type. Add all types using this function before creating the controls (createStatisticsHandlerControls).
  void addStatType(StatisticsType type) { statsTypeList.append(type); }
  // Clear the statistics type list.
  void clearStatTypes();

  // Load/Save status of statistics from playlist file
  void savePlaylist(QDomElementYUView &root);
  void loadPlaylist(QDomElementYUView &root);

  QHash<int, StatisticsItemList> statsCache; // cache of the statistics for the current POC [statsTypeID]
  int statsCacheFrameIdx;

signals:
  // Update the item (and maybe redraw it)
  void updateItem(bool redraw);
  // Request to load the statistics for the given frame index/typeIdx into statsCache.
  void requestStatisticsLoading(int frameIdx, int typeIdx);

private:

  // The list of all statistics that this class can provide (and a backup for updating the list)
  StatisticsTypeList statsTypeList;
  StatisticsTypeList statsTypeListBackup;

  // Primary controls for the statistics
  Ui::statisticHandler *ui;

  // Secondary controls. These can be set up it the item is used in an overlay item so that the properties
  // of the statistics item can be controlled from the properties panel of the overlay item. The primary
  // and secondary controls are linked and always show/control the same thing.
  Ui::statisticHandler *ui2;
  QWidget *secondaryControlsWidget;

  StatisticsStyleControl statisticsStyleUI;

  // Pointers to the primary and (if created) secondary controls that we added to the properties panel per item
  QList<QCheckBox*> itemNameCheckBoxes[2];
  QList<QSlider*>   itemOpacitySliders[2];
  QList<QCheckBox*> itemGridCheckBoxes[2];
  QList<QCheckBox*> itemArrowCheckboxes[2];
  QSpacerItem*        spacerItems[2];
  QList<QPushButton*> itemPushButtons[2];
  QSignalMapper *signalMapper[2];
  // Some global settings
  bool mapAllVectorsToColor;

private slots:

  // This slot is toggeled whenever one of the controls for the statistics is changed
  void onStatisticsControlChanged();
  void onSecondaryStatisticsControlChanged();
  void onStyleButtonClicked(int id);
  void updateStatisticItem();
};

#endif
