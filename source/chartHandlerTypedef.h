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
#include "playlistItem.h"


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


#define CHARTSWIDGET_WINDOW_TITLE_DEFAULT "Charts"
#define CHARTSWIDGET_WINDOW_TITLE_STATISTICS "Statistics File Chart"


/*-------------------- Struct collectedData --------------------*/
// small struct to avoid big return-types
struct collectedData {
  // the label
  QString mLabel = "";
  // each int* should be an array with two ints
  // first: value
  // second: count, how often the value was found in the frame
  QList<int*> mValueList;

  // default-constructor
  collectedData()
  {}

  // copy-constructor
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

  // check that the Pointer on the items are equal
  bool operator==(const collectedData& aData) const
  {
    return (mLabel == aData.mLabel);
  }
  bool operator==(const collectedData* aData)
  {
    return (this->mLabel == aData->mLabel);
  }
  bool operator==(const collectedData aData)
  {
    return (this->mLabel == aData.mLabel);
  }

};


/*-------------------- Struct itemWidgetCoord --------------------*/
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


/*-------------------- Struct chartSettingsData --------------------*/
// will collect all information about the setting to the chart.
struct chartSettingsData {
  bool mSettingsIsValid = true;
  QStringList mCategories;
  QBarSeries* mSeries = new QBarSeries();
  QHash<QString, QBarSet*> mTmpCoordCategorieSet;
};


/*-------------------- Enum ChartOrderBy --------------------*/
/**
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
enum ChartOrderBy {
  cobPerFrameGrpByValueNrmNone,         // order: each frame, group by value, no normalize
  cobPerFrameGrpByValueNrmByArea,       // order: each frame, group by value, normalize by Area

  cobPerFrameGrpByBlocksizeNrmNone,     // order: each frame, group by blocksize, no normalize
  cobPerFrameGrpByBlocksizeNrmByArea,   // order: each frame, group by blocksize, normalize by Area

  cobAllFramesGrpByValueNrmNone,        // order: all frames, group by value, normalize by Area
  cobAllFramesGrpByValueNrmByArea,      // order: all frames, group by value, normalize by Area

  cobAllFramesGrpByBlocksizeNrmNone,    // order: all frames, group by blocksize, normalize by Area
  cobAllFramesGrpByBlocksizeNrmByArea,  // order: all frames, group by blocksize, normalize by Area

  cobUnknown                            // no order
};

// which options we have to show the data
enum ChartShow {
  csPerFrame,     // show for each frame
  csAllFrames,    // show for all frames
  csUnknown       // if not definied
};

// which options we have to group the data
enum ChartGroupBy {
  cgbByValue,     // group by value
  cgbByBlocksize, // group by blocksize
  cgbUnknown      // if not definied
};

// which options we have to normalize the data
enum ChartNormalize {
  cnNone,         // no normalize
  cnByArea,       // will be normalized by the complete area (all pixels of an picture
  cnUnknown       // if not definied
};

/*-------------------- Enum Class functions  --------------------*/
// Every enum-function should be inserted as a static function, not as globalfuntion.
// there is no difference, but it's easier to read int the code later
class EnumAuxiliary : private QObject {
  Q_OBJECT

  public:
/*-------------------- ChartOrderBy --------------------*/
  // converts the given enum to an readable string
  static QString asString(ChartOrderBy aEnum);
  // converts the given enum to an readable tooltip
  static QString asTooltip(ChartOrderBy aEnum);

/*-------------------- ChartShow --------------------*/
  // converts the given enum to an readable string
  static QString asString(ChartShow aEnum);
  // converts the given enum to an readable tooltip
  static QString asTooltip(ChartShow aEnum);

/*-------------------- ChartGroupBy --------------------*/
  // converts the given enum to an readable string
  static QString asString(ChartGroupBy aEnum);
  // converts the given enum to an readable tooltip
  static QString asTooltip(ChartGroupBy aEnum);

/*-------------------- ChartNormalize --------------------*/
  // converts the given enum to an readable string
  static QString asString(ChartNormalize aEnum);
  // converts the given enum to an readable tooltip
  static QString asTooltip(ChartNormalize aEnum);

  // converts the three given enums to the the ChartOrderBy-Enum
  static ChartOrderBy makeChartOrderBy(ChartShow aShow, ChartGroupBy aGroup, ChartNormalize aNormalize);
};

// Metatype-Information
// necessary that QVariant can handle the enums

Q_DECLARE_METATYPE(ChartOrderBy)
Q_DECLARE_METATYPE(ChartShow)
Q_DECLARE_METATYPE(ChartGroupBy)
Q_DECLARE_METATYPE(ChartNormalize)

#endif // CHARTHANDLERTYPEDEF_H
