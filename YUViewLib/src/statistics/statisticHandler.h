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

#ifndef STATISTICSOURCE_H
#define STATISTICSOURCE_H

#include <QPointer>
#include <QVector>
#include <QMutex>
#include "statisticsExtensions.h"
#include "ui/statisticsstylecontrol.h"
#include "common/saveUi.h"
#include "common/typedef.h"

#include "ui_statisticHandler.h"

typedef QVector<StatisticsType> StatisticsTypeList;

/* The statisticHandler can handle statistics.
*/
class statisticHandler : public QObject
{
  Q_OBJECT

public:
  statisticHandler();

  // Get the statistics values under the cursor position (if they are visible)
  QStringPairList getValuesAt(const QPoint &pos);

  // Get the list of all statistics that this source can provide
  StatisticsTypeList getStatisticsTypeList() const { return statsTypeList; }
  // Set the attributes of the statistics that this source can provide (rendered, drawGrid...)
  bool setStatisticsTypeList(const StatisticsTypeList &typeList);
  
  // Create all the check boxes/sliders and so on. If recreateControlsOnly is set, the UI is assumed to be already
  // initialized. Only all the controls are created.
  QLayout *createStatisticsHandlerControls(bool recreateControlsOnly=false);
  // The statsTypeList might have changed. Update the controls. Maybe a statistics type was removed/added
  void updateStatisticsHandlerControls();

  // For the overlay items, a secondary set of controls can be created which also control drawing of the statistics.
  QWidget *getSecondaryStatisticsHandlerControls(bool recreateControlsOnly=false);
  void deleteSecondaryStatisticsHandlerControls();

  // The statistic with the given frameIdx/typeIdx could not be found in the cache.
  // Load it to the cache. This has to be handled by the child classes.
  //virtual void loadStatisticToCache(int frameIdx, int typeIdx) = 0;

  // Draw the statistics for the given frame index with the given zoomFactor to the painter.
  // Returns false if the statistics need to be loaded first.
  void paintStatistics(QPainter *painter, int frameIdx, double zoomFactor);

  // Draw a vector.
  void paintVector(QPainter *painter, const int &statTypeIdx, const double &zoomFactor,
                   const int &x1, const int &y1, const int &x2, const int &y2,
                   const float &vx, const float &vy, bool isLine, const int &xMin, const int &xMax, const int &yMin, const int &yMax);

  // Do we need to load some of the statistics before we can draw them?
  itemLoadingState needsLoading(int frameIdx);

  // If needsLoading() returned LoadingNeeded, this function is called to load all the needed statistics
  // data that is needed to render the statistics for the given frame.
  void loadStatistics(int frameIdx);

  // Get the statisticsType with the given typeID from p_statsTypeList
  StatisticsType *getStatisticsType(int typeID);

  void setFrameSize(QSize frameSize) { statFrameSize = frameSize; }
  void setFrameSize(int width, int height) { statFrameSize = QSize(width, height); }
  QSize getFrameSize() const { return statFrameSize; }

  // Add new statistics type. Add all types using this function before creating the controls (createStatisticsHandlerControls).
//  void addStatType(const StatisticsType &type) { statsTypeList.append(type); }
  void addStatType(const StatisticsType &type);

  // Clear the statistics type list.
  void clearStatTypes();

  // Load/Save status of statistics from playlist file
  void savePlaylist(YUViewDomElement &root) const;
  void loadPlaylist(const YUViewDomElement &root);

  QHash<int, statisticsData> statsCache; // cache of the statistics for the current POC [statsTypeID]
  int statsCacheFrameIdx;

  // Update the settings. For the statistics this means updating the icons for editing statistic.
  void updateSettings();

signals:
  // Update the item (and maybe redraw it)
  void updateItem(bool redraw);
  // Request to load the statistics for the given frame index/typeIdx into statsCache.
  void requestStatisticsLoading(int frameIdx, int typeIdx);

private:

  // The frame size of the statistics. Needed for drawing the statistics at the right position.
  QSize statFrameSize;

  // Make sure that nothing is read from the stats cache while it is being changed.
  QMutex statsCacheAccessMutex;

  // The list of all statistics that this class can provide (and a backup for updating the list)
  StatisticsTypeList statsTypeList;
  StatisticsTypeList statsTypeListBackup;

  // Primary controls for the statistics
  SafeUi<Ui::statisticHandler> ui;

  // Secondary controls. These can be set up it the item is used in an overlay item so that the properties
  // of the statistics item can be controlled from the properties panel of the overlay item. The primary
  // and secondary controls are linked and always show/control the same thing.
  SafeUi<Ui::statisticHandler> ui2;
  QPointer<QWidget> secondaryControlsWidget;

  StatisticsStyleControl statisticsStyleUI;

  // Pointers to the primary and (if created) secondary controls that we added to the properties panel per item
  QList<QCheckBox*>   itemNameCheckBoxes[2];
  QList<QSlider*>     itemOpacitySliders[2];
  QList<QPushButton*> itemStyleButtons[2];
  QSpacerItem*        spacerItems[2];

private slots:

  // This slot is toggled whenever one of the controls for the statistics is changed
  void onStatisticsControlChanged();
  void onSecondaryStatisticsControlChanged();
  void onStyleButtonClicked(int id);
  void updateStatisticItem() { emit updateItem(true); }
};

#endif
