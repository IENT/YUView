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
#define CHARTSWIDGET_WINDOW_TITLE_IMAGE "Image File Chart"

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

#define CBX_LABEL_IMAGE_TYPE        "Images: "
#define CBX_LABEL_RGB               "RGB"

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


/*-------------------- Struct collectedData --------------------*/
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
  // each int* should be an array with two ints
  // first: value
  // second: count, how often the value was found in the frame
  QList<int*> mValueList;

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
    this->mLabel = aData->mLabel;
    this->mValueList = aData->mValueList;
  }

  // destructor
  ~collectedData()
  {
    this->mValueList.clear();
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

};


/*-------------------- Struct itemWidgetCoord --------------------*/
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
  QMap<QString, QList<QList<QVariant>>>* mData;

  /**
   * @brief itemWidgetCoord
   * default-constructor
   */
  itemWidgetCoord()
    : mItem(NULL), mWidget(NULL),mChart(new QStackedWidget), mData(NULL) // initialise member
  {}

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
};


/*-------------------- Struct chartSettingsData --------------------*/
/**
 * @brief The chartSettingsData struct
 * will collect all information about the setting to the chart.
 */
struct chartSettingsData
{
  bool mSettingsIsValid = true;
  QStringList mCategories;
  QAbstractSeries* mSeries;
  QHash<QString, QBarSet*> mTmpCoordCategorieSet;
};


/*-------------------- Enum ChartOrderBy --------------------*/
/**
 * @brief The ChartOrderBy enum
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
enum ChartOrderBy
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
 * @brief The ChartShow enum
 * which options we have to show the data
 */
enum ChartShow
{
  csPerFrame,     // show for each frame
  csRange,        // show for an range
  csAllFrames,    // show for all frames
  csUnknown       // if not definied
};

/**
 * @brief The ChartGroupBy enum
 * which options we have to group the data
 */
enum ChartGroupBy
{
  cgbByValue,     // group by value
  cgbByBlocksize, // group by blocksize
  cgbUnknown      // if not definied
};

/**
 * @brief The ChartNormalize enum
 * which options we have to normalize the data
 */
enum ChartNormalize
{
  cnNone,         // no normalize
  cnByArea,       // will be normalized by the complete area (all pixels of an picture
  cnUnknown       // if not definied
};


/*-------------------- Enum Class functions  --------------------*/
/**
 * @brief The EnumAuxiliary class
 * Every enum-function should be inserted as a static function, not as globalfuntion.
 * there is no difference, but it's easier to read int the code later
 */
class EnumAuxiliary : private QObject {
  Q_OBJECT

  public:
/*-------------------- ChartOrderBy --------------------*/
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
  static QString asString(ChartOrderBy aEnum);

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
  static QString asTooltip(ChartOrderBy aEnum);

/*-------------------- ChartShow --------------------*/
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
  static QString asString(ChartShow aEnum);

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
  static QString asTooltip(ChartShow aEnum);

/*-------------------- ChartGroupBy --------------------*/
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
  static QString asString(ChartGroupBy aEnum);

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
  static QString asTooltip(ChartGroupBy aEnum);

/*-------------------- ChartNormalize --------------------*/
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
  static QString asString(ChartNormalize aEnum);

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
  static QString asTooltip(ChartNormalize aEnum);

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
  static ChartOrderBy makeChartOrderBy(ChartShow aShow, ChartGroupBy aGroup, ChartNormalize aNormalize);
};

// Metatype-Information
// necessary that QVariant can handle the enums
Q_DECLARE_METATYPE(ChartOrderBy)
Q_DECLARE_METATYPE(ChartShow)
Q_DECLARE_METATYPE(ChartGroupBy)
Q_DECLARE_METATYPE(ChartNormalize)

#endif // CHARTHANDLERTYPEDEF_H
