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

#ifndef YUVCHARTS_H
#define YUVCHARTS_H


#include "chartHandlerTypedef.h"
#include "typedef.h"

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
   * @return
   * a complete chartview with the data
   */
  virtual QWidget* createChart(const ChartOrderBy aOrderBy, playlistItem* aItem, indexRange aRange, QString aType) = 0;

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
  int getTotalAmountOfPixel(playlistItem* aItem, ChartShow aShow, indexRange aRange);

protected:
  // holder for invalid data or if no data can display as chart
  QWidget* mNoDataToShowWidget;
  QWidget* mDataIsLoadingWidget;
};

class YUVBarChart : public YUVCharts
{
  Q_OBJECT

  public:
  //reintroduce the constructor
  YUVBarChart(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget) : YUVCharts(aNoDataToShowWidget, aDataIsLoadingWidget){}

  //documentation see @YUVCharts::createChart
  QWidget* createChart(const ChartOrderBy aOrderBy, playlistItem* aItem, indexRange aRange, QString aType) Q_DECL_OVERRIDE;

private:
  /**
   * @brief makeStatistic
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
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
   * a chartview, that can be placed
   */
  QWidget* makeStatistic(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy, playlistItem* aItem, indexRange aRange);

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
  chartSettingsData makeStatisticsPerFrameGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem);

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
  chartSettingsData makeStatisticsFrameRangeGrpByValNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem, indexRange aRange);

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
  chartSettingsData makeStatisticsFrameRangeGrpByBlocksizeNrmArea(QList<collectedData>* aSortedData, playlistItem* aItem, indexRange aRange);

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
};

class YUVLineChart : public YUVCharts
{
    Q_OBJECT

    public:
    //reintroduce the constructor
    YUVLineChart(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget) : YUVCharts(aNoDataToShowWidget, aDataIsLoadingWidget){}

    //documentation see @YUVCharts::createChart
    QWidget* createChart(const ChartOrderBy aOrderBy, playlistItem* aItem, indexRange aRange, QString aType) Q_DECL_OVERRIDE;

private:
  /**
   * @brief makeStatistic
   * creates the chart based on the sorted Data from sortAndCategorizeData or sortAndCategorizeDataByRange
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
   * a chartview, that can be placed
   */
  QWidget* plotLineGraph(QList<collectedData>* aSortedData, const ChartOrderBy aOrderBy, playlistItem* aItem, indexRange aRange);
};


#endif // YUVCHARTS_H
