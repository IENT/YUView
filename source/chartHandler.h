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

#ifndef CHARTHANDLER_H
#define CHARTHANDLER_H

#include <QtCharts>
#include <QVector>
#include "playlistItem.h"
#include "playlistItems.h"

#define CHARTSWIDGET_DEFAULT_WINDOW_TITLE "Charts"

// necesseray, because if we want to use QMap or QHash,
// we have to implement the <() operator(QMap) or the ==() operator
// a small work around, just implement the ==() based on the struct
struct itemWidgetCoord {
  playlistItem* mItem;
  QWidget* mWidget;

  bool operator==(const itemWidgetCoord& aCoord) const
  {
    return (mItem == aCoord.mItem);
  }
};

class ChartHandler : public QObject
{
  Q_OBJECT

public:
  ChartHandler();

  /*
   * creating the Chart depending on the data
   */
  QChartView* createChart(playlistItem* aItem);

  // creates a widget. the content is specified by the playlistitem
  QWidget* createChartWidget(playlistItem* aItem);

  // the title is specified by the playlistitem
  QString getStatisticTitle(playlistItem* aItem);

  // removes a widget from the list and
  void removeWidgetFromList(playlistItem* aItem);

  // TODO -oCH: delete later
  QChartView* makeDummyChart();

public slots:
  void onStatisticsChange(const QString aString);

private:
  // variables
  QVector<itemWidgetCoord> mListItemWidget;

  // functions
  QWidget* createStatisticFileWidget(playlistItemStatisticsFile* aItem);
};

#endif // CHARTHANDLER_H
