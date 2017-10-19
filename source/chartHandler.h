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
  QString getStatisticTitle(playlistItem* aItem);

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

public slots:
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

  void sliderRangeChange(int aValue);
  void spinboxRangeChange(int aValue);
  void rangeChange(bool aSlider = true, bool aSpinbox = false);

private:
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



  void setSliderRange(itemWidgetCoord aCoord);

  /**
   * @brief sortAndCategorizeData
   * the data from the frame will be ordered and categorized by his value
   *
   * @param aCoord
   * all data from the statistics
   *
   * @param aType
   * statistic-type
   *
   * @param aFrameIndex
   * actual viewed frame
   *
   * @return
   * a list of sort and categorized data for the viewed frame
   */
  QList<collectedData>* sortAndCategorizeData(const itemWidgetCoord aCoord, const QString aType, const int aFrameIndex);

  /**
   * @brief sortAndCategorizeDataAllFrames
   * the data from the frame will be ordered and categorized by his value
   * calls internally sortAndCategorizeData for each frame of the item
   *
   * @param aCoord
   * all data from the statistics and the item
   *
   * @param aType
   * statistic-type
   *
   * @return
   * a list of sort and categorized data
   */
  QList<collectedData>* sortAndCategorizeDataAllFrames(const itemWidgetCoord aCoord, const QString aType);

  /**
   * @brief makeStatistic
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @param aOrderBy
   * option-enum how the sorted Data will display
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @return
   * a chartview, that can be placed
   */
  QWidget* makeStatistic(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy, playlistItem* aItem);

  /**
   * @brief makeStatisticsPerFrameGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobPerFrameGrpByValueNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsPerFrameGrpByValNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobPerFrameGrpByValueNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem);

  /**
   * @brief makeStatisticsPerFrameGrpByBlocksizeNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsPerFrameGrpByBlocksizeNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem);

  /**
   * @brief makeStatisticsAllFramesGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobAllFramesGrpByValueNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByValNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobAllFramesGrpByValueNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem);

  /**
   * @brief makeStatisticsAllFramesGrpByBlocksizeNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByBlocksizeNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataAllFrames
   * provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataAllFrames
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem);

  /**
   * @brief calculateAndDefineGrpByValueNrmArea
   * the function will calculate the ratio depends of the values and the total amount of pixel
   *
   * @param aSortedData
   * given data
   *
   * @param aTotalAmountPixel
   * the amount of pixel for the frame(s)
   *
   * @return
   * defined chartSettingsData
   */
  chartSettingsData calculateAndDefineGrpByValueNrmArea(QList<collectedData>* aSortedData, int aTotalAmountPixel);

  /**
   * @brief calculateAndDefineGrpByBlocksizeNrmArea
   * the function will calculate the ratio depends of the blocksize and the total amount of pixel
   *
   * @param aSortedData
   * given data
   *
   * @param aTotalAmountPixel
   * the amount of pixel for the frame(s)
   *
   * @return
   * defined chartSettingsData
   */
  chartSettingsData calculateAndDefineGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, int aTotalAmountPixel);

  /**
   * @brief getTotalAmountOfPixel
   * the function will calculate the total amount of pixel
   *
   * @param aItem
   * from the item, we get the frame-dimension and the amount of frames
   *
   * @param aShow
   * from ChartShow csPerFrame for one frame (default) or csAllFrames for all frames
   *
   * @return
   * total amount of pixel in the viewed range (one frame or all frames)
   */
  int getTotalAmountOfPixel(playlistItem* aItem, ChartShow aShow);

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
 * () -  no chart should be selectable, if file is not completly loaded (thread)
 *    -- if thread is ready send a message to the charthandler
 *    -- or still wait in a loop (seems to be the bad way :D)
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
 * () -  widget mNoDatatoShowWiget and mDataIsLoadingWidget make better look and feel (LAF)
 * (done) -- label with the information should be dynamicly changeable (linebreaks ...)
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
 *    -- save / load settings
 *
 * () - implement different chart-types (bar, pie, ...)
 * () -- implement Interface, that  the base is for diffrent types of charts
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
