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
  frameRate = 1;
  sampling = 1;
  controlsCreated = false;
  startEndFrameChanged = false;
}

statisticSource::~statisticSource()
{
}

void statisticSource::paintStatistics(QPainter *painter, StatisticsItemList statsList, StatisticsType statsType, int zoomFactor)
{
  QRect statRect;
  statRect.setSize( statFrameSize * zoomFactor );
  statRect.moveCenter( QPoint(0,0) );
  painter->translate( statRect.topLeft() );

  StatisticsItemList::iterator it;
  for (it = statsList.begin(); it != statsList.end(); ++it)
  {
    StatisticsItem anItem = *it;

    switch (anItem.type)
    {
      case arrowType:
      {
        QRect aRect = anItem.positionRect;
        QRect displayRect = QRect(aRect.left()*zoomFactor, aRect.top()*zoomFactor, aRect.width()*zoomFactor, aRect.height()*zoomFactor);

        int x, y;

        // start vector at center of the block
        x = displayRect.left() + displayRect.width() / 2;
        y = displayRect.top() + displayRect.height() / 2;

        QPoint startPoint = QPoint(x, y);

        float vx = anItem.vector[0];
        float vy = anItem.vector[1];

        QPoint arrowBase = QPoint(x + zoomFactor*vx, y + zoomFactor*vy);
        QColor arrowColor = anItem.color;
        //arrowColor.setAlpha( arrowColor.alpha()*((float)statsType.alphaFactor / 100.0) );

        QPen arrowPen(arrowColor);
        painter->setPen(arrowPen);
        painter->drawLine(startPoint, arrowBase);

        if (vx != 0 || vy != 0)
        {
          // draw an arrow
          float nx, ny;

          // TODO: scale arrow head with
          float a = zoomFactor * 4;    // length of arrow
          float b = zoomFactor * 2;    // base width of arrow

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
          painter->setBrush(arrowColor);
          painter->drawPolygon(points, 3);
        }

        break;
      }
      case blockType:
      {
        //draw a rectangle
        QColor rectColor = anItem.color;
        rectColor.setAlpha(rectColor.alpha()*((float)statsType.alphaFactor / 100.0));
        painter->setBrush(rectColor);

        QRect aRect = anItem.positionRect;
        QRect displayRect = QRect(aRect.left()*zoomFactor, aRect.top()*zoomFactor, aRect.width()*zoomFactor, aRect.height()*zoomFactor);

        painter->fillRect(displayRect, rectColor);

        break;
      }
    }

    // optionally, draw a grid around the region
    if (statsType.renderGrid) 
    {
      //draw a rectangle
      QColor gridColor = anItem.gridColor;
      QPen gridPen(gridColor);
      gridPen.setWidth(1);
      painter->setPen(gridPen);
      painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush));  // no fill color

      QRect aRect = anItem.positionRect;
      QRect displayRect = QRect(aRect.left()*zoomFactor, aRect.top()*zoomFactor, aRect.width()*zoomFactor, aRect.height()*zoomFactor);

      painter->drawRect(displayRect);
    }
  }

  // Perform the inverse translation, so that the painter is in the same state as before
  painter->translate( statRect.topLeft() * -1 );
}

StatisticsItemList statisticSource::getStatistics(int frameIdx, int typeIdx)
{
  //// if requested statistics are not in cache, read from file
  //if (!statsCache.contains(frameIdx) || !statsCache[frameIdx].contains(typeIdx))
  //{
  //  loadStatisticToCache(frameIdx, typeIdx);
  //}

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
  // Absolutely always only do this once
  Q_ASSERT_X(!controlsCreated, "statisticSource::addPropertiesWidget", "The controls must only be created once.");
  
  setupUi( widget );
  widget->setLayout( verticalLayout );
    
  startSpinBox->setValue( startEndFrame.first );
  startSpinBox->setMinimum( startEndFrameLimit.first );
  startSpinBox->setMaximum( startEndFrameLimit.second );

  endSpinBox->setValue( startEndFrame.second );
  endSpinBox->setMinimum( startEndFrame.first );
  endSpinBox->setMaximum( startEndFrameLimit.second );
    
  rateSpinBox->setValue( frameRate );
  rateSpinBox->setMaximum(1000);
  samplingSpinBox->setMinimum(1);
  samplingSpinBox->setMaximum(100000);
  samplingSpinBox->setValue( sampling );

  // connect signals/slots
  connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxChanged()));
  connect(endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxChanged()));
  connect(rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(onSpinBoxChanged()));
  connect(samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxChanged()));
    
  // Add the controls to the gridLayer
  for (int row = 0; row < statsTypeList.length(); ++row) 
  {
    // Append the name (with the checkbox to enable/disable the statistics item)
    QCheckBox *itemNameCheck = new QCheckBox( statsTypeList[row].typeName, scrollAreaWidgetContents);
    gridLayout->addWidget(itemNameCheck, row+2, 0);
    connect(itemNameCheck, SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
    itemNameCheckBoxes.append(itemNameCheck);

    // Append the opactiy slider
    QSlider *opacitySlider = new QSlider( Qt::Horizontal );
    opacitySlider->setMinimum(0);
    opacitySlider->setMaximum(100);
    opacitySlider->setValue(statsTypeList[row].alphaFactor);
    gridLayout->addWidget(opacitySlider, row+2, 1);
    connect(opacitySlider, SIGNAL(valueChanged(int)), this, SLOT(onStatisticsControlChanged()));
    itemOpacitySliders.append(opacitySlider);

    // Append the grid checkbox
    QCheckBox *gridCheckbox = new QCheckBox( "", scrollAreaWidgetContents );
    gridCheckbox->setChecked(true);
    gridLayout->addWidget(gridCheckbox, row+2, 2);
    connect(gridCheckbox, SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
    itemGridCheckBoxes.append(gridCheckbox);
  }

  // Add a spacer at the very bottom
  gridLayout->addItem( new QSpacerItem(1,1,QSizePolicy::Expanding), statsTypeList.length()+1, 1 );

  QSpacerItem *verticalSpacer = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
  gridLayout->addItem(verticalSpacer, statsTypeList.length()+2, 0, 1, 1);

  controlsCreated = true;
  
  // Update all controls
  onStatisticsControlChanged();
}

void statisticSource::onStatisticsControlChanged()
{
  for (int row = 0; row < statsTypeList.length(); ++row)
  {
    // Get the values of the statistics type from the controls
    statsTypeList[row].render      = itemNameCheckBoxes[row]->isChecked();
    statsTypeList[row].alphaFactor = itemOpacitySliders[row]->value();
    statsTypeList[row].renderGrid  = itemGridCheckBoxes[row]->isChecked();

    // Enable/disable the slider and grid checkbox depending on the item name check box
    bool enable = itemNameCheckBoxes[row]->isChecked();
    itemOpacitySliders[row]->setEnabled( enable );
    itemGridCheckBoxes[row]->setEnabled( enable );
  }

  emit updateItem(true);
}

void statisticSource::onSpinBoxChanged()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();
  if (sender == startSpinBox || sender == endSpinBox)
    startEndFrameChanged = true;
  
  startEndFrame.first  = startSpinBox->value();
  startEndFrame.second = endSpinBox->value();
  frameRate = rateSpinBox->value();
  sampling  = samplingSpinBox->value();

  emit updateItem(false);
}

void statisticSource::updateStartEndFrameLimit( indexRange limit ) 
{
  startEndFrameLimit = limit;
  if (startEndFrameChanged)
  {
    // Only update the startEndFrame if the new limits are smaller than the current startEndFrame values
    if (limit.first > startEndFrame.first)
      startEndFrame.first = limit.first;
    if (limit.second < startEndFrame.second)
      startEndFrame.second = limit.second;
  }
  else
    startEndFrame = limit;
  
  if (controlsCreated)
  {
    // Update the limits and values of the spin boxes. For this we only emit one value change event
    // and not up to four.

    disconnect(startSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    disconnect(endSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    disconnect(rateSpinBox, SIGNAL(valueChanged(double)), NULL, NULL);
    disconnect(samplingSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);

    startSpinBox->setValue( startEndFrame.first );
    startSpinBox->setMinimum( startEndFrameLimit.first );
    startSpinBox->setMaximum( startEndFrameLimit.second );

    endSpinBox->setValue( startEndFrame.second );
    endSpinBox->setMinimum( startEndFrame.first );
    endSpinBox->setMaximum( startEndFrameLimit.second );
  
    connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxChanged()));
    connect(endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxChanged()));
    connect(rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(onSpinBoxChanged()));
    connect(samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(onSpinBoxChanged()));

    emit onSpinBoxChanged();
  }
};