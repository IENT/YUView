/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "statisticsExtensions.h"

#include <cmath>
#include "typedef.h"

// All types that are supported by the getColor() function.
QStringList colorMapper::supportedComplexTypes = QStringList() << "jet" << "heat" << "hsv" << "hot" << "cool" << "spring" << "summer" << "autumn" << "winter" << "gray" << "bone" << "copper" << "pink" << "lines" << "col3_gblr" << "col3_gwr" << "col3_bblr" << "col3_bwr" << "col3_bblg" << "col3_bwg";

// ---------- StatisticsType -----------

StatisticsType::StatisticsType()
{
  typeID = INT_INVALID;
  typeName = "?";

  // Default, do not render, alpha 50%
  render = false;
  alphaFactor = 50;

  // For this constructor, we don't know if there is value or vector data.
  // Set one of these to true if you want to render something.
  hasValueData = false;
  hasVectorData = false;
  hasAffineTFData = false;

  // Default values for drawing value data
  renderValueData = false;
  scaleValueToBlockSize = false;

  // Default values for drawing vectors
  renderVectorData = false;
  renderVectorDataValues = true;
  vectorScale = 1;
  vectorPen = QPen(QBrush(QColor(Qt::black)),1.0,Qt::SolidLine);
  scaleVectorToZoom = false;
  mapVectorToColor = false;
  arrowHead = arrow;

  // Default values for drawing grids
  renderGrid = true;
  scaleGridToZoom = false;
  gridPen = QPen(QBrush(QColor(Qt::black)),0.25,Qt::SolidLine);

  // Default: no polygon shape, use Rect
  isPolygon = false;
}

StatisticsType::StatisticsType(int tID, const QString &sName, int vectorScaling) : StatisticsType()
{
  typeID = tID;
  typeName = sName;

  hasVectorData = true;
  renderVectorData = true;
  vectorScale = vectorScaling;

  setInitialState();
}

// Convenience constructor for a statistics type with block data and a named color map
StatisticsType::StatisticsType(int tID, const QString &sName, const QString &defaultColorRangeName, int rangeMin, int rangeMax, bool hasAndRenderVectorData) : StatisticsType()
{
  typeID = tID;
  typeName = sName;

  // There is value data. Set up a color mapper.
  hasValueData = true;
  renderValueData = true;
  colMapper = colorMapper(defaultColorRangeName, rangeMin, rangeMax);

  hasVectorData = hasAndRenderVectorData;
  renderVectorData = hasAndRenderVectorData;

  setInitialState();
}

// Convenience constructor for a statistics type with block data and a color gradient based color mapping
StatisticsType::StatisticsType(int tID, const QString &sName, int cRangeMin, const QColor &cRangeMinColor, int cRangeMax, const QColor &cRangeMaxColor, bool hasAndRenderVectorData) : StatisticsType()
{
  typeID = tID;
  typeName = sName;

  // There is value data. Set up a color mapper.
  hasValueData = true;
  renderValueData = true;
  colMapper = colorMapper(cRangeMin, cRangeMinColor, cRangeMax, cRangeMaxColor);

  hasVectorData = hasAndRenderVectorData;
  renderVectorData = hasAndRenderVectorData;

  setInitialState();
}

void StatisticsType::setInitialState()
{
  init.render = render;
  init.alphaFactor = alphaFactor;

  init.renderValueData = renderValueData;
  init.scaleValueToBlockSize = scaleValueToBlockSize;
  init.colMapper = colMapper;

  init.renderVectorData = renderVectorData;
  init.scaleVectorToZoom = scaleVectorToZoom;
  init.vectorPen = vectorPen;
  init.vectorScale = vectorScale;
  init.mapVectorToColor = mapVectorToColor;
  init.arrowHead = arrowHead;

  init.renderGrid = renderGrid;
  init.gridPen = gridPen;
  init.scaleGridToZoom = scaleGridToZoom;
}

// Get a string with all values of the QPen
QString convertPenToString(const QPen &pen)
{
  return QString("%1 %2 %3").arg(pen.color().name()).arg(pen.widthF()).arg(pen.style());
}
// The inverse functio to get a QPen from the string
QPen convertStringToPen(const QString &str)
{
  QPen pen;
  QStringList split = str.split(" ");
  if (split.length() == 3)
  {
    pen.setColor(QColor(split[0]));
    pen.setWidthF(split[1].toFloat());
    pen.setStyle(Qt::PenStyle(split[2].toInt()));
  }
  return pen;
}

/* Save all the settings of the statistics type that have changed from the initial state
*/
void StatisticsType::savePlaylist(QDomElementYUView &root) const
{
  bool statChanged = (init.render != render || init.alphaFactor != alphaFactor ||
    init.renderValueData != renderValueData || init.scaleValueToBlockSize != scaleValueToBlockSize || init.colMapper != colMapper ||
    init.renderVectorData != renderVectorData || init.scaleVectorToZoom != scaleVectorToZoom || init.vectorPen != vectorPen ||
    init.vectorScale != vectorScale || init.mapVectorToColor != mapVectorToColor || init.arrowHead != arrowHead ||
    init.renderGrid != renderGrid || init.gridPen != gridPen || init.scaleGridToZoom != scaleGridToZoom);

  if (!statChanged)
    return;

  // Create a new node
  QDomElement newChild = root.ownerDocument().createElement(QString("statType%1").arg(typeID));
  newChild.appendChild(root.ownerDocument().createTextNode(typeName));

  // Append only the parameters that changed
  if (init.render != render)
    newChild.setAttribute("render", render);
  if (init.alphaFactor != alphaFactor)
    newChild.setAttribute("alphaFactor", alphaFactor);
  if (init.renderValueData != renderValueData)
    newChild.setAttribute("renderValueData", renderValueData);
  if (init.scaleValueToBlockSize != scaleValueToBlockSize)
    newChild.setAttribute("scaleValueToBlockSize", scaleValueToBlockSize);
  if (init.colMapper != colMapper)
  {
    if (init.colMapper.type != colMapper.type)
      newChild.setAttribute("colorMapperType", colMapper.type);
    if (colMapper.type == colorMapper::mappingType::gradient)
    {
      if (init.colMapper.minColor != colMapper.minColor)
        newChild.setAttribute("colorMapperMinColor", colMapper.minColor.name());
      if (init.colMapper.maxColor != colMapper.maxColor)
        newChild.setAttribute("colorMapperMaxColor", colMapper.maxColor.name());
    }
    if (colMapper.type == colorMapper::mappingType::gradient || colMapper.type == colorMapper::mappingType::complex)
    {
      if (init.colMapper.rangeMin != colMapper.rangeMin)
        newChild.setAttribute("colorMapperRangeMin", colMapper.rangeMin);
      if (init.colMapper.rangeMax != colMapper.rangeMax)
        newChild.setAttribute("colorMapperRangeMax", colMapper.rangeMax);
    }
    if (colMapper.type == colorMapper::mappingType::map)
    {
      if (init.colMapper.colorMap != colMapper.colorMap)
      {
        // Append the whole color map
        for (auto i = colMapper.colorMap.begin(); i != colMapper.colorMap.end(); ++i)
          newChild.setAttribute(QString("colorMapperMapValue%1").arg(i.key()), i.value().name());
      }
    }
  }

  if (init.renderVectorData != renderVectorData)
    newChild.setAttribute("renderVectorData", renderVectorData);
  if (init.scaleVectorToZoom != scaleVectorToZoom)
    newChild.setAttribute("scaleVectorToZoom", scaleVectorToZoom);
  if (init.vectorPen != vectorPen)
    newChild.setAttribute("vectorPen", convertPenToString(vectorPen));
  if (init.vectorScale != vectorScale)
    newChild.setAttribute("vectorScale", vectorScale);
  if (init.mapVectorToColor != mapVectorToColor)
    newChild.setAttribute("mapVectorToColor", mapVectorToColor);
  if (init.arrowHead != arrowHead)
    newChild.setAttribute("renderarrowHead", arrowHead);
  if (init.renderGrid != renderGrid)
    newChild.setAttribute("renderGrid", renderGrid);
  if (init.gridPen != gridPen)
    newChild.setAttribute("gridPen", convertPenToString(gridPen));
  if (init.scaleGridToZoom != scaleGridToZoom)
    newChild.setAttribute("scaleGridToZoom", scaleGridToZoom);

  root.appendChild(newChild);
}

void StatisticsType::loadPlaylist(const QDomElementYUView &root)
{
  ValuePairList attributes;
  QString statItemName = root.findChildValue(QString("statType%1").arg(typeID), attributes);

  if (statItemName != typeName)
    // The name of this type with the right ID and the name in the playlist don't match?...
    return;

  // Parse and set all the attributes that are in the playlist
  for (int i = 0; i < attributes.length(); i++)
  {
    if (attributes[i].first == "render")
      render = (attributes[i].second != "0");
    else if (attributes[i].first == "alphaFactor")
      alphaFactor = attributes[i].second.toInt();
    else if (attributes[i].first == "renderValueData")
      renderValueData = (attributes[i].second != "0");
    else if (attributes[i].first == "scaleValueToBlockSize")
      scaleValueToBlockSize = (attributes[i].second != "0");
    else if (attributes[i].first == "colorMapperType")
      colMapper.type = colorMapper::mappingType(attributes[i].second.toInt());
    else if (attributes[i].first == "colorMapperMinColor")
      colMapper.minColor = QColor(attributes[i].second);
    else if (attributes[i].first == "colorMapperMaxColor")
      colMapper.maxColor = QColor(attributes[i].second);
    else if (attributes[i].first == "colorMapperRangeMin")
      colMapper.rangeMin = attributes[i].second.toInt();
    else if (attributes[i].first == "colorMapperRangeMax")
      colMapper.rangeMax = attributes[i].second.toInt();
    else if (attributes[i].first.startsWith("colorMapperMapValue"))
    {
      int key = attributes[i].first.mid(19).toInt();
      QColor value = QColor(attributes[i].second);
      colMapper.colorMap.insert(key, value);
    }
    else if (attributes[i].first == "renderVectorData")
      renderVectorData = (attributes[i].second != "0");
    else if (attributes[i].first == "scaleVectorToZoom")
      scaleVectorToZoom = (attributes[i].second != "0");
    else if (attributes[i].first == "vectorPen")
      vectorPen = convertStringToPen(attributes[i].second);
    else if (attributes[i].first == "vectorScale")
      vectorScale = attributes[i].second.toInt();
    else if (attributes[i].first == "mapVectorToColor")
      mapVectorToColor = (attributes[i].second != "0");
    else if (attributes[i].first == "renderarrowHead")
      arrowHead = arrowHead_t(attributes[i].second.toInt());
    else if (attributes[i].first == "renderGrid")
      renderGrid = (attributes[i].second != "0");
    else if (attributes[i].first == "gridPen")
      gridPen = convertStringToPen(attributes[i].second);
    else if (attributes[i].first == "scaleGridToZoom")
      scaleGridToZoom = (attributes[i].second != "0");
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

void statisticsData::addBlockValue(unsigned short x, unsigned short y, unsigned short w, unsigned short h, int val)
{
  statisticsItem_Value value;
  value.pos[0] = x;
  value.pos[1] = y;
  value.size[0] = w;
  value.size[1] = h;
  value.value = val;

  // Always keep the biggest block size updated.
  unsigned int wh = w*h;
  if (wh > maxBlockSize)
    maxBlockSize = wh;

  valueData.append(value);
}

void statisticsData::addBlockVector(unsigned short x, unsigned short y, unsigned short w, unsigned short h, int vecX, int vecY)
{
  statisticsItem_Vector vec;
  vec.pos[0] = x;
  vec.pos[1] = y;
  vec.size[0] = w;
  vec.size[1] = h;
  vec.point[0] = QPoint(vecX,vecY);
  vec.isLine = false;
  vectorData.append(vec);
}

void statisticsData::addBlockAffineTF(unsigned short x, unsigned short y, unsigned short w, unsigned short h, int vecX0, int vecY0, int vecX1, int vecY1, int vecX2, int vecY2)
{
  statisticsItem_AffineTF affineTF;
  affineTF.pos[0] = x;
  affineTF.pos[1] = y;
  affineTF.size[0] = w;
  affineTF.size[1] = h;
  affineTF.point[0] = QPoint(vecX0,vecY0);
  affineTF.point[1] = QPoint(vecX1,vecY1);
  affineTF.point[2] = QPoint(vecX2,vecY2);
  affineTFData.append(affineTF);
}


void statisticsData::addLine(unsigned short x, unsigned short y, unsigned short w, unsigned short h, int x1, int y1, int x2, int y2)
{
  statisticsItem_Vector vec;
  vec.pos[0] = x;
  vec.pos[1] = y;
  vec.size[0] = w;
  vec.size[1] = h;
  vec.point[0] = QPoint(x1,y1);
  vec.point[1] = QPoint(x2,y2);
  vec.isLine = true;
  vectorData.append(vec);
}

void statisticsData::addPolygonValue(const QVector<QPoint> &points, int val)
{
  statisticsItemPolygon_Value value;
  value.corners = QPolygon(points);
  value.value = val;

// todo: how to do this nicely?
//  // Always keep the biggest block size updated.
//  unsigned int wh = w*h;
//  if (wh > maxBlockSize)
//    maxBlockSize = wh;

  polygonValueData.append(value);
}

void statisticsData::addPolygonVector(const QVector<QPoint> &points, int vecX, int vecY)
{
  statisticsItemPolygon_Vector vec;
  vec.corners = QPolygon(points);
  vec.point[0] = QPoint(vecX,vecY);
  polygonVectorData.append(vec);
}

// Setup an invalid (uninitialized color mapper)
colorMapper::colorMapper()
{
  rangeMin = 0;
  rangeMax = 0;
  colorMapOther = Qt::black;
  type = none;
}

// Setup a color mapper with a gradient
colorMapper::colorMapper(int min, const QColor &colMin, int max, const QColor &colMax)
{
  rangeMin = min;
  rangeMax = max;
  minColor = colMin;
  maxColor = colMax;
  colorMapOther = Qt::black;
  type = gradient;
}

colorMapper::colorMapper(const QString &rangeName, int min, int max)
{
  if (supportedComplexTypes.contains(rangeName))
  {
    rangeMin = min;
    rangeMax = max;
    complexType = rangeName;
    type = complex;
  }
  else
  {
    rangeMin = 0;
    rangeMax = 0;
    type = none;
  }
  colorMapOther = Qt::black;
}

QColor colorMapper::getColor(int value)
{
  if (type == map)
  {
    if (colorMap.contains(value))
      return colorMap[value];
    else
      return colorMapOther;
  }
  else
  {
    return getColor(float(value));
  }
}

QColor colorMapper::getColor(float value)
{
  if (type == map)
    // Round and use the integer value to get the value from the map
    return getColor(int(value+0.5));

  // clamp the value to [min max]
  if (value > rangeMax)
    value = rangeMax;
  if (value < rangeMin)
    value = rangeMin;

  if (type == gradient)
  {
    // The value scaled from 0 to 1 within the range (rangeMin ... rangeMax)
    float valScaled = (value-rangeMin) / (rangeMax-rangeMin);

    unsigned char retR = minColor.red() + (unsigned char)(floor(valScaled * (float)(maxColor.red()-minColor.red()) + 0.5f));
    unsigned char retG = minColor.green() + (unsigned char)(floor(valScaled * (float)(maxColor.green()-minColor.green()) + 0.5f));
    unsigned char retB = minColor.blue() + (unsigned char)(floor(valScaled * (float)(maxColor.blue()-minColor.blue()) + 0.5f));
    unsigned char retA = minColor.alpha() + (unsigned char)(floor(valScaled * (float)(maxColor.alpha()-minColor.alpha()) + 0.5f));

    return QColor(retR, retG, retB, retA);
  }
  else if (type == complex)
  {
    float x = (value - rangeMin) / (rangeMax-rangeMin);
    float r = 1, g = 1, b = 1, a = 1;

    if (complexType == "jet")
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
    else if (complexType == "heat")
    {
      r = 1;
      g = 0;
      b = 0;
      a = x;
    }
    else if (complexType == "hsv")
    {
      // h = x, s = 1, v = 1
      if (x >= 1.0)
        x = 0.0;
      x = x * 6.0f;
      int I = (int) x;   /* should be in the range 0..5 */
      float F = x - I;     /* fractional part */

      float N = (1.0f - 1.0f * F);
      float K = (1.0f - 1.0f * (1 - F));

      if (I == 0) { r = 1; g = K; b = 0; }
      if (I == 1) { r = N; g = 1.0; b = 0; }
      if (I == 2) { r = 0; g = 1.0; b = K; }
      if (I == 3) { r = 0; g = N; b = 1.0; }
      if (I == 4) { r = K; g = 0; b = 1.0; }
      if (I == 5) { r = 1.0; g = 0; b = N; }
    }
    else if (complexType == "hot")
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
    else if (complexType == "cool")
    {
      r = x;
      g = 1.0f - x;
      b = 1.0;
    }
    else if (complexType == "spring")
    {
      r = 1.0f;
      g = x;
      b = 1.0f - x;
    }
    else if (complexType == "summer")
    {
      r = x;
      g = 0.5f + x / 2.0f;
      b = 0.4f;
    }
    else if (complexType == "autumn")
    {
      r = 1.0;
      g = x;
      b = 0.0;
    }
    else if (complexType == "winter")
    {
      r = 0;
      g = x;
      b = 1.0f - x / 2.0f;
    }
    else if (complexType == "gray")
    {
      r = x;
      g = x;
      b = x;
    }
    else if (complexType == "bone")
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
    else if (complexType == "copper")
    {
      if (x < 4.0/5.0)
        r = (5.0f/4.0f * x);
      else r
        = 1.0f;
      g = 4.0f/5.0f * x;
      b = 1.0f/2.0f * x;
    }
    else if (complexType == "pink")
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
    else if (complexType == "lines")
    {
      if (x >= 1.0)
        x = 0.0f;
      x = x * 7.0f;
      int I = (int) x;

      if (I == 0) { r = 0.0; g = 0.0; b = 1.0; }
      if (I == 1) { r = 0.0; g = 0.5; b = 0.0; }
      if (I == 2) { r = 1.0; g = 0.0; b = 0.0; }
      if (I == 3) { r = 0.0; g = 0.75; b = 0.75; }
      if (I == 4) { r = 0.75; g = 0.0; b = 0.75; }
      if (I == 5) { r = 0.75; g = 0.75; b = 0.0; }
      if (I == 6) { r = 0.25; g = 0.25; b = 0.25; }
    }
    else if (complexType == "col3_gblr")
    {
      // 3 colors: green, black, red
      r = (x < 0.5) ? 0.0 : (x-0.5) * 2;
      g = (x < 0.5) ? (0.5-x)*2 : 0.0;
      b = 0.0;
    }
    else if (complexType == "col3_gwr")
    {
      // 3 colors: green, white, red
      r = (x < 0.5) ? x*2 : 1.0;
      g = (x < 0.5) ? 1.0 : (1-x)*2;
      b = (x < 0.5) ? x*2 : (1-x)*2;
    }
    else if (complexType == "col3_bblr")
    {
      // 3 colors: blue,  black, red
      r = (x < 0.5) ? 0.0 : (x-0.5)*2;
      g = 0.0;
      b = (x < 0.5) ? 1.0-2*x : 0.0;
    }
    else if (complexType == "col3_bwr")
    {
      // 3 colors: blue,  white, red
      r = (x < 0.5) ? x*2 : 1.0;
      g = (x < 0.5) ? x*2 : (1-x)*2;
      b = (x < 0.5) ? 1.0 : (1-x)*2;
    }
    else if (complexType == "col3_bblg")
    {
      // 3 colors: blue,  black, green
      r = 0.0;
      g = (x < 0.5) ? 0.0 : (x-0.5)*2;
      b = (x < 0.5) ? 1.0-2*x : 0.0;
    }
    else if (complexType == "col3_bwg")
    {
      // 3 colors: blue,  white, green
      r = (x < 0.5) ? x*2 : (1-x)*2;
      g = (x < 0.5) ? x*2 : 1.0;
      b = (x < 0.5) ? 1.0 : (1-x)*2;
    }

    unsigned char retR = (unsigned char)(floor(r * 255.0f + 0.5f));
    unsigned char retG = (unsigned char)(floor(g * 255.0f + 0.5f));
    unsigned char retB = (unsigned char)(floor(b * 255.0f + 0.5f));
    unsigned char retA = (unsigned char)(floor(a * 255.0f + 0.5f));

    return QColor(retR, retG, retB, retA);
  }

  return QColor();
}

int colorMapper::getMinVal()
{
  if (type == gradient || type == complex)
    return rangeMin;
  else if (type == map && !colorMap.empty())
    return colorMap.firstKey();

  return 0;
}

int colorMapper::getMaxVal()
{
  if (type == gradient || type == complex)
    return rangeMax;
  else if (type == map && !colorMap.empty())
    return colorMap.lastKey();

  return 0;
}

int colorMapper::getID()
{
  if (type == gradient)
    return 0;
  else if (type == map)
    return 1;
  else if (type == complex)
    return supportedComplexTypes.indexOf(complexType) + 2;

  // Invalid type
  return -1;
}

bool colorMapper::operator!=(const colorMapper &other) const
{
  if (type != other.type)
    return true;
  if (type == gradient)
    return rangeMin != other.rangeMin || rangeMax != other.rangeMax || minColor != other.minColor || maxColor != other.maxColor;
  if (type == map)
    return colorMap != other.colorMap;
  if (type == complex)
    return rangeMin != other.rangeMin || rangeMax != other.rangeMax || complexType != other.complexType;
  return false;
}
