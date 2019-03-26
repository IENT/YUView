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

#ifndef CHARTHANDLERTYPEDEF_H
#define CHARTHANDLERTYPEDEF_H

#include <QtCharts>
#include "typedef.h"

// define class prototypes, so we don´t have to do the inlcude in this header file
class playlistItem;

/**
 * Information about handling this file
 *
 * adding a new struct:
 *    - small information what the struct is for
 *
 * adding a new enum:
 *    - small information about the enum
 *    - all enums have a prefix
 *    - all enums hava a Unknown inside as last one
 *    - add little comment for what the enum-type is
 *    - add enum-functions (if necessary) to the EnumAuxiliary-class (all functions are static)
 *    - add Q_DECLARE_METATYPE if QVariant has to handle the enum allways at the end of the file
 *
 * append a new enum-type to an existing enum:
 *    - append new type with prefix, always over prefix+Unknown
 *    - add little comment for what the enum-type is
 *    - change existing functions in EnumAuxiliary-class
 *
 * delete an enum-type from an existing enum:
 *    - prefix+Unknown must be last one
 *    - change existing functions in EnumAuxiliary-class
 *
 * define constants for window-title:
 *    - always in big letters
 *    - const has form of: CHARTSWIDGET_WINDOW_TITLE_???
 *    - replace ??? with an one word description
 *
 */

/*-------------------- consts window title --------------------*/
#define CHARTSWIDGET_WINDOW_TITLE_DEFAULT "Charts"
#define CHARTSWIDGET_WINDOW_TITLE_STATISTICS "Statistics File Chart"

/*-------------------- consts text --------------------*/
#define WIDGET_NO_DATA_TO_SHOW      "No data to show.\nPlease select another combination or change the currently viewed frame."
#define WIDGET_DATA_IS_LOADING      "Data is loading.\nPlease wait.\nThe possible options will show automaticly after loading."
#define CBX_LABEL_STATISTICS_TYPE   "Statistics: "
#define CBX_OPTION_NO_TYPES         "No types"
#define CBX_OPTION_SELECT           "Select..."
#define CBX_LABEL_FRAME             "Show for: "
#define CBX_LABEL_GROUPBY           "Group by: "
#define CBX_LABEL_NORMALIZE         "Normalize: "
#define SLIDER_LABEL_BEGIN_FRAME    "Begin frame: "
#define SLIDER_LABEL_END_FRAME      "End frame: "


/*-------------------- enum statisticsDataType --------------------*/
/**
 * @brief defintion to interpreting the data of the statistic
 *
 */
enum statisticsDataType
{
  sdtInt,                         // value is type of integer
  sdtRGB,                         // value is type of RGB
  sdtStructStatisticsItem_Value,  // value is type of struct --> statisticItem_Value
  sdtStructStatisticsItem_Vector, // value is type of struct --> statisticItem_Vector
  sdtObjectStatisticsData,        // value is type of object --> statisticData
  sdtUnknown                      // always the last one if undefined or just dont know
};

/*-------------------- enum chartType --------------------*/
/**
 * @brief The chartType2D enum
 * enum to define which 2D chart type we want
 * for example bar-chart, pie-chart and so on
 */
enum chartType2D
{
  ct2DBarChart,           // draw a bar chart
  ct2DUnknown             // undefined
};

/**
 * @brief The chartType2D enum
 * enum to define which 2D chart type we want
 * for example bar-chart, pie-chart and so on
 */
enum chartType3D
{
  ct3DBarChart,           // draw a 3D bar chart
  ct3DSurfaceChart,       // draw a 3D surface chart
  ct3DUnknown             // undefined
};

/*-------------------- struct collectedData --------------------*/
/**
 * @brief The collectedData struct
 * small struct to avoid big return-types
 */
struct collectedData
{
  // in case of different types of data
  statisticsDataType mStatDataType = sdtUnknown;

  // the label
  QString mLabel = "";

  // list of all values
  // QPair can be used to count all possible value-types (int, QString, QPoint, ...)
  // the QPair is always a specifc type and the total amount of the type
  QList<QPair<QVariant, int>*> mValues;

  /**
   * @brief collectedData
   * default-constructor
   */
  collectedData() {}

  /**
   * @brief collectedData
   * copy-constructor
   *
   * @param aData
   * an already existing collectedData
   */
  collectedData(const collectedData* aData)
  {
    this->mLabel  = aData->mLabel;
    this->mValues = aData->mValues;
  }

  /**
   * @brief addValue
   * adding a specific combination of QVariant and amount to the valuelist
   *
   * @param aTypeValue
   * qvariant of type
   *
   * @param aAmount
   * how often does the qvariant exist
   */
  void addValue(QVariant aTypeValue, int aAmount)
  {
    QPair<QVariant, int>* pair = new QPair<QVariant, int>(aTypeValue, aAmount);

    this->mValues.append(pair);
  }

  /**
   * @brief addValues
   * adding all values from an existing collectedData
   *
   * @param aData
   * existing collectedData to append
   */
  void addValues(collectedData aData)
  {
    for (int i = 0; i < aData.mValues.count(); i++)
    {
      auto valuePair = aData.mValues.at(i);
      this->addValue(valuePair->first, valuePair->second);
    }
  }

  /**
   * @brief addValueList
   * adding a list of QPair-pointers to the collectedData
   *
   * @param aList
   * list to append
   */
  void addValueList(QList<QPair<QVariant, int>*>* aList)
  {
    for (int i = 0; i < aList->count(); i++)
    {
      auto valuePair = aList->at(i);
      this->addValue(valuePair->first, valuePair->second);
    }
  }

  /**
   * @brief operator ==
   * check that the Pointer on the items are equal
   *
   * @param aData
   * collectedData to check
   *
   * @return
   * true, if label-pointer are equal
   * otherwise false
   */
  bool operator==(const collectedData& aData) const
  {
    return (mLabel == aData.mLabel);
  }

  /**
   * @brief operator ==
   * check that the Pointer on the items are equal
   *
   * @param aData
   * collectedData to check
   *
   * @return
   * true, if label-pointer are equal
   * otherwise false
   */
  bool operator==(const collectedData* aData)
  {
    return (this->mLabel == aData->mLabel);
  }

  /**
   * @brief operator ==
   * check that the Pointer on the items are equal
   *
   * @param aData
   * collectedData to check
   *
   * @return
   * true, if label-pointer are equal
   * otherwise false
   */
  bool operator==(const collectedData aData)
  {
    return (this->mLabel == aData.mLabel);
  }

  bool is3DData()
  {
    return this->mStatDataType == sdtStructStatisticsItem_Vector;
  }

};

/*-------------------- struct chartSettingsData --------------------*/
/**
 * @brief The chartSettingsData struct
 * will collect all information about the setting to the chart.
 */
struct chartSettingsData
{
  // bool to check if our data is valid
  bool mSettingsIsValid = true;

  // check what we have for an type, so we can decide to build the chart a bit other
  statisticsDataType mStatDataType = sdtUnknown;

  // we can check to set custom axes, the custom axes will depend on the statisticsDataType
  bool mSetCustomAxes = false;

  // check if we have to show the legend of the chart
  bool mShowLegend = false;

  bool mIsNormalized = false;

  // list of categories
  QStringList mCategories;

  // for 2D data we use the QAbstractSeries
  // the abstract series can be placed  to QChart
  QAbstractSeries* mSeries;

  //for 3D data we use
  QMap<int, QMap<int, double>> m3DData;
  bool mIs3DData = false;

  // use maybe later for caching or something else
  indexRange mX3DRange = indexRange(0, 0);
  indexRange mY3DRange = indexRange(0, 0);

  /**
   * @brief define3DRanges
   * for use in 3d data. it determines the ranges of x and y components
   * the given parameter are maximum / minimum.
   * the function checks, that the values are possible
   *
   * @param aMinX
   * minimum of x
   *
   * @param aMaxX
   * maximum of x
   *
   * @param aMinY
   * minimum of y
   *
   * @param aMaxY
   * maximum of y
   */
  void define3DRanges(int aMinX, int aMaxX, int aMinY, int aMaxY)
  {
    // set result-vars to default
    int xmin = INT_MAX;
    int xmax = INT_MIN;
    int ymin = INT_MAX;
    int ymax = INT_MIN;

    // go thru the elements and save the min and  max
    foreach (int x, this->m3DData.keys())
    {
      if(xmin > x)
        xmin = x;

      if(xmax < x)
        xmax = x;

      QMap<int, double> innermap = this->m3DData.value(x);

      foreach (int y, innermap.keys())
      {
        if(ymin > y)
          ymin = y;

        if(ymax < y)
          ymax = y;
      }
    }

    mX3DRange.first   = xmin;
    mX3DRange.second  = xmax;
    mY3DRange.first   = ymin;
    mY3DRange.second  = ymax;

    if((aMinX != INT_MIN) && (aMinX < mX3DRange.first))
      mX3DRange.first = aMinX;

    if((aMaxX != INT_MAX) && (aMaxX > mX3DRange.second))
      mX3DRange.second = aMaxX;

    if((aMinY != INT_MIN) && (aMinY < mY3DRange.first))
      mY3DRange.first = aMinY;

    if((aMaxY != INT_MAX) && (aMaxY > mY3DRange.second))
      mY3DRange.second = aMaxY;
  }
};

/*-------------------- struct itemWidgetCoord --------------------*/
/**
 * @brief The itemWidgetCoord struct
 * necesseray, because if we want to use QMap or QHash,
 * we have to implement the <() operator(QMap) or the ==() operator(QHash)
 * a small work around, just implement the ==() based on the struct
 */
struct itemWidgetCoord
{
  playlistItem* mItem;
  QWidget*      mWidget;
  QStackedWidget*      mChart;

  bool mHasSettings = false;
  chartSettingsData mSettings;


  /**
   * @brief itemWidgetCoord
   * default-constructor
   */
  itemWidgetCoord()
    : mItem(NULL), mWidget(NULL), mChart(new QStackedWidget)// initialise member
  {}

  /**
   * @brief itemWidgetCoord
   * copy-constructor
   *
   * @param aCoord
   * element that have to copy
   */
  itemWidgetCoord(const itemWidgetCoord& aCoord)
  {
    this->mItem = aCoord.mItem;
    this->mWidget = aCoord.mWidget;
    this->mChart = aCoord.mChart;
  }

  /*
    *@brief ~itemWidgetCoord
    * default-destructor
    */
  ~itemWidgetCoord()
  {
  }

  /**
   * @brief operator ==
   * check that the Pointer on the items are equal
   *
   * @param aCoord
   * an already existing itemWidgetCoord
   *
   * @return
   * true, if both item pointers are the same item
   * otherwise false
   */
  bool operator==(const itemWidgetCoord& aCoord) const
  {
    return (mItem == aCoord.mItem);
  }

  /**
   * @brief operator ==
   * check that the Pointer on the items are equal
   *
   * @param aCoord
   * an already existing itemWidgetCoord
   *
   * @return
   * true, if both item pointers are the same item
   * otherwise false
   */
  bool operator==(const playlistItem* aItem) const
  {
    return (mItem == aItem);
  }

  /**
   * @brief setSettings
   * if chartSettingsData was calculated, we can save it
   * if a chartSettingsData was set, a boolean to check is set to true
   *
   * @param aSettings
   * chartSettingsData struct which should be set
   */
  void setSettings(chartSettingsData aSettings)
  {
    this->mSettings = aSettings;
    this->mHasSettings = true;
  }
};

/*-------------------- Enum ChartOrderBy --------------------*/
/**
 * @brief The chartOrderBy enum
 * options how the data can be displayed
 *
 * if change the enum, change the enum-methods to it too
 * enum construction:
 *
 * prefix + show + group by + normalize
 *
 *
 * 1: cob
 * 2: PerFrame / AllFrame
 * 3: GrpByValue / GrpByBlocksize
 * 4: NrmNone / NrmByArea
 *
 * example:
 * cobPerFrameGrpByValueNrmNone;
 *
 * last one is always cobUnknown
 */
enum chartOrderBy
{
  cobPerFrameGrpByValueNrmNone,         // order: each frame,   group by value,     no normalize
  cobPerFrameGrpByValueNrmByArea,       // order: each frame,   group by value,     normalize by Area

  cobPerFrameGrpByBlocksizeNrmNone,     // order: each frame,   group by blocksize, no normalize
  cobPerFrameGrpByBlocksizeNrmByArea,   // order: each frame,   group by blocksize, normalize by Area

  cobRangeGrpByValueNrmNone,            // order: frame range,  group by value,     no normalize
  cobRangeGrpByValueNrmByArea,          // order: frame range,  group by value,     normalize by Area

  cobRangeGrpByBlocksizeNrmNone,        // order: frame range,  group by blocksize, no normalize
  cobRangeGrpByBlocksizeNrmByArea,      // order: frame range,  group by blocksize, normalize by Area

  cobAllFramesGrpByValueNrmNone,        // order: all frames,   group by value,     no normalize
  cobAllFramesGrpByValueNrmByArea,      // order: all frames,   group by value,     normalize by Area

  cobAllFramesGrpByBlocksizeNrmNone,    // order: all frames,   group by blocksize, no normalize
  cobAllFramesGrpByBlocksizeNrmByArea,  // order: all frames,   group by blocksize, normalize by Area

  cobUnknown                            // no order
};

/**
 * @brief The chartShow enum
 * which options we have to show the data
 */
enum chartShow
{
  csPerFrame,     // show for each frame
  csRange,        // show for an range
  csAllFrames,    // show for all frames
  csUnknown       // if not definied
};

/**
 * @brief The chartGroupBy enum
 * which options we have to group the data
 */
enum chartGroupBy
{
  cgbByValue,     // group by value
  cgbByBlocksize, // group by blocksize
  cgbUnknown      // if not definied
};

/**
 * @brief The chartNormalize enum
 * which options we have to normalize the data
 */
enum chartNormalize
{
  cnNone,         // no normalize
  cnByArea,       // will be normalized by the complete area (all pixels of an picture
  cnUnknown       // if not definied
};


/*-------------------- struct chartCachingInformation --------------------*/
struct chartCachingInformation
{
  playlistItem* mItem;
  indexRange    mRange = indexRange(0, 0);
  chartOrderBy  mChartOrderBy = cobUnknown;
  QString       mType;

  /**
   * @brief operator ==
   * check that enum is equal to an other one
   *
   * @param aData
   * chartCachingInformation to check
   *
   * @return
   * true, if  equal
   * otherwise false
   */
  bool operator==(const chartCachingInformation& aData) const
  {
    bool item       = this->mItem == aData.mItem;
    bool range      = this->mRange == aData.mRange;
    bool chartOrder = this->mChartOrderBy == aData.mChartOrderBy;
    bool type       = this->mType == aData.mType;

    return (item && range && chartOrder && type);
  }
};


/*-------------------- Enum Class functions  --------------------*/
/**
 * @brief The EnumAuxiliary class
 * Every enum-function should be inserted as a static function, not as globalfuntion.
 * there is no difference, but it's easier to read int the code later
 */
class EnumAuxiliary : private QObject
{
  Q_OBJECT

  public:
/*-------------------- chartOrderBy --------------------*/
  /**
   * @brief asString
   * converts the given enum to an readable string
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asString(chartOrderBy aEnum);

  /**
   * @brief asTooltip
   * converts the given enum to an readable tooltip
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asTooltip(chartOrderBy aEnum);

/*-------------------- chartShow --------------------*/
  /**
   * @brief asString
   * converts the given enum to an readable string
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asString(chartShow aEnum);

  /**
   * @brief asTooltip
   * converts the given enum to an readable tooltip
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asTooltip(chartShow aEnum);

/*-------------------- chartGroupBy --------------------*/
  /**
   * @brief asString
   * converts the given enum to an readable string
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asString(chartGroupBy aEnum);

  /**
   * @brief asTooltip
   * converts the given enum to an readable tooltip
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asTooltip(chartGroupBy aEnum);

/*-------------------- chartNormalize --------------------*/
  /**
   * @brief asString
   * converts the given enum to an readable string
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asString(chartNormalize aEnum);

  /**
   * @brief asTooltip
   * converts the given enum to an readable tooltip
   *
   * @param aEnum
   * enum to convert
   *
   * @return
   * readable string
   */
  static QString asTooltip(chartNormalize aEnum);

  /**
   * @brief makeChartOrderBy
   * converts the three given enums to the the ChartOrderBy-Enum
   *
   * @param aShow
   * each frame or all frames
   *
   * @param aGroup
   * by value or by blocksize
   *
   * @param aNormalize
   * normalize by dimension or not
   *
   * @return
   * a created enum, combines the parameters
   */
  static chartOrderBy makeChartOrderBy(chartShow aShow, chartGroupBy aGroup, chartNormalize aNormalize);

  static chartShow getShowType(chartOrderBy aType);
};

// other necessary implementations

/**
 * @brief qHash
 * creates an hash to an QPoint, necessary in using QHash as container
 *
 * @param key
 * QPoint to get an hash
 *
 * @return
 * hash-value
 */
inline uint qHash (const QPoint & key)
{
  return qHash (QPair<int,int>(key.x(), key.y()) );
}


// Metatype-Information
// necessary that QVariant can handle the enums
Q_DECLARE_METATYPE(chartOrderBy)
Q_DECLARE_METATYPE(chartShow)
Q_DECLARE_METATYPE(chartGroupBy)
Q_DECLARE_METATYPE(chartNormalize)
Q_DECLARE_METATYPE(chartType2D)
Q_DECLARE_METATYPE(chartType3D)

// necessary that threads can work handle and work with
Q_DECLARE_METATYPE(itemWidgetCoord)

#endif // CHARTHANDLERTYPEDEF_H
