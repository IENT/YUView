/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2018  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef YUVCHARTS_H
#define YUVCHARTS_H

#include "chartHandlerTypedef.h"
#include "graphmodifier.h"
#include "typedef.h"


/**
 * @brief The CollapsibleWidget class
 * a small widget where a layout will be placed. The content of the layout can collapse
 */
class CollapsibleWidget : public QWidget
{
    Q_OBJECT
public:
  /**
   * @brief CollapsibleWidget
   * creates the base of the CollapsibleWidget (no content)
   *
   * @param aTitle
   * title of the CollapsibleWidget
   *
   * @param aAnimationDuration
   * duration of the animation in ms
   *
   * @param aParent
   * parent of the widget
   */
  explicit CollapsibleWidget(const QString& aTitle = "", const int aAnimationDuration = 300, QWidget* aParent = 0);

  /**
   * @brief setContentLayout
   * new content which can collapse+
   *
   * @param aContentLayout
   * the new layout
   *
   * @param aDisplay
   * default: false
   * if this option is true, the widget will unfold
   */
  void setContentLayout(QLayout& aContentLayout, const bool aDisplay = false);

private:
  // mainlayout to organize
  QGridLayout mMainLayout;
  // button to collapse
  QToolButton mToggleButton;
  // line as marker
  QFrame mHeaderLine;
  // the toggle animation group (down and up)
  QParallelAnimationGroup mToggleAnimation;
  // content
  QScrollArea mContentArea;
  // durations in ms
  int mAnimationDuration;
};

/**
 * @brief The YUVCharts class
 * basic class for charts (abstract-class)
 */
class YUVCharts : public QObject
{
  Q_OBJECT

public:

  /**
   * @brief YUVCharts
   * default-constructor
   * in inherited classes it should be reintroduced or reimplemented always
   *
   * @param aNoDataToShowWidget
   * a basic widget if no data is to show
   *
   * @param aDataIsLoadingWidget
   * a basic widget if the data is still loading
   */
  YUVCharts(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget){this->mNoDataToShowWidget = aNoDataToShowWidget; this->mDataIsLoadingWidget = aDataIsLoadingWidget;}

  /**
   * @brief createChart
   * function which will control how the chart is making
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * the amount of data if avaible
   * this parameter is optional
   * if aSorted is null, we get the data from the item new otherwise we use the parameter
   *
   * @return
   * a complete chartview with the data
   */
  virtual QWidget* createChart(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) = 0;

  /**
   * @brief createChart
   * function will create an chart which depends on the given settings
   *
   * @param aSettings
   * settings how the chart should look
   *
   * @return
   * a complete chartview with the data
   */
  virtual QWidget* createChart(chartSettingsData aSettings) = 0;

  /**
   * @brief createSettings
   * function that creat a complete setting for the chart
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * the amount of data if avaible
   * this parameter is optional
   * if aSorted is null, we get the data from the item new otherwise we use the parameter
   *
   * @return
   * a complete setting; basic for an chart
   */
  virtual chartSettingsData createSettings(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) = 0;

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
  int getTotalAmountOfPixel(playlistItem* aItem, const chartShow aShow, const indexRange aRange);

  /**
   * @brief is2DData
   * the function will check if the data is only 2D
   *
   * @param aSortedData
   * data to check
   *
   * @return
   * true if 2D otherwise false
   */
  bool is2DData(QList<collectedData>* aSortedData);

  /**
   * @brief is3DData
   * the function will check if the data is only 2D
   *
   * @param aSortedData
   * data to check
   *
   * @return
   * true if 3D otherwise false
   */
  bool is3DData(QList<collectedData>* aSortedData);

protected:
  // holder for invalid data or if no data can display as chart
  QWidget* mNoDataToShowWidget;
  QWidget* mDataIsLoadingWidget;
};

/**
 * @brief The YUV3DCharts class
 * default-implementation for all 3D-charts
 */
class YUV3DCharts : public YUVCharts
{
  Q_OBJECT
public:

  /**
   * @brief YUV3DCharts
   * default-constructor
   * in inherited classes it should be reintroduced or reimplemented always
   *
   * @param aNoDataToShowWidget
   * a basic widget if no data is to show
   *
   * @param aDataIsLoadingWidget
   * a basic widget if the data is still loading
   */
  YUV3DCharts(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget);

  /**
   * @brief createChart
   * function which will control hoq the chart is making
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * the amount of data if avaible
   * this parameter is optional
   * if aSorted is null, we get the data from the item new otherwise we use the parameter
   *
   * @return
   * a complete chartview with the data
   */
  QWidget* createChart(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) Q_DECL_OVERRIDE;

  /**
   * @brief createChart
   * function will create an chart which depends on the given settings
   *
   * @param aSettings
   * settings how the chart should look
   *
   * @return
   * a complete chartview with the data
   */
  QWidget* createChart(chartSettingsData aSettings) Q_DECL_OVERRIDE;

  /**
   * @brief createSettings
   * function that creat a complete setting for the chart
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * the amount of data if avaible
   * this parameter is optional
   * if aSorted is null, we get the data from the item new otherwise we use the parameter
   *
   * @return
   * a complete setting; basic for an chart
   */
  chartSettingsData createSettings(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) Q_DECL_OVERRIDE;

  /**
   * @brief hasOpenGL
   * creating an YUV3DBarChart-Object, we check that we can init OpenGL
   *
   * @return
   * true, if it is possible to init OpenGl otherwise false
   */
  bool hasOpenGL() const;

  /**
   * @brief set3DCoordinationRange
   * look at a specific range of the vector
   *
   * @param aMinX
   * minimum x
   *
   * @param aMaxX
   * maximum x
   *
   * @param aMinY
   * minimun y
   *
   * @param aMaxY
   * maximum y
   */
  void set3DCoordinationRange(const int aMinX, const int aMaxX, const int aMinY, const int aMaxY);

  /**
   * @brief set3DCoordinationtoDefault
   * reset the coordinates
   */
  void set3DCoordinationtoDefault();

protected:
  Modifier* mModifier;
  // the graph / chart / histogram
  QWidget* mWidgetGraph;
  // check if we can init OpenGL
  bool mHasOpenGL = false;
  // identifier to use the coordinates
  bool mUse3DCoordinationLimits = false;
  // minimum x coordinate
  int mMinX;
  // maximum x coordinate
  int mMaxX;
  // minimum y coordinate
  int mMinY;
  // maximum y coordinate
  int mMaxY;

  /**
   * @brief makeStatistic
   * creates the setting based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @param aOrderBy
   * option-enum how the sorted Data will display
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @param aRange
   * range, we look at
   *
   * @return
   * a setting, that the chart depends on
   */
  chartSettingsData makeStatistic(QList<collectedData>* aSortedData, const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange);

  /**
   * @brief makeStatisticsPerFrameGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByValueNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobRangeGrpByValueNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsFrameRangeGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobAllFramesGrpByValueNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsPerFrameGrpByValNrm
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByValueNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByValNrm(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByValNrm
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByValueNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsFrameRangeGrpByValNrm(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByValNrm
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobAllFramesGrpByValueNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByValNrm(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsPerFrameGrpByBlocksizeNrmNone
   * creates the chart based on the sorted data from the item
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * settings
   */
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByBlocksizeNrmNone
   * creates the chart based on the sorted data from the item
   * provides the ChartOrderBy: cobRangeGrpByBlocksizeNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * settings
   */
  chartSettingsData makeStatisticsFrameRangeGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByBlocksizeNrmNone
   * creates the chart based on the sorted data from the item
   * provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * the chart to display
   */
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsPerFrameGrpByBlocksizeNrm
   * creates the chart based on the sorted data from the item
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * settings
   */
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrm(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByBlocksizeNrm
   * creates the chart based on the sorted data from the item
   * provides the ChartOrderBy: cobRangeGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * settings
   */
  chartSettingsData makeStatisticsFrameRangeGrpByBlocksizeNrm(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByBlocksizeNrm
   * creates the chart based on the sorted data from the item
   * provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * settings
   */
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrm(QList<collectedData>* aSortedData);

};

/**
 * @brief The YUV3DSurfaceChart class
 * specific class for 3D
 * Surface-Chart
 */
class YUV3DSurfaceChart : public YUV3DCharts
{
  Q_OBJECT

public:

  /**
   * @brief YUV3DSurfaceChart
   * default-constructor
   */
  YUV3DSurfaceChart();

  /**
   * @brief YUV3DSurfaceChart
   * default-constructor
   * in inherited classes it should be reintroduced or reimplemented always
   *
   * @param aNoDataToShowWidget
   * a basic widget if no data is to show
   *
   * @param aDataIsLoadingWidget
   * a basic widget if the data is still loading
   */
  YUV3DSurfaceChart(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget);
};

/**
 * @brief The YUVBarChart class
 * the YUVBarChart class can display 2D Data
 */
class YUVBarChart : public YUVCharts
{
  Q_OBJECT

public:

  /**
   * @brief YUVBarChart
   * default-constructor
   */
  YUVBarChart();

  /**
   * @brief YUVBarChart
   * default-constructor
   * in inherited classes it should be reintroduced or reimplemented always
   *
   * @param aNoDataToShowWidget
   * a basic widget if no data is to show
   *
   * @param aDataIsLoadingWidget
   * a basic widget if the data is still loading
   */
  YUVBarChart(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget) : YUVCharts(aNoDataToShowWidget, aDataIsLoadingWidget){}

  /**
   * @brief createChart
   * function which will control how the chart is making
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * the amount of data if avaible
   * this parameter is optional
   * if aSorted is null, we get the data from the item new otherwise we use the parameter
   *
   * @return
   * a complete chartview with the data
   */
  QWidget* createChart(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) Q_DECL_OVERRIDE;

  /**
   * @brief createChart
   * function will create an chart which depends on the given settings
   *
   * @param aSettings
   * settings how the chart should look
   *
   * @return
   * a complete chartview with the data
   */
  QWidget* createChart(chartSettingsData aSettings) Q_DECL_OVERRIDE;

  /**
   * @brief createSettings
   * function that creat a complete setting for the chart
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * the amount of data if avaible
   * this parameter is optional
   * if aSorted is null, we get the data from the item new otherwise we use the parameter
   *
   * @return
   * a complete setting; basic for an chart
   */
  chartSettingsData createSettings(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) Q_DECL_OVERRIDE;

private:
  /**
   * @brief makeStatistic
   * creates settings the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @param aOrderBy
   * option-enum how the sorted Data will display
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @param aRange
   * range, we look at
   *
   * @return
   * a setting, the chart depends on it
   */
  chartSettingsData makeStatistic(QList<collectedData>* aSortedData, const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange);

  /**
   * @brief makeStatisticsPerFrameGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByValueNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsPerFrameGrpByValNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByValueNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
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
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsPerFrameGrpByBlocksizeNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsFrameRangeGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByValNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @param aRange
   * range, we look at
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsFrameRangeGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem, const indexRange aRange);

  /**
   * @brief makeStatisticsFrameRangeGrpByBlocksizeNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobPerFrameGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsFrameRangeGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByBlocksizeNrmArea
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @param aRange
   * range, we look at
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsFrameRangeGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByValNrmNone
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobAllFramesGrpByValueNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByValNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsAllFramesGrpByValNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobAllFramesGrpByValueNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
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
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmNone
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrmNone(QList<collectedData>* aSortedData);

  /**
   * @brief makeStatisticsFrameRangeGrpByBlocksizeNrmArea
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
   * provides the ChartOrderBy: cobAllFramesGrpByBlocksizeNrmByArea
   *
   * @param aSortedData
   * list of sorted data from sortAndCategorizeData / sortAndCategorizeDataByRange
   *
   * @param aItem
   * from the item we get the actual frame-dimension
   *
   * @return
   * a struct, contains all chart settings and options
   */
  chartSettingsData makeStatisticsAllFramesGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData);

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
  chartSettingsData calculateAndDefineGrpByValueNrmArea(QList<collectedData>* aSortedData, const int aTotalAmountPixel);

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
  chartSettingsData calculateAndDefineGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData);
};

/**
 * @brief The YUV3DBarChart class
 * the YUV3DBarChart class can display 3D Data
 */
class YUV3DBarChart : public YUV3DCharts
{
  Q_OBJECT
public:

  /**
   * @brief YUV3DBarChart
   * default-constructor
   */
  YUV3DBarChart();

  /**
   * @brief YUV3DBarChart
   * default-constructor
   * in inherited classes it should be reintroduced or reimplemented always
   *
   * @param aNoDataToShowWidget
   * a basic widget if no data is to show
   *
   * @param aDataIsLoadingWidget
   * a basic widget if the data is still loading
   */
  YUV3DBarChart(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget);
};

/**
 * @brief The YUVChartFactory class
 * The factory class decides by his own wether it creates a 2D or a 3D plot
 */
class YUVChartFactory : public YUVCharts
{
  Q_OBJECT
public:

  /**
   * @brief YUVChartFactory
   * default-constructor
   */
  YUVChartFactory();

  /**
   * @brief YUVChartFactory
   * default-constructor
   * in inherited classes it should be reintroduced or reimplemented always
   *
   * @param aNoDataToShowWidget
   * a basic widget if no data is to show
   *
   * @param aDataIsLoadingWidget
   * a basic widget if the data is still loading
   */
  YUVChartFactory(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget);

  /**
   * @brief createChart
   * function which will control how the chart is making
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * ! aSortedData is not used in this function
   *
   * @return
   * a complete chartview with the data
   */
  QWidget* createChart(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) Q_DECL_OVERRIDE;

  /**
   * @brief createChart
   * function will create an chart which depends on the given settings
   *
   * @param aSettings
   * settings how the chart should look
   *
   * @return
   * a complete chartview with the data
   */
  QWidget* createChart(chartSettingsData aSettings) Q_DECL_OVERRIDE;

  /**
   * @brief createSettings
   * function that creat a complete setting for the chart
   *
   * @param aOrderBy
   * the order type: frame-art, value or blocksize, noralisation or not
   *
   * @param aItem
   * specified item we look at, we get the data and lot more
   *
   * @param aRange
   * range we look at (also for current frame or all frames)
   *
   * @param aType
   * if statistics-item: the selected type is needed
   *
   * @param aSortedData
   * the amount of data if avaible
   * this parameter is optional
   * if aSorted is null, we get the data from the item new otherwise we use the parameter
   *
   * @return
   * a complete setting; basic for an chart
   */
  chartSettingsData createSettings(const chartOrderBy aOrderBy, playlistItem* aItem, const indexRange aRange, const QString aType, QList<collectedData>* aSortedData = NULL) Q_DECL_OVERRIDE;

  /**
   * @brief set3DCoordinationRange
   * look at a specific range of the vector
   *
   * @param aMinX
   * minimum x
   *
   * @param aMaxX
   * maximum x
   *
   * @param aMinY
   * minimun y
   *
   * @param aMaxY
   * maximum y
   */
  void set3DCoordinationRange(const int aMinX, const int aMaxX, const int aMinY, const int aMaxY);

  /**
   * @brief set3DCoordinationtoDefault
   * reset the coordinates
   */
  void set3DCoordinationtoDefault();

  /**
   * @brief set2DChartType
   * set the new valid type for a 2D chart
   *
   * @param aType
   * new 2D type
   */
  void set2DChartType(chartType2D aType) {this->m2DType = aType;}

  /**
   * @brief set3DChartType
   * set a new valid type for a 3D chart
   *
   * @param aType
   * new 3D type
   */
  void set3DChartType(chartType3D aType) {this->m3DType = aType;}
private:
  // holder for the 2D chart
  chartType2D m2DType = ct2DBarChart;

  // holder for the 3D chart
  chartType3D m3DType = ct3DSurfaceChart;

  // identifier to use the coordinates
  bool mUse3DCoordinationLimits = false;
  // minimum x coordinate
  int mMinX;
  // maximum x coordinate
  int mMaxX;
  // minimum y coordinate
  int mMinY;
  // maximum y coordinate
  int mMaxY;

  // 2D-Barchart
  YUVBarChart   mBarChart;
  // 3D-Barchart
  YUV3DBarChart mBarChart3D;
  // 3D-Surface
  YUV3DSurfaceChart mSurfaceChart3D;
};
#endif // YUVCHARTS_H
