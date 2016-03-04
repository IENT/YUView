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

#include "statisticSource.h"

#include <QPainter>
#include <QDebug>
#include <QMouseEvent>

#if _WIN32 && !__MINGW32__
#define _USE_MATH_DEFINES
#include "math.h"
#else
#include <cmath>
#endif

void rotateVector(float angle, float vx, float vy, float &nx, float &ny)
{
  float s = sinf(angle);
  float c = cosf(angle);

  nx = c * vx + s * vy;
  ny = -s * vx + c * vy;

  //normalize vector
  float n_abs = sqrtf(nx*nx + ny*ny);
  nx /= n_abs;
  ny /= n_abs;
}

statisticSource::statisticSource()
{
  lastFrameIdx = -1;
}

statisticSource::~statisticSource()
{
}

void statisticSource::drawStatistics(QPixmap *img, int frameIdx)
{
  // draw statistics (inverse order)
  for (int i = statsTypeList.count() - 1; i >= 0; i--)
  {
    if (!statsTypeList[i].render)
      continue;

    StatisticsItemList stat = getStatistics(frameIdx, statsTypeList[i].typeID);
    drawStatisticsImage(img, stat, statsTypeList[i]);
  }

  lastFrameIdx = frameIdx;
}

void statisticSource::drawStatisticsImage(QPixmap *img, StatisticsItemList statsList, StatisticsType statsType)
{
  QPainter painter(img);

  int p_internalScaleFactor = 1;

  StatisticsItemList::iterator it;
  for (it = statsList.begin(); it != statsList.end(); ++it)
  {
    StatisticsItem anItem = *it;

    switch (anItem.type)
    {
    case arrowType:
    {
      QRect aRect = anItem.positionRect;
      QRect displayRect = QRect(aRect.left()*p_internalScaleFactor, aRect.top()*p_internalScaleFactor, aRect.width()*p_internalScaleFactor, aRect.height()*p_internalScaleFactor);

      int x, y;

      // start vector at center of the block
      x = displayRect.left() + displayRect.width() / 2;
      y = displayRect.top() + displayRect.height() / 2;

      QPoint startPoint = QPoint(x, y);

      float vx = anItem.vector[0];
      float vy = anItem.vector[1];

      QPoint arrowBase = QPoint(x + p_internalScaleFactor*vx, y + p_internalScaleFactor*vy);
      QColor arrowColor = anItem.color;
      //arrowColor.setAlpha( arrowColor.alpha()*((float)statsType.alphaFactor / 100.0) );

      QPen arrowPen(arrowColor);
      painter.setPen(arrowPen);
      painter.drawLine(startPoint, arrowBase);

      if (vx == 0 && vy == 0)
      {
        // nothing to draw...
      }
      else
      {
        // draw an arrow
        float nx, ny;

        // TODO: scale arrow head with
        float a = p_internalScaleFactor * 4;    // length of arrow
        float b = p_internalScaleFactor * 2;    // base width of arrow

        float n_abs = sqrtf(vx*vx + vy*vy);
        float vxf = (float)vx / n_abs;
        float vyf = (float)vy / n_abs;

        QPoint arrowTip = arrowBase + QPoint(vxf*a + 0.5, vyf*a + 0.5);

        // arrow head right
        rotateVector((float)-M_PI_2, -vx, -vy, nx, ny);
        QPoint offsetRight = QPoint(nx*b + 0.5, ny*b + 0.5);
        QPoint arrowHeadRight = arrowBase + offsetRight;

        // arrow head left
        rotateVector((float)M_PI_2, -vx, -vy, nx, ny);
        QPoint offsetLeft = QPoint(nx*b + 0.5, ny*b + 0.5);
        QPoint arrowHeadLeft = arrowBase + offsetLeft;

        // draw arrow head
        QPoint points[3] = { arrowTip, arrowHeadRight, arrowHeadLeft };
        painter.setBrush(arrowColor);
        painter.drawPolygon(points, 3);
      }

      break;
    }
    case blockType:
    {
      //draw a rectangle
      QColor rectColor = anItem.color;
      rectColor.setAlpha(rectColor.alpha()*((float)statsType.alphaFactor / 100.0));
      painter.setBrush(rectColor);

      QRect aRect = anItem.positionRect;
      QRect displayRect = QRect(aRect.left()*p_internalScaleFactor, aRect.top()*p_internalScaleFactor, aRect.width()*p_internalScaleFactor, aRect.height()*p_internalScaleFactor);

      painter.fillRect(displayRect, rectColor);

      break;
    }
    }

    // optionally, draw a grid around the region
    if (statsType.renderGrid) {
      //draw a rectangle
      QColor gridColor = anItem.gridColor;
      QPen gridPen(gridColor);
      gridPen.setWidth(1);
      painter.setPen(gridPen);
      painter.setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush));  // no fill color

      QRect aRect = anItem.positionRect;
      QRect displayRect = QRect(aRect.left()*p_internalScaleFactor, aRect.top()*p_internalScaleFactor, aRect.width()*p_internalScaleFactor, aRect.height()*p_internalScaleFactor);

      painter.drawRect(displayRect);
    }
  }
}

StatisticsItemList statisticSource::getStatistics(int frameIdx, int typeIdx)
{
  // if requested statistics are not in cache, read from file
  if (!statsCache.contains(frameIdx) || !statsCache[frameIdx].contains(typeIdx))
  {
    loadStatisticToCache(frameIdx, typeIdx);
  }

  return statsCache[frameIdx][typeIdx];
}

StatisticsType* statisticSource::getStatisticsType(int typeID)
{
  for (int i = 0; i<statsTypeList.count(); i++)
    {
        if( statsTypeList[i].typeID == typeID )
            return &statsTypeList[i];
    }

    return NULL;
}

// return raw(!) value of frontmost, active statistic item at given position
ValuePairList statisticSource::getValuesAt(int x, int y)
{
  ValuePairList valueList;

  for (int i = 0; i<statsTypeList.count(); i++)
  {
    if (statsTypeList[i].render)  // only show active values
    {
      int typeID = statsTypeList[i].typeID;
      StatisticsItemList statsList = getStatistics(lastFrameIdx, typeID);

      if (statsList.size() == 0 && typeID == INT_INVALID) // no active statistics
        continue;

      StatisticsType* aType = getStatisticsType(typeID);
      Q_ASSERT(aType->typeID != INT_INVALID && aType->typeID == typeID);

      // find item of this type at requested position
      StatisticsItemList::iterator it;
      bool foundStats = false;
      for (it = statsList.begin(); it != statsList.end(); ++it)
      {
        StatisticsItem anItem = *it;
        QRect aRect = anItem.positionRect;
                
        if (aRect.contains(x, y))
        {
          if (anItem.type == blockType)
          {
            QString sValTxt = aType->getValueTxt(anItem.rawValues[0]);
            valueList.append(ValuePair(aType->typeName, sValTxt));
          }
          else if (anItem.type == arrowType)
          {
            // TODO: do we also want to show the raw values?
            float vectorValue1 = anItem.vector[0];
            float vectorValue2 = anItem.vector[1];
            valueList.append(ValuePair(QString("%1[x]").arg(aType->typeName), QString::number(vectorValue1)));
            valueList.append(ValuePair(QString("%1[y]").arg(aType->typeName), QString::number(vectorValue2)));
          }

          foundStats = true;
        }
      }

      if (!foundStats)
        valueList.append(ValuePair(aType->typeName, "-"));
    }
  }

  return valueList;
}

/* Set the statistics Type list.
 * we do not overwrite our statistics type, we just change their parameters
 * return if something has changed where a redraw would be necessary
*/
bool statisticSource::setStatisticsTypeList(StatisticsTypeList typeList)
{
  bool bChanged = false;
  foreach(StatisticsType aType, typeList)
  {
    StatisticsType* internalType = getStatisticsType(aType.typeID);

    if (internalType->typeName != aType.typeName)
      continue;

    if (internalType->render != aType.render) {
      internalType->render = aType.render;
      bChanged = true;
    }
    if (internalType->renderGrid != aType.renderGrid) {
      internalType->renderGrid = aType.renderGrid;
      bChanged = true;
    }

    if (internalType->alphaFactor != aType.alphaFactor) {
      internalType->alphaFactor = aType.alphaFactor;
      bChanged = true;
    }
  }

  return bChanged;
}

/* Check if at least one of the statistics is actually displayed.
*/
bool statisticSource::anyStatisticsRendered()
{
  for (int i = 0; i<statsTypeList.count(); i++)
  {
    if( statsTypeList[i].render )
      return true;
  }
  return false;
}

void statisticSource::addPropertiesWidget(QWidget *widget)
{
  setupUi( widget );
  widget->setLayout( verticalLayout );

  // Set the model to use
  model.setColumnCount(3);
  model.setRowCount( statsTypeList.length() );

  statisticTable->setModel(&model);
  
  for (int row = 0; row < statsTypeList.length(); ++row) 
  {
    // Append name
    QStandardItem *item = new QStandardItem( statsTypeList[row].typeName );
    item->setCheckable(true);
    //item->setEnabled(true);
    model.setItem(row, 0, item);

    // Alpha factor
    item = new QStandardItem( QString("%1").arg(statsTypeList[row].alphaFactor) );
    model.setItem(row, 1, item);

    item = new QStandardItem( "" );
    item->setCheckable(true);
    model.setItem(row, 2, item);
  }

  // For the first columm (opacity), we use a slider
  statisticTable->setItemDelegateForColumn(1, &delegate);

  // Set column lables
  model.setHorizontalHeaderLabels( QStringList({"Name","Opacity","Grid"}) );
  // Hide the vertical header
  statisticTable->verticalHeader()->hide();

  // Set colum sizes
  statisticTable->resizeColumnToContents(0);
  statisticTable->setColumnWidth(1, 100);
  statisticTable->resizeColumnToContents(2);

  // Here we could connect signals/slots ...
}

// ------------------ SliderDelegate
SliderDelegate::SliderDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

void SliderDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if (index.column() == 1)
  {
    // For the values in the first column, draw sliders
    int val = index.data().toInt();
        
    QStyleOptionSlider sliderOption;
    sliderOption.rect = option.rect;
    sliderOption.minimum = 0;
    sliderOption.maximum = 100;
    sliderOption.sliderValue = val;
    sliderOption.sliderPosition = val;
    sliderOption.state = QStyle::State_Active & QStyle::State_Selected & QStyle::State_HasFocus;
  
    QApplication::style()->drawComplexControl(QStyle::CC_Slider, &sliderOption, painter);
  }
  else
    QStyledItemDelegate::paint(painter, option, index);
}

bool SliderDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if (index.column() == 1 && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease))
  {
    // Mouse press/move/elease events are only send to this function if the mouse button was pressed first
    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
    
    // From the position of the mouse, calculate the new position
    int newVal = (int)(((double)mouseEvent->pos().x() - (double)option.rect.x()) / (double)option.rect.width() * 100);

    model->setData(index, QVariant(newVal));
  }
    
  return false;
}