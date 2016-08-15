/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef STATISTICSEXTENSIONS_H
#define STATISTICSEXTENSIONS_H

#include <QStringList>
#include <QMap>
#include <QPen>
#include "typedef.h"
#if _WIN32 && !__MINGW32__
#include "math.h"
#else
#include <cmath>
#endif

typedef QMap<int,QColor> ColorMap;

/* A color range that maps a color gradient from one color to the other. Bothe minimum and maximum
   color have an assigned minimum and maximum value. The getColor(float) function can be used to 
   get a color from the gradient. The color range also supports color maps with more than two colors.

*/
class ColorRange {
public:
  ColorRange();
  ColorRange(int min, QColor colMin, int max, QColor colMax);
  ColorRange(QStringList row);
  ColorRange(QString rangeName, int min, int max);
  ColorRange(int typeID, int min, int max);
  int getTypeId();
  QColor getColor(float value);

  int rangeMin, rangeMax;
  QColor minColor;
  QColor maxColor;

private:
  static QStringList supportedTyped;
  QString type;
};

enum arrowHead_t {
  arrow=0,
  circle,
  none
};

enum visualizationType_t { colorMapType, colorRangeType, vectorType };
typedef QMap<int, QString> valueMap;
class StatisticsType
{
public:
  StatisticsType();
  StatisticsType(int tID, QString sName, visualizationType_t visType);
  StatisticsType(int tID, QString sName, QString defaultColorRangeName, int rangeMin, int rangeMax);
  StatisticsType(int tID, QString sName, visualizationType_t visType, int cRangeMin, QColor cRangeMinColor, int cRangeMax, QColor cRangeMaxColor );

  void readFromRow(QStringList row);

  // Get the value text (from the value map (if there is an entry))
  QString getValueTxt(int val);

  int typeID;
  QString typeName;
  visualizationType_t visualizationType;
  bool scaleToBlockSize;

  // only for vector type
  int vectorSampling;

  // only one of the next should be set, depending on type
  ColorMap colorMap;
  ColorRange colorRange;

  QColor vectorColor;
  bool scaleVectorToZoom;

  QColor gridColor;

  QPen vectorPen;
  QPen gridPen;
  bool scaleGridToZoom;

  // If set, this map is used to map values to text
  valueMap valMap;

  // parameters controlling visualization
  bool    render;
  bool    renderGrid;
  bool    showArrow;
  bool    mapVectorToColor;
  int     alphaFactor;
  arrowHead_t arrowHead;
};

enum statistics_t
{
  arrowType = 0,
  blockType
};

struct StatisticsItem
{
  statistics_t type;
  QRect positionRect;
  int rawValues[2];
};

#endif // STATISTICSEXTENSIONS_H

