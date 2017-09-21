/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2017  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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


#include <QVector>
#include "chartWidget.h"
#include "chartHandlerTypedef.h"
#include "playbackController.h"


class ChartHandler : public QObject
{
  Q_OBJECT

public:
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
  // define names for
  const QString mOpNaCbxChartFrameShow  = "cbxOptionsShow";
  const QString mOpNaCbxChartGroupBy    = "cbxOptionsGroup";
  const QString mOpNaCbxChartNormalize  = "cbxOptionsNormalize";


// variables
  // holds the ChartWidget for showing the charts
  ChartWidget* mChartWidget;
  // an empty default-charview
  QChartView mEmptyChartView;
  // an default widget if no data is avaible
  QWidget mNoDataToShowWidget;
  // an default widget if the chart is not ready yet
  QWidget mDataIsLoadingWidget;
  // save the last created statisticsWidget
  QWidget* mLastStatisticsWidget;
  // save the last ChartOrderBy-Enum
  ChartOrderBy mLastChartOrderBy = cobUnknown;
  // save the last selected statistics-type
  QString mLastStatisticsType = "";

  //list of all created Widgets and items
  QVector<itemWidgetCoord> mListItemWidget;
  // Pointer to the TreeWidget
  QPointer<PlaylistTreeWidget> mPlaylist;
  // Pointer to the PlaybackController
  QPointer<PlaybackController> mPlayback;

// functions
/*----------auxiliary functions----------*/
 /*----------generel functions----------*/
  // try to find an itemWidgetCoord to a specified item
  // if found returns a valid itemWidgetCoord
  // otherwise return a coord with null
  itemWidgetCoord getItemWidgetCoord(playlistItem* aItem);

  // place a widget in the itemWidgetCoord on the chart
  void placeChart(itemWidgetCoord aCoord, QWidget* aChart);

  // function defines the widgets to order the chart-values
  /** IMPORTANT the comboboxes has no connect!!! so connect it so something you want to do with */
  QList<QWidget*> generateOrderWidgetsOnly(bool aAddOptions);

  // generates a layout, widget from "generateOrderWidgetsOnly" will be placed
  QLayout* generateOrderByLayout(bool aAddOptions);

  // the data from the frame will be ordered and categorized by his value
  QList<collectedData>* sortAndCategorizeData(const itemWidgetCoord aCoord, const QString aType, const int aFrameIndex);
  // the data from the frame will be ordered and categorized by his value
  QList<collectedData>* sortAndCategorizeDataAllFrames(const itemWidgetCoord aCoord, const QString aType);

  // creates the chart based on the sorted Data from sortAndCategorizeData() or sortAndCategorizeDataAllFrames()
  QWidget* makeStatistic(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy);

  // provides the ChartOrderBy: cobPerFrameGrpByValueNrmNone
  chartSettingsData makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData);
  // provides the ChartOrderBy: cobPerFrameGrpByValueNrmByArea
  chartSettingsData makeStatisticsPerFrameGrpByValNrmArea(QList<collectedData>* aSortedData);
  // provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmNone
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);
  // provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmByArea
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData);
  // provides the ChartOrderBy: cobAllFramesGrpByValueNrmNone
  chartSettingsData makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData);
  // provides the ChartOrderBy: cobAllFramesGrpByValueNrmByArea
  chartSettingsData makeStatisticsAllFramesGrpByValNrmArea(QList<collectedData>* aSortedData);
  // provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmNone
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);
  // provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmByArea
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData);

/*----------playListItemStatisticsFile----------*/
  // creates Widget based on an "playListItemStatisticsFile"
  QWidget* createStatisticFileWidget(playlistItemStatisticsFile* aItem, itemWidgetCoord& aCoord);

  // creating the Chart depending on the data
  QWidget* createStatisticsChart(itemWidgetCoord& aCoord);


};


/**
 * NEXT FEATURES / KNOWN BUGS
 *
 * () -  no chart should be selectable, if file is not completly loaded (thread)
 *
 * (done) -  order-by-dropdown change to 3 dropdwon (show: per frame / all frames; group by: value / blocksize; normalize : none / by area (dimension of frame)
 * (done) --   Show: per frame / all frames
 * (done) --   Group by: value / blocksize
 * (done) --   Normalize: none / by area (values dimension compare to complete dimension of frame)
 *
 * () - normalize: dimension of frame, get real dimension of frame and don't calculate it
 *
 * (done) -  change enum ChartOrderBy
 * (done) --   change enum functions
 * (done) --   maybe place enum in typedef.h or create new File ChartHandlerDefinition.h
 *
 * () -  widget mNoDatatoShowWiget and mDataIsLoadingWidget make better look and feel (LAF)
 *
 * (done) -  implement new widget for order-group-settings
 *
 * (done) -  implement all possible settings
 *
 * () -  implement for value is vector-type
 *
 * () -  implement same function for the other playlistitems
 *
 * () -  test on windows / mac
 *
 * (done) - implement mechanism, that the chart doesn't load every time new, if frameindex change
 *
 * () - implement settings widget to set chart-type
 *
 * () - implement different chart-types (bar, pie, ...)
 *
 * () - implement calculating and creating the chart in an seperate thread, not in main-thread
 *
 * () - save last selected frameindex in charthandler? so we can change the frameindex after the selected file has changed
 *
 */
#endif // CHARTHANDLER_H
