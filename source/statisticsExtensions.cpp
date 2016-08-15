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

#include "statisticsExtensions.h"

// All types that are supported by the getColor() function.
QStringList ColorRange::supportedTyped = QStringList() << "jet" << "heat" << "hsv" << "hot" << "cool" << "spring" << "summer" << "autumn" << "winter" << "gray" << "bone" << "copper" << "pink" << "lines" << "col3_gblr" << "col3_gwr" << "col3_bblr" << "col3_bwr" << "col3_bblg" << "col3_bwg";

ColorRange::ColorRange()
{
  rangeMin = 0;
  rangeMax = 0;
  minColor = Qt::black;
  maxColor = Qt::black;
  type     = "twoColor";    // By default, use the two color mode
}

ColorRange::ColorRange(int min, QColor colMin, int max, QColor colMax)
{
  rangeMin = min;
  rangeMax = max;
  minColor = colMin;
  maxColor = colMax;
  type     = "twoColor";    // By default, use the two color mode
}

ColorRange::ColorRange(QStringList row)
{
  ColorRange();

  if (row[1] == "defaultRange")
  {
    // This is a color gradient function
    rangeMin = row[2].toInt();
    rangeMax = row[3].toInt();
    QString rangeName = row[4];
    if (supportedTyped.contains(rangeName))
      type = rangeName;
  }
  else if (row[1] == "range")
  {
    // This is a range with min/max
    rangeMin = row[2].toInt();
    unsigned char minColorR = row[4].toInt();
    unsigned char minColorG = row[6].toInt();
    unsigned char minColorB = row[8].toInt();
    unsigned char minColorA = row[10].toInt();
    minColor = QColor( minColorR, minColorG, minColorB, minColorA );

    rangeMax = row[3].toInt();
    unsigned char maxColorR = row[5].toInt();
    unsigned char maxColorG = row[7].toInt();
    unsigned char maxColorB = row[9].toInt();
    unsigned char maxColorA = row[11].toInt();
    maxColor = QColor( maxColorR, maxColorG, maxColorB, maxColorA );
  }
}

ColorRange::ColorRange(QString rangeName, int min, int max)
{
  ColorRange();
  rangeMin = min;
  rangeMax = max;
  if (supportedTyped.contains(rangeName))
    type = rangeName;
}

ColorRange::ColorRange(int typeID, int min, int max)
{
  ColorRange();
  rangeMin = min;
  rangeMax = max;
  if (typeID > 0 && supportedTyped.count() > typeID - 1)
    type = supportedTyped[typeID-1];
}

int ColorRange::getTypeId()
{
  if (type == "twoColor")
    return 0;
  if (!supportedTyped.contains(type))
    // The selected type is not supported
    return -1;
  return supportedTyped.indexOf(type) + 1;
}

QColor ColorRange::getColor(float value)
{
  if (type == "twoColor")
  {
    // clamp the value to [min max]
    if (value > rangeMax)
      value = (float)rangeMax;
    if (value < rangeMin)
      value = (float)rangeMin;

    // The value scaled from 0 to 1 within the range (rangeMin ... rangeMax)
    float valScaled = (value-(float)rangeMin) / (float)(rangeMax-rangeMin);

    unsigned char retR = minColor.red() + (unsigned char)( floor(valScaled * (float)(maxColor.red()-minColor.red()) + 0.5f) );
    unsigned char retG = minColor.green() + (unsigned char)( floor(valScaled * (float)(maxColor.green()-minColor.green()) + 0.5f) );
    unsigned char retB = minColor.blue() + (unsigned char)( floor(valScaled * (float)(maxColor.blue()-minColor.blue()) + 0.5f) );
    unsigned char retA = minColor.alpha() + (unsigned char)( floor(valScaled * (float)(maxColor.alpha()-minColor.alpha()) + 0.5f) );

    return QColor(retR, retG, retB, retA);
  }
  else
  {
    float span = (float)(rangeMax-rangeMin),
      x = (value - (float)rangeMin) / span,
      r = 1,
      g = 1,
      b = 1,
      a = 1;
    float F,N,K;
    int I;

    if (type == "jet")
    {
      if ((x >= 3.0/8.0) && (x < 5.0/8.0))
        r = (4.0f * x - 3.0f/2.0f);
      else if ((x >= 5.0/8.0) && (x < 7.0/8.0))
        r = 1.0;
      else if (x >= 7.0/8.0)
        r = (-4.0f * x + 9.0f/2.0f);
      else
        r = 0.0f;
      if ((x >= 1.0/8.0) && (x < 3.0/8.0))
        g = (4.0f * x - 1.0f/2.0f);
      else if ((x >= 3.0/8.0) && (x < 5.0/8.0))
        g = 1.0;
      else if ((x >= 5.0/8.0) && (x < 7.0/8.0))
        g = (-4.0f * x + 7.0f/2.0f);
      else
        g = 0.0f;
      if (x < 1.0/8.0)
        b = (4.0f * x + 1.0f/2.0f);
      else if ((x >= 1.0/8.0) && (x < 3.0/8.0))
        b = 1.0f;
      else if ((x >= 3.0/8.0) & (x < 5.0/8.0))
        b = (-4.0f * x + 5.0f/2.0f);
      else
        b = 0.0f;
    }
    else if (type == "heat")
    {
      r = 1;
      g = 0;
      b = 0;
      a = x;
    }
    else if (type == "hsv")
    {
      // h = x, s = 1, v = 1
      if (x >= 1.0) x = 0.0;
      x = x * 6.0f;
      I = (int) x;   /* should be in the range 0..5 */
      F = x - I;     /* fractional part */

      N = (1.0f - 1.0f * F);
      K = (1.0f - 1.0f * (1 - F));

      if (I == 0) { r = 1; g = K; b = 0; }
      if (I == 1) { r = N; g = 1.0; b = 0; }
      if (I == 2) { r = 0; g = 1.0; b = K; }
      if (I == 3) { r = 0; g = N; b = 1.0; }
      if (I == 4) { r = K; g = 0; b = 1.0; }
      if (I == 5) { r = 1.0; g = 0; b = N; }
    }
    else if (type == "hot")
    {
      if (x < 2.0/5.0)
        r = (5.0f/2.0f * x);
      else
        r = 1.0f;
      if ((x >= 2.0/5.0) && (x < 4.0/5.0))
        g = (5.0f/2.0f * x - 1);
      else if (x >= 4.0/5.0)
        g = 1.0f;
      else
        g = 0.0f;
      if (x >= 4.0/5.0)
        b = (5.0f*x - 4.0f);
      else
        b = 0.0f;
    }
    else if (type == "cool")
    {
      r = x;
      g = 1.0f - x;
      b = 1.0;
    }
    else if (type == "spring")
    {
      r = 1.0f;
      g = x;
      b = 1.0f - x;
    }
    else if (type == "summer")
    {
      r = x;
      g = 0.5f + x / 2.0f;
      b = 0.4f;
    }
    else if (type == "autumn")
    {
      r = 1.0;
      g = x;
      b = 0.0;
    }
    else if (type == "winter")
    {
      r = 0;
      g = x;
      b = 1.0f - x / 2.0f;
    }
    else if (type == "gray")
    {
      r = x;
      g = x;
      b = x;
    }
    else if (type == "bone")
    {
      if (x < 3.0/4.0)
        r = (7.0f/8.0f * x);
      else if (x >= 3.0/4.0)
        r = (11.0f/8.0f * x - 3.0f/8.0f);
      if (x < 3.0/8.0)
        g = (7.0f/8.0f * x);
      else if ((x >= 3.0/8.0) && (x < 3.0/4.0))
        g = (29.0f/24.0f * x - 1.0f/8.0f);
      else if (x >= 3.0/4.0)
        g = (7.0f/8.0f * x + 1.0f/8.0f);
      if (x < 3.0/8.0)
        b = (29.0f/24.0f * x);
      else if (x >= 3.0/8.0)
        b = (7.0f/8.0f * x + 1.0f/8.0f);
    }
    else if (type == "copper")
    {
      if (x < 4.0/5.0)
        r = (5.0f/4.0f * x);
      else r
        = 1.0f;
      g = 4.0f/5.0f * x;
      b = 1.0f/2.0f * x;
    }
    else if (type == "pink")
    {
      if (x < 3.0/8.0)
        r = (14.0f/9.0f * x);
      else if (x >= 3.0/8.0)
        r = (2.0f/3.0f * x + 1.0f/3.0f);
      if (x < 3.0/8.0)
        g = (2.0f/3.0f * x);
      else if ((x >= 3.0/8.0) && (x < 3.0/4.0))
        g = (14.0f/9.0f * x - 1.0f/3.0f);
      else if (x >= 3.0/4.0)
        g = (2.0f/3.0f * x + 1.0f/3.0f);
      if (x < 3.0/4.0)b = (2.0f/3.0f * x);
      else if (x >= 3.0/4.0)
        b = (2.0f * x - 1.0f);
    }
    else if (type == "lines")
    {
      if (x >= 1.0)
        x = 0.0f;
      x = x * 7.0f;
      I = (int) x;

      if (I == 0) { r = 0.0; g = 0.0; b = 1.0; }
      if (I == 1) { r = 0.0; g = 0.5; b = 0.0; }
      if (I == 2) { r = 1.0; g = 0.0; b = 0.0; }
      if (I == 3) { r = 0.0; g = 0.75; b = 0.75; }
      if (I == 4) { r = 0.75; g = 0.0; b = 0.75; }
      if (I == 5) { r = 0.75; g = 0.75; b = 0.0; }
      if (I == 6) { r = 0.25; g = 0.25; b = 0.25; }
    }
    else if (type == "col3_gblr")
    {
      // 3 colors: green, black, red
      r = (x < 0.5) ? 0.0         : (x - 0.5);
      g = (x < 0.5) ? (1.0 - 2*x) : 0.0;
      b = 0.0;
    }
    else if (type == "col3_gwr")
    {
      // 3 colors: green, white, red
      r = (x < 0.5) ? x*2 : 1.0;
      g = (x < 0.5) ? 1.0 : (1-x)*2;
      b = (x < 0.5) ? x*2 : (1-x)*2;
    }
    else if (type == "col3_bblr")
    {
      // 3 colors: blue,  black, red
      r = (x < 0.5) ? 0.0 : (x-0.5)*2;
      g = 0.0;
      b = (x < 0.5) ? 1.0-2*x : 0.0;
    }
    else if (type == "col3_bblr")
    {
      // 3 colors: blue,  white, red
      r = (x < 0.5) ? x*2 : 1.0;
      g = (x < 0.5) ? x*2 : 1.0;
      b = (x < 0.5) ? 1.0 : (1-x)*2;
    }
    else if (type == "col3_bblg")
    {
      // 3 colors: blue,  black, green
      r = 0.0;
      g = (x < 0.5) ? 0.0     : (x-0.5)*2;
      b = (x < 0.5) ? 1.0-2*x : 0.0;
    }
    else if (type == "col3_bblr")
    {
      // 3 colors: blue,  white, green
      r = (x < 0.5) ? x*2 : (1-x)*2;
      g = (x < 0.5) ? x*2 : 1.0;
      b = (x < 0.5) ? 1.0 : (1-x)*2;
    }

    unsigned char retR = (unsigned char)( floor(r * 255.0f + 0.5f) );
    unsigned char retG = (unsigned char)( floor(g * 255.0f + 0.5f) );
    unsigned char retB = (unsigned char)( floor(b * 255.0f + 0.5f) );
    unsigned char retA = (unsigned char)( floor(a * 255.0f + 0.5f) );

    return QColor(retR, retG, retB, retA);
  }
}

// ---------- StatisticsType -----------

StatisticsType::StatisticsType()
{
  typeID = INT_INVALID;
  typeName = "?";
  render = false;
  renderGrid = true;
  alphaFactor = 50;

  vectorSampling = 1;
  scaleToBlockSize = false;
  visualizationType = colorRangeType;
  vectorPen = QPen(QBrush(QColor(Qt::black)),1.0,Qt::SolidLine);
  gridPen = QPen(QBrush(QColor(Qt::black)),0.25,Qt::SolidLine);
  scaleGridToZoom = true;
  mapVectorToColor = false;
  arrowHead = arrow;
}

StatisticsType::StatisticsType(int tID, QString sName, visualizationType_t visType)
{
  typeID = tID;
  typeName = sName;
  render = false;
  renderGrid = true;
  alphaFactor = 50;

  vectorSampling = 1;
  scaleToBlockSize = false;
  visualizationType = visType;

  vectorPen = QPen(QBrush(QColor(Qt::black)),1.0,Qt::SolidLine);
  gridPen = QPen(QBrush(QColor(Qt::black)),0.25,Qt::SolidLine);
  scaleGridToZoom = true;
}

StatisticsType::StatisticsType(int tID, QString sName, QString defaultColorRangeName, int rangeMin, int rangeMax)
{
  typeID = tID;
  typeName = sName;
  render = false;
  renderGrid = true;
  alphaFactor = 50;

  colorRange = ColorRange(defaultColorRangeName, rangeMin, rangeMax);

  vectorSampling = 1;
  scaleToBlockSize = false;
  visualizationType = colorRangeType;

  vectorPen = QPen(QBrush(QColor(Qt::black)),1.0,Qt::SolidLine);
  gridPen = QPen(QBrush(QColor(Qt::black)),0.25,Qt::SolidLine);
  scaleGridToZoom = true;
}

StatisticsType::StatisticsType(int tID, QString sName, visualizationType_t visType, int cRangeMin, QColor cRangeMinColor, int cRangeMax, QColor cRangeMaxColor )
{
  typeID = tID;
  typeName = sName;
  render = false;
  renderGrid = true;
  alphaFactor = 50;

  colorRange = ColorRange(cRangeMin, cRangeMinColor, cRangeMax, cRangeMaxColor);

  vectorSampling = 1;
  scaleToBlockSize = false;
  visualizationType = visType;

  vectorPen = QPen(QBrush(QColor(Qt::black)),1.0,Qt::SolidLine);
  gridPen = QPen(QBrush(QColor(Qt::black)),0.25,Qt::SolidLine);
  scaleGridToZoom = true;
}

void StatisticsType::readFromRow(QStringList row)
{
  if( row.count() >= 5 )
  {
    Q_ASSERT( typeID == row[2].toInt() ); // typeID needs to be set already
    typeName = row[3];
    if (row[4] == "map") visualizationType = colorMapType;
    else if (row[4] == "range") visualizationType = colorRangeType;
    else if (row[4] == "vector") visualizationType = vectorType;
  }
}

// If the internal valueMap can map the value to text, text and value will be returned.
// Otherwise just the value as QString will be returned.
QString StatisticsType::getValueTxt(int val)
{
  if (valMap.contains(val))
  {
    // A text for this value van be shown.
    return QString("%1 (%2)").arg(valMap[val]).arg(val);
  }
  return QString("%1").arg(val);
}