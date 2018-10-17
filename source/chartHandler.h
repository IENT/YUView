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
#include "yuvcharts.h"

#define CHECKBOX_DRAW_CHART             "cbDrawChart"
#define OPTION_NAME_CBX_CHART_TYPES     "cbxTypes"
#define OPTION_NAME_CBX_CHART_FRAMESHOW "cbxOptionsShow"
#define OPTION_NAME_CBX_CHART_GROUPBY   "cbxOptionsGroup"
#define OPTION_NAME_CBX_CHART_NORMALIZE "cbxOptionsNormalize"
#define LABEL_FRAME_RANGE_BEGIN         "lblBeginFrameRange"
#define LABEL_FRAME_RANGE_END           "lblEndFrameRange"
#define SLIDER_FRAME_RANGE_BEGIN        "sldBeginFrameRange"
#define SLIDER_FRAME_RANGE_END          "sldEndFrameRange"
#define SPINBOX_FRAME_RANGE_BEGIN       "sbxBeginFrameRange"
#define SPINBOX_FRAME_RANGE_END         "sbxEndFrameRange"
#define EDIT_NAME_LIMIT_NEGX            "edXLimNegative"
#define EDIT_NAME_LIMIT_POSX            "edXLimPositive"
#define EDIT_NAME_LIMIT_NEGY            "edYLimNegative"
#define EDIT_NAME_LIMIT_POSY            "edYLimPositive"

/**
 * @brief The ChartHandler class
 * the ChartHandler class will organise all charts and items.
 * the handler will create option-widgets, where you can define the chart.
 *
 * the entire sequence is controlled by public slots!
 */
class ChartHandler : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief ChartHandler
   * default-constructor
   */
  ChartHandler();

  /**
   * @brief createChartWidget
   * creates a widget. the content is specified by the playlistitem
   *
   * @param aItem
   * the playListItem will define, which options are avaible
   *
   * @return
   * if item is defined / known in the function, it will return the options-widget
   * otherwise it will return a default-widget
   */
  QWidget* createChartWidget(playlistItem* aItem);

  /**
   * @brief getStatisticTitle
   * the title is specified by the playlistitem
   *
   * @param aItem
   * the playListItem will define, which title it will have
   *
   * @return
   * if item is defined / known in the function, it will return the title
   * otherwise it will return a default-widget
   */
  QString getChartsTitle(playlistItem* aItem);

  /**
   * @brief removeWidgetFromList
   * removes a widget from the list and
   *
   * @param aItem
   * defines which widget will remove
   *
   */
  void removeWidgetFromList(playlistItem* aItem);

  /**
   * @brief setChartWidget
   * setting the ChartWidget, so we can show the charts later
   *
   * @param aChartWidget
   * the chartwidget, where later charts can be placed
   *
   */
  void setChartWidget(ChartWidget* aChartWidget) {this->mChartWidget = aChartWidget;}

  /**
   * @brief setPlaylistTreeWidget
   * setting the PlaylistTreeWidget, because we want to know which items are selected
   * actual support only for one! item
   *
   * @param aPlayLTW
   * a reference to the playlist
   */
  void setPlaylistTreeWidget( PlaylistTreeWidget *aPlayLTW ) { this->mPlaylist = aPlayLTW; }

  /**
   * @brief setPlaybackController
   * setting the PlaybackController, maybe we can use it for define the Framerange
   *
   * @param aPBC
   * a reference to the playback
   */
  void setPlaybackController( PlaybackController *aPBC ) { this->mPlayback = aPBC; }


  chartSettingsData createStatisticsChartSettings(itemWidgetCoord& aCoord);

public slots:
  void asynchFinished();

  /**
   * @brief currentSelectedItemsChanged
   * slot, after the playlist item selection have changed
   * will set new title, set the new Chartwidget
   *
   * @param aItem1
   * first playlistItem, define what happens after change
   *
   * @param aItem2
   * no support for a second playListItem
   */
  void currentSelectedItemsChanged(playlistItem *aItem1, playlistItem *aItem2);

  /**
   * @brief itemAboutToBeDeleted
   * slot, item will be deleted from the playlist
   *
   * @param aItem
   * define which widget can be delete
   */
  void itemAboutToBeDeleted(playlistItem *aItem);

  /**
   * @brief playbackControllerFrameChanged
   * slot, after the playbackcontroller was moved, so the selected frame changed
   *
   * @param aNewFrameIndex
   * no support, because, we get the Index later new.
   * Necessry because if a item has more frames than an other we have to set the index by ourself
   */
  void playbackControllerFrameChanged(int aNewFrameIndex);

protected:
  /**
   * @brief timerEvent
   * the timerevent will be used to check that an item is loaded, after that we can load the Chartwidget
   *
   * @param event
   * event to check if we have the same event from the timer
   */
  void timerEvent(QTimerEvent *event);


private slots:
/*----------playListItemStatisticsFile----------*/
  /**
   * @brief onStatisticsChange
   * if the selected statistic-type changed, the chart has to be updated
   *
   * @param aString
   * statistic-Type, which was selected
   */
  void onStatisticsChange(const QString aString);

  /**
   * @brief switchOrderEnableStatistics
   * if the selected statistic-type changed, we have to decide if the order/sort-combobox is enabled
   *
   * @param aString
   * statistic-Type, which was selected
   */
  void switchOrderEnableStatistics(const QString aString);

  /**
   * @brief sliderRangeChange
   * if the slider change (avaible in csRange)
   *
   * @param aValue
   * new frameindex (unused)
   */
  void sliderRangeChange(int aValue);

  /**
   * @brief spinboxRangeChange
   * if the spinbox change (avaible in csRange)
   *
   * @param aValue
   * new frameindex (unused)
   */
  void spinboxRangeChange(int aValue);

private:
// variables
  YUVChartFactory mYUVChartFactory;
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
  chartOrderBy mLastChartOrderBy = cobUnknown;
  // save the last selected statistics-type
  QString mLastStatisticsType = "";

  QCheckBox* mCbDrawChart;

  //list of all created Widgets and items
  QVector<itemWidgetCoord> mListItemWidget;
  // Pointer to the TreeWidget
  QPointer<PlaylistTreeWidget> mPlaylist;
  // Pointer to the PlaybackController
  QPointer<PlaybackController> mPlayback;

  QBasicTimer mTimer;

  bool mDoMultiThread = true;
  bool mCancelBackgroundParser = false;
  QFuture<chartSettingsData> mBackgroundParserFuture;
  QFutureWatcher<chartSettingsData> mFutureWatcherWidgets;
  QList<QPair<QFuture<chartSettingsData>, itemWidgetCoord>> mMapFutureItemWidgetCoord;


// functions
/*----------auxiliary functions----------*/
 /*----------generel functions----------*/
  /**
   * @brief getItemWidgetCoord
   * try to find an itemWidgetCoord to a specified item
   *
   * @param aItem
   * item to search
   *
   * @return
   * if found returns a valid itemWidgetCoord
   * otherwise return a coord with null
   */
  itemWidgetCoord getItemWidgetCoord(playlistItem* aItem);

  /**
   * @brief placeChart
   * place a widget in the itemWidgetCoord on the chart
   *
   * @param aCoord
   * where aChart has to be placed
   *
   * @param aChart
   * will be placed
   */
  void placeChart(itemWidgetCoord aCoord, QWidget* aChart);

  /**
   *
   * IMPORTANT the comboboxes has no connect!!! so connect it so something you want to do with
   *
   * @brief generateOrderWidgetsOnly
   * function defines the widgets to order the chart-values
   *
   * @param aAddOptions
   * true: options are avaible
   * false: no otions are avaible, just a default-information
   *
   * @return
   * a List with the order-gruop-normalize options
   */
  QList<QWidget*> generateOrderWidgetsOnly(bool aAddOptions);

  /**
   * @brief generateOrderByLayout
   * generates a layout, widget from "generateOrderWidgetsOnly" will be placed
   *
   * @param aAddOptions
   * true: options are avaible
   * false: no otions are avaible, just a default-information
   *
   * @return
   * a QHBoxLayout where the components/widgets are added
   */
  QLayout* generateOrderByLayout(bool aAddOptions);

  /**
   * @brief setRangeToComponents
   * we set the range to the slider and the spinboxes
   *
   * @param aCoord
   * itemWidgetCoord::mWidget will use for QObject::children() if aObject is NULL
   *
   * @param aObject
   * necessary object, we call the function QObject::children() if not NULL otherwise itemWidgetCoord::mWidget will use
   */
  void setRangeToComponents(itemWidgetCoord aCoord, QObject* aObject = NULL);

  /**
   * @brief getFrameRange
   * the function will create an indexRange for the selected range in case of csRange
   *
   * @param aCoord
   * itemWidgetCoord::mWidget will searched for the slider/spinboxes
   *
   * @return
   * an indexRange, build from the beginslider-value and endslider-value
   */
  indexRange getFrameRange(itemWidgetCoord aCoord);

  /**
   * @brief rangeChange
   * function, which will react to the slider-spinbox-valuechanged event
   * is used by ChartHandler::sliderRangeChange and ChartHandler::spinboxRangeChange
   *
   * @param aSlider
   * true: slider was the sender
   *
   * @param aSpinbox
   * true: spinbox was the sender
   *
   * !!take care, that one of both bools is false and the other one is true!!
   */
  void rangeChange(bool aSlider = true, bool aSpinbox = false);


/*----------playListItemStatisticsFile----------*/
  /**
   * @brief createStatisticFileWidget
   * creates Widget based on an "playListItemStatisticsFile"
   *
   * @param aItem
   * a explicit playlistItemStatisticsFile
   *
   * @param aCoord
   * a itemWidgetCoord where data can be saved in
   *
   * @return
   * a option-widget for the playlistItemStatisticsFile
   */
  QWidget* createStatisticFileWidget(playlistItemStatisticsFile* aItem, itemWidgetCoord& aCoord);

  /**
   * @brief createStatisticsChart
   * creating the Chart depending on the data
   *
   * @param aCoord
   * data for the chart
   *
   * @return
   * a chartview, that can be placed
   */
  QWidget* createStatisticsChart(itemWidgetCoord& aCoord);
};


/**
 * NEXT FEATURES / KNOWN BUGS
 *
 * (done) -  no chart should be selectable, if file is not completly loaded (thread)
 *        -- if thread is ready send a message to the charthandler
 *        -- or still wait in a loop (seems to be the bad way :D)
 *
 * (done) -  order-by-dropdown change to 3 dropdwon (show: per frame / all frames; group by: value / blocksize; normalize : none / by area (dimension of frame)
 * (done) --   Show: per frame / all frames
 * (done) --   Group by: value / blocksize
 * (done) --   Normalize: none / by area (values dimension compare to complete dimension of frame)
 *
 * (done) - normalize: dimension of frame, get real dimension of frame and don't calculate it
 *
 * (done) -  change enum ChartOrderBy
 * (done) --   change enum functions
 * (done) --   maybe place enum in typedef.h or create new File ChartHandlerDefinition.h
 *
 * (done) -  widget mNoDatatoShowWiget and mDataIsLoadingWidget make better look and feel (LAF)
 * (done) -- label with the information should be dynamicly changeable (linebreaks ...)
 *
 * (done) -  implement new widget for order-group-settings
 *
 * (done) -  implement all possible settings
 *
 * (done) -  implement for value is vector-type
 *
 * () -  implement same function for the other playlistitems
 *
 * () -  test on windows / mac
 *
 * (done) - implement mechanism, that the chart doesn't load every time new, if frameindex change
 *
 * () - implement settings widget to set chart-type
 *    -- save / load settings
 *
 * () - implement different chart-types (bar, pie, ...)
 * (done) -- implement Interface, that  the base is for diffrent types of charts
 * () -- getting better labels for the axes
 *
 * () - implement calculating and creating the chart in an seperate thread, not in main-thread
 *
 * () - save last selected frameindex in charthandler? so we can change the frameindex after the selected file has changed
 *
 * () - maybe?: implement below the chartwidget a grid, which contains all absolut data from the chart (shows data, that maybe get lost in the chart)
 *
 * () - get more than one instance of ChartWidget to one playListItem --> better comparison of the data, between different frames (maybe, we can use the overlay-item-container)
 *
 * () - implement as option: if selecting a statistic-type in "Statistics File Propertiers" update the chartwidget
 *      -- as connect; setting with a checkbox
 *
 * () -load more than one playListItem, just one file will be loaded and the other one after selecting it
 *    -- cache the other playlistItem in the background, after change the item, it can be shown directly
 *    -- switching to another playlistitem directy after the FileSelect-Dialog, YUView will crash --> playListItemStatisticsFile.cpp (int type = rowItemList[5].toInt(); there is nothing at Index 5-- line 497)
 *
 * () -export the charts as files
 *    -- graphic?
 *    -- all data as list
 *    --- including: type-show-group-normalize -- data sort and categorized as a list
 *
 * () -save / load charts
 *    --must include the last settings (comboboxes)
 *    --load the last items --> save the filepath from the items
 *    ---if file can't be loaded (maybe not exits anymore) mark chart-savefile as broken
 */
#endif // CHARTHANDLER_H
