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
#include "chartWidget.h"
#include "playbackController.h"
#include "playlistItem.h"
#include "playlistItems.h"
#include "playlistTreeWidget.h"

#define CHARTSWIDGET_DEFAULT_WINDOW_TITLE "Charts"


// small struct to avoid big return-types
struct collectedData {
  // the label
  QString mLabel = "";
  // each int* should be an array with two ints
  // first: value
  // second: count, how often the value was found in the frame
  QList<int*> mValueList;
};

// necesseray, because if we want to use QMap or QHash,
// we have to implement the <() operator(QMap) or the ==() operator(QHash)
// a small work around, just implement the ==() based on the struct
struct itemWidgetCoord {
  playlistItem* mItem;
  QWidget*      mWidget;
  QStackedWidget*      mChart;
  QMap<QString, QList<QList<QVariant>>>* mData;

  itemWidgetCoord()
    : mItem(NULL), mWidget(NULL),mChart(new QStackedWidget), mData(NULL) // initialise member
  {}

  // check that the Pointer on the items are equal
  bool operator==(const itemWidgetCoord& aCoord) const
  {
    return (mItem == aCoord.mItem);
  }

  // check that the Pointer on the items are equal
  bool operator==(const playlistItem* aItem) const
  {
    return (mItem == aItem);
  }
};

class ChartHandler : public QObject
{
  Q_OBJECT
  Q_ENUMS(ChartOrderBy)

public:
  // small enum, to define the different order-types
  /** if change the enum, change the enum-methods to it too*/
  enum ChartOrderBy {
    cobFrame,                   // order: frame
    cobValue,                   // order: value
    cobBlocksize,               // order: blocksize
    cobAbsoluteFrames,          // order: absolute frames with each Values
    cobAbsoluteValues,          // order: absolute all values for each frame
    cobAbsoluteValuesAndFrames, // order: absolute all values and all frames
    cobUnknown                  // order type is unknown, no sort available
  };

  // default-constructor
  ChartHandler();

  // creates a widget. the content is specified by the playlistitem
  QWidget* createChartWidget(playlistItem* aItem);

  // the title is specified by the playlistitem
  QString getStatisticTitle(playlistItem* aItem);

  // removes a widget from the list and
  void removeWidgetFromList(playlistItem* aItem);

  // setting the ChartWidget, so we can show the charts later
  void setChartWidget(ChartWidget* aChartWidget) {this->mChartWidget = aChartWidget;}

  // setting the PlaylistTreeWidget, because we want to know which items are selected, actual support only for one! item
  void setPlaylistTreeWidget( PlaylistTreeWidget *aPlayLTW ) { this->mPlaylist = aPlayLTW; }

  // setting the PlaybackController, maybe we can use it for define the Framerange
  void setPlaybackController( PlaybackController *aPBC ) { this->mPlayback = aPBC; }

public slots:
  // slot, after the playlist item selection have changed
  void currentSelectedItemsChanged(playlistItem *aItem1, playlistItem *aItem2);

  // slot, item will be deleted from the playlist
  void itemAboutToBeDeleted(playlistItem *aItem);

  // slot, after the playbackcontroller was moved, so the selected frame changed
  void playbackControllerFrameChanged(int aNewFrameIndex);

private slots:
/*----------playListItemStatisticsFile----------*/
  // if the selected statistic-type changed, the chart has to be updated
  void onStatisticsChange(const QString aString);
  // if the selected statistic-type changed, we have to decide if the order/sort-combobox is enabled
  void switchOrderEnableStatistics(const QString aString);

private:
// variables
  // holds the ChartWidget for showing the charts
  ChartWidget* mChartWidget;
  // an empty default-charview
  QChartView mEmptyChartView;
  // an default widget if no data is avaible
  QWidget mNoDatatoShowWiget;
  // an default widget if the chart is not ready yet
  QWidget mDataIsLoadingWidget;

  //list of all created Widgets and items
  QVector<itemWidgetCoord> mListItemWidget;
  // Pointer to the TreeWidget
  QPointer<PlaylistTreeWidget> mPlaylist;
  // Pointer to the PlaybackController
  QPointer<PlaybackController> mPlayback;

// functions
/*----------auxiliary functions----------*/
 /*----------Enum ChartOrderBy----------*/
  // converts the given enum to an readable string
  QString chartOrderByEnumAsString(ChartOrderBy aEnum);

  // converts the given enum to an readable tooltip
  QString chartOrderByEnumAsTooltip(ChartOrderBy aEnum);

 /*----------generel functions----------*/
  // try to find an itemWidgetCoord to a specified item
  // if found returns a valid itemWidgetCoord
  // otherwise return a coord with null
  itemWidgetCoord getItemWidgetCoord(playlistItem* aItem);

  // place a widget in the itemWidgetCoord on the chart
  void placeChart(itemWidgetCoord aCoord, QWidget* aChart);

  // function defines the widgets to order the chart-values
  /** IMPORTANT the combobox has no connect!!! so connect it so something you want to do with */
  QList<QWidget*> generateOrderWidgetsOnly(bool aAddOptions);

  // generates a layout, widget from "generateOrderWidgetsOnly" will be placed
  QLayout* generateOrderByLayout(bool aAddOptions);

  // the data from the frame will be ordered and categorized by his value
  QList<collectedData>* sortAndCategorizeData(const itemWidgetCoord aCoord, const QString aType, const int aFrameIndex);

  // creates the chart based on the sorted Data from sortAndCategorizeData()
  QWidget* makeStatistic(QList<collectedData>* aSortedData, const QString aOrderBy = "frame");

/*----------playListItemStatisticsFile----------*/
  // creates Widget based on an "playListItemStatisticsFile"
  QWidget* createStatisticFileWidget(playlistItemStatisticsFile* aItem, itemWidgetCoord& aCoord);

  // creating the Chart depending on the data
  QWidget* createStatisticsChart(itemWidgetCoord& aCoord);
};

#endif // CHARTHANDLER_H
