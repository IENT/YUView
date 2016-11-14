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

/* This class knows how to map values to color.
 * There are 3 types of mapping:
 * 1: gradient - We use a min and max value (rangeMin, rangeMax) and two colors (minColor, maxColor).
 *               getColor(rangeMin) will return minColor. getColor(rangeMax) will return maxColor.
 *               In between, we perform linear interpolation.
 * 2: map      - We use a fixed map to map values (int) to color. The values are stored in colorMap.
 * 3: complex  - We use a specific complex color gradient for values from rangeMin to rangeMax.
 *               They are similar to the ones used in MATLAB. The are set by name. supportedComplexTypes
 *               has a list of all supported types.
 */
class colorMapper
{
public:
  colorMapper();
  colorMapper(int min, const QColor &colMin, int max, const QColor &colMax);
  colorMapper(const QString &rangeName, int min, int max);

  QColor getColor(int value);
  QColor getColor(float value);
  double getMinVal();
  double getMaxVal();

  // ID: 0:clorMapperGradient, 1:colorMapperMap, 2+:ColorMapperComplex
  int getID();

  double rangeMin, rangeMax;
  QColor minColor, maxColor;
  QMap<int,QColor> colorMap;    // Each int is mapped to a specific color
  QColor colorMapOther;         // All other values are mapped to this color
  QString complexType;

  // Two colorMappers are identical if they will return the same color when asked for any value.
  // When changing the type of one of the mappers, this might not be true anymore.
  bool operator!=(colorMapper &other);

  enum mappingType
  {
    gradient,
    map,
    complex,
    none
  };
  mappingType type;
  static QStringList supportedComplexTypes;
};

/* This class defines a type of statistic to render. Each statistics type entry defines the name and and ID of a statistic. It also defines
 * what type of data should be drawn for this type and how it should be drawn.
*/
class StatisticsType
{
public:
  StatisticsType();
  StatisticsType(int tID, const QString &sName, int vectorScaling);
  StatisticsType(int tID, const QString &sName, const QString &defaultColorRangeName, int rangeMin, int rangeMax, bool hasAndRenderVectorData=false);
  StatisticsType(int tID, const QString &sName, int cRangeMin, const QColor &cRangeMinColor, int cRangeMax, const QColor &cRangeMaxColor, bool hasAndRenderVectorData=false);

  // Save all the values that the user could change. When saving to playlist we can save only the
  // changed values to playlist.
  void setInitialState();

  // Load/Save status of statistics from playlist file
  void savePlaylist(QDomElementYUView &root);
  void loadPlaylist(const QDomElementYUView &root);

  // Every statistics type has an ID and a name
  int typeID;
  QString typeName;

  // Get the value text (from the value map (if there is an entry))
  QString getValueTxt(int val);

  // If set, this map is used to map values to text
  QMap<int, QString> valMap;

  // Is this statistics type rendered and what is the alpha value?
  // These are corresponding to the controls in the properties panel
  bool render;
  int  alphaFactor;

  // Value data (a certain value, that is set for a block)
  bool        hasValueData;           // Does this type have value data?
  bool        renderValueData;        // Do we render the value data?
  bool        scaleValueToBlockSize;  // Scale the values according to the size of the block
  colorMapper colMapper;              // How do we map values to color?

  // Vector data (a vector that is set for a block)
  bool hasVectorData;       // Does this type have any vector data?
  bool renderVectorData;    // Do we draw the vector data?
  bool scaleVectorToZoom;
  QPen vectorPen;           // How do we draw the vectors
  int  vectorScale;         // Every vector value (x,y) has to be divided by this value before displaying it (e.g. 1/4 th pixel accuracy)
  bool mapVectorToColor;    // Color the vectors depending on their direction
  enum arrowHead_t {
    arrow=0,
    circle,
    none
  };
  arrowHead_t arrowHead;    // Do we draw an arrow, a circle or nothing at the end of the arrow?

  // Do we (and if yes how) draw a grid around each block (vector or value)
  bool renderGrid;
  QPen gridPen;
  bool scaleGridToZoom;

private:
  // We keep a backup of the last used color map so that the map is not lost if the user tries out
  // different color maps.
  QMap<int,QColor> colorMapBackup;

  // Backup values for setDefaultState()
  struct initialState
  {
    bool render;
    int  alphaFactor;

    bool renderValueData;
    bool scaleValueToBlockSize;
    colorMapper colMapper;

    bool renderVectorData;
    bool scaleVectorToZoom;
    QPen vectorPen;
    int  vectorScale;
    bool mapVectorToColor;
    arrowHead_t arrowHead;

    bool renderGrid;
    QPen gridPen;
    bool scaleGridToZoom;
  };
  initialState init;
};

struct statisticsItem_Value
{
  // The position and size of the item. (max 65535)
  unsigned short pos[2];
  unsigned short size[2];

  // The actual value
  int value;
};

struct statisticsItem_Vector
{
  // The position and size of the item. (max 65535)
  unsigned short pos[2];
  unsigned short size[2];

  // The actual vector value
  int vector[2];
};

// A collection of statistics data (value and vector) for a certain context (for example for a certain type and a certain POC).
class statisticsData
{
public:
  statisticsData() { maxBlockSize = 0; }
  void addBlockValue(unsigned short x, unsigned short y, unsigned short w, unsigned short h, int val);
  void addBlockVector(unsigned short x, unsigned short y, unsigned short w, unsigned short h, int vecX, int vecY);
  QList<statisticsItem_Value> valueData;
  QList<statisticsItem_Vector> vectorData;

  // What is the size (area) of the biggest block)? This is needed for scaling the blocks according to their size.
  unsigned int maxBlockSize;
};

#endif // STATISTICSEXTENSIONS_H

