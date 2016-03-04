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
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include "typedef.h"
#include "statisticsExtensions.h"

#include "ui_statisticSource.h"

typedef QList<StatisticsItem> StatisticsItemList;
typedef QVector<StatisticsType> StatisticsTypeList;

/** Virtual class.
* The Statistics source can be anything that provides statistics data. Every statistics source should provide
*  functions for getting 
*/

class SliderDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  SliderDelegate(QObject *parent = 0);

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE;
  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) Q_DECL_OVERRIDE;
};

class statisticSource : private Ui_statisticSource
{
public:
  statisticSource();
  virtual ~statisticSource();

  // Draw the statistics that are selected to be rendered of the given frameIdx into the given pixmap
  void drawStatistics(QPixmap *img, int frameIdx);
  
  ValuePairList getValuesAt(int x, int y);
      
  // Get the list of all statistics that this source can provide
  StatisticsTypeList getStatisticsTypeList() { return statsTypeList; }
  // Set the attributes of the statistics that this source can provide (rendered, drawGrid...)
  bool setStatisticsTypeList(StatisticsTypeList typeList);
  // Return true if any of the statistics are actually rendered
  bool anyStatisticsRendered();

  void addPropertiesWidget(QWidget *widget);

protected:
 
  // Get the statistics with the given frameIdx/typeIdx.
  // Check cache first, if not load by calling loadStatisticToCache.
  StatisticsItemList getStatistics(int frameIdx, int typeIdx);

  // The statistic with the given frameIdx/typeIdx could not be found in the cache.
  // Load it to the cache. This has to be handeled by the child classes.
  virtual void loadStatisticToCache(int frameIdx, int typeIdx) = 0;

  // Draw the given list of statistics to the 
  void drawStatisticsImage(QPixmap *img, StatisticsItemList statsList, StatisticsType statsType);

  // Get the statisticsType with the given typeID from p_statsTypeList 
  StatisticsType* getStatisticsType(int typeID);

  int lastFrameIdx;
  
  // The list of all statistics that this class can provide
  StatisticsTypeList statsTypeList;

  QHash< int, QHash< int, StatisticsItemList > > statsCache; // 2D map of type StatisticsItemList with indexing: [POC][statsTypeID]

private:

  SliderDelegate delegate;

  QStandardItemModel model;

};

#endif
