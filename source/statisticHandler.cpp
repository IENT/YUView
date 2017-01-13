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

#include "statisticHandler.h"

#include <cmath>
#include <QPainter>
#include <QtMath>
#include "signalsSlots.h"

// Activate this if you want to know when what is loaded.
#define STATISTICS_DEBUG_LOADING 0
#if STATISTICS_DEBUG_LOADING && !NDEBUG
#define DEBUG_STAT qDebug
#else
#define DEBUG_STAT(fmt,...) ((void)0)
#endif

statisticHandler::statisticHandler()
{
  lastFrameIdx = -1;
  statsCacheFrameIdx = -1;

  spacerItems[0] = nullptr;
  spacerItems[1] = nullptr;
  connect(&statisticsStyleUI, &StatisticsStyleControl::StyleChanged, this, &statisticHandler::updateStatisticItem, Qt::QueuedConnection);
}

itemLoadingState statisticHandler::needsLoading(int frameIdx)
{
  if (frameIdx != statsCacheFrameIdx)
  {
    // New frame, but do we even render any statistics?
    for (StatisticsType t : statsTypeList)
      if(t.render)
        return LoadingNeeded;
  }

  QMutexLocker lock(&statsCacheAccessMutex);
  // Check all the statistics. Do some need loading?
  for (int i = statsTypeList.count() - 1; i >= 0; i--)
  {
    // If the statistics for this frame index were not loaded yet but will be rendered, load them now.
    int typeIdx = statsTypeList[i].typeID;
    if (statsTypeList[i].render)
    {
      if (!statsCache.contains(typeIdx))
        // Return that loading is needed before we can render the statitics.
        return LoadingNeeded;
    }
  }

  // Everything needed for drawing is loaded
  return LoadingNotNeeded;
}

void statisticHandler::loadStatistics(int frameIdx)
{
  DEBUG_STAT("statisticHandler::loadStatistics frame %d", frameIdx);

  QMutexLocker lock(&statsCacheAccessMutex);
  if (frameIdx != statsCacheFrameIdx)
  {
    // New frame to draw. Clear the cache.
    statsCache.clear();
    statsCacheFrameIdx = frameIdx;
  }

  // Request all the data for the statistics (that were not already loaded to the local cache)
  int statTypeRenderCount = 0;
  for (int i = statsTypeList.count() - 1; i >= 0; i--)
  {
    // If the statistics for this frame index were not loaded yet but will be rendered, load them now.
    int typeIdx = statsTypeList[i].typeID;
    if (statsTypeList[i].render)
    {
      statTypeRenderCount++;
      if (!statsCache.contains(typeIdx))
        // Load the statistics
        emit requestStatisticsLoading(frameIdx, typeIdx);
    }
  }
}

void statisticHandler::paintStatistics(QPainter *painter, int frameIdx, double zoomFactor)
{
  // Save the state of the painter. This is restored when the function is done.
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing,true);

  QRect statRect;
  statRect.setSize(statFrameSize * zoomFactor);
  statRect.moveCenter(QPoint(0,0));

  // Get the visible coordinates of the statistics
  QRect viewport = painter->viewport();
  QTransform worldTransform = painter->worldTransform();
  int xMin = statRect.width() / 2 - worldTransform.dx();
  int yMin = statRect.height() / 2 - worldTransform.dy();
  int xMax = statRect.width() / 2 - (worldTransform.dx() - viewport.width());
  int yMax = statRect.height() / 2 - (worldTransform.dy() - viewport.height());

  painter->translate(statRect.topLeft());

  // First, get if more than one statistic that has block values is rendered.
  bool moreThanOneBlockStatRendered = false;
  bool oneBlockStatRendered = false;
  for (StatisticsType t : statsTypeList)
  {
    if(t.render && t.hasValueData)
    {
      if (oneBlockStatRendered)
      {
        moreThanOneBlockStatRendered = true;
        break;
      }
      else
        oneBlockStatRendered = true;
    }
  }

  // Lock the statsCache mutex so that nothing is changed while we draw the data
  QMutexLocker lock(&statsCacheAccessMutex);

  // Draw all the block types. Also, if the zoom factor is larger than STATISTICS_DRAW_VALUES_ZOOM,
  // also save a list of all the values of the blocks and their position in order to draw the values in the next step.
  QList<QPoint> drawStatPoints;       // The positions of each value
  QList<QStringList> drawStatTexts;   // For each point: The values to draw
  double maxLineWidth = 0.0;          // Also get the maximum width of the lines that is drawn. This will be used as an offset.
  for (int i = statsTypeList.count() - 1; i >= 0; i--)
  {
    int typeIdx = statsTypeList[i].typeID;
    if (!statsTypeList[i].render || !statsCache.contains(typeIdx))
      // This statistics type is not rendered or could not be loaded.
      continue;

    // Go through all the value data
    for (const statisticsItem_Value &valueItem : statsCache[typeIdx].valueData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      QRect rect = QRect(valueItem.pos[0], valueItem.pos[1], valueItem.size[0], valueItem.size[1]);
      QRect displayRect = QRect(rect.left()*zoomFactor, rect.top()*zoomFactor, rect.width()*zoomFactor, rect.height()*zoomFactor);
      // Check if the rectangle of the statistics item is even visible
      bool rectVisible = (!(displayRect.left() > xMax || displayRect.right() < xMin || displayRect.top() > yMax || displayRect.bottom() < yMin));

      if (rectVisible)
      {
        int value = valueItem.value; // This value determines the color for this item
        if (statsTypeList[i].renderValueData)
        {
          // Get the right color for the item and draw it.
          QColor rectColor;
          if (statsTypeList[i].scaleValueToBlockSize)
            rectColor = statsTypeList[i].colMapper.getColor(float(value) / (valueItem.size[0] * valueItem.size[1]));
          else
            rectColor = statsTypeList[i].colMapper.getColor(value);
          rectColor.setAlpha(rectColor.alpha()*((float)statsTypeList[i].alphaFactor / 100.0));
          painter->setBrush(rectColor);
          painter->fillRect(displayRect, rectColor);
        }

        // optionally, draw a grid around the region
        if (statsTypeList[i].renderGrid)
        {
          // Set the grid color (no fill)
          QPen gridPen = statsTypeList[i].gridPen;
          if (statsTypeList[i].scaleGridToZoom)
            gridPen.setWidthF(gridPen.widthF() * zoomFactor);
          painter->setPen(gridPen);
          painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush));  // no fill color

          // Save the line width (if thicker)
          if (gridPen.widthF() > maxLineWidth)
            maxLineWidth = gridPen.widthF();

          painter->drawRect(displayRect);
        }

        // Save the position/text in order to draw the values later
        if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
        {
          QString valTxt  = statsTypeList[i].getValueTxt(value);
          if (!statsTypeList[i].valMap.contains(value) && statsTypeList[i].scaleValueToBlockSize)
            valTxt = QString("%1").arg(float(value) / (valueItem.size[0] * valueItem.size[1]));

          QString typeTxt = statsTypeList[i].typeName;
          QString statTxt = moreThanOneBlockStatRendered ? typeTxt + ":" + valTxt : valTxt;

          int i = drawStatPoints.indexOf(displayRect.topLeft());
          if (i == -1)
          {
            // No value for this point yet. Append it and start a new QStringList
            drawStatPoints.append(displayRect.topLeft());
            drawStatTexts.append(QStringList(statTxt));
          }
          else
            // There is already a value for this point. Just append the text.
            drawStatTexts[i].append(statTxt);
        }
      }
    }
  }

  // Step three: Draw the values of the block types
  if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
  {
    // For every point, draw only one block of values. So for every point, we check if there are also other
    // text entries for the same point and then we draw all of them
    QPoint lineOffset =  QPoint(int(maxLineWidth/2), int(maxLineWidth/2));
    for (int i = 0; i < drawStatPoints.count(); i++)
    {
      QString txt = drawStatTexts[i].join("\n");
      QRect textRect = painter->boundingRect(QRect(), Qt::AlignLeft, txt);
      textRect.moveTopLeft(drawStatPoints[i] + QPoint(3,1) + lineOffset);
      painter->drawText(textRect, Qt::AlignLeft, txt);
    }
  }

  // Step four: Draw all the arrows
  for (int i = statsTypeList.count() - 1; i >= 0; i--)
  {
    int typeIdx = statsTypeList[i].typeID;
    if (!statsTypeList[i].render || !statsCache.contains(typeIdx))
      // This statistics type is not rendered or could not be loaded.
      continue;

    // Go through all the vector data
    for (const statisticsItem_Vector &vectorItem : statsCache[typeIdx].vectorData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      QRect rect = QRect(vectorItem.pos[0], vectorItem.pos[1], vectorItem.size[0], vectorItem.size[1]);
      QRect displayRect = QRect(rect.left()*zoomFactor, rect.top()*zoomFactor, rect.width()*zoomFactor, rect.height()*zoomFactor);
      // Check if the rectangle of the statistics item is even visible
      bool rectVisible = (!(displayRect.left() > xMax || displayRect.right() < xMin || displayRect.top() > yMax || displayRect.bottom() < yMin));

      if (rectVisible)
      {
        if (statsTypeList[i].renderVectorData)
        {
          // start vector at center of the block
          int x1,y1,x2,y2;
          float vx, vy;
          if (vectorItem.isLine)
          {
            x1 = displayRect.left() + zoomFactor*vectorItem.point[0].x();
            y1 = displayRect.top() + zoomFactor*vectorItem.point[0].y();
            x2 = displayRect.left() + zoomFactor*vectorItem.point[1].x();
            y2 = displayRect.top() + zoomFactor*vectorItem.point[1].y();
            vx = (float)(x2-x1) / statsTypeList[i].vectorScale;
            vy = (float)(y2-y1) / statsTypeList[i].vectorScale;
          }
          else
          {
            x1 = displayRect.left() + displayRect.width() / 2;
            y1 = displayRect.top() + displayRect.height() / 2;

            // The length of the vector
            vx = (float)vectorItem.point[0].x() / statsTypeList[i].vectorScale;
            vy = (float)vectorItem.point[0].y() / statsTypeList[i].vectorScale;

            // The end point of the vector
            x2 = x1 + zoomFactor * vx;
            y2 = y1 + zoomFactor * vy;
          }

          // Is the arrow (possibly) visible?
          if (!(x1 < xMin && x2 < xMin) && !(x1 > xMax && x2 > xMax) && !(y1 < yMin && y2 < yMin) && !(y1 > yMax && y2 > yMax))
          {
            // Set the pen for drawing
            QPen vectorPen = statsTypeList[i].vectorPen;
            QColor arrowColor = vectorPen.color();
            if (statsTypeList[i].mapVectorToColor)
              arrowColor.setHsvF(clip((atan2f(vy,vx)+M_PI)/(2*M_PI),0.0,1.0), 1.0,1.0);
            arrowColor.setAlpha( arrowColor.alpha()*((float)statsTypeList[i].alphaFactor / 100.0));
            vectorPen.setColor(arrowColor);
            if (statsTypeList[i].scaleVectorToZoom)
              vectorPen.setWidthF(vectorPen.widthF() * zoomFactor / 8);
            painter->setPen(vectorPen);
            painter->setBrush(arrowColor);

            // Draw the arrow tip, or a circle if the vector is (0,0) if the zoom factor is not 1 or smaller.
            if (zoomFactor > 1)
            {
              // At which angle do we draw the triangle?
              // A vector to the right (1,  0) -> 0°
              // A vector to the top   (0, -1) -> 90°
              const qreal angle = qAtan2(vy, vx);

              // Draw the vector head if the vector is not 0,0
              if ((vx != 0 || vy != 0))
              {
                // The size of the arrow head
                const int headSize = (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && !statsTypeList[i].scaleVectorToZoom) ? 8 : zoomFactor/2;

                if (statsTypeList[i].arrowHead != StatisticsType::arrowHead_t::none)
                {
                  // We draw an arrow head. This means that we will have to draw a shortened line
                  const int shorten = (statsTypeList[i].arrowHead == StatisticsType::arrowHead_t::arrow) ? headSize * 2 : headSize * 0.5;

                  if (sqrt(vx*vx*zoomFactor*zoomFactor + vy*vy*zoomFactor*zoomFactor) > shorten)
                  {
                    // Shorten the line and draw it
                    QLineF vectorLine = QLineF(x1, y1, double(x2) - cos(angle) * shorten, double(y2) - sin(angle) * shorten);
                    painter->drawLine(vectorLine);
                  }
                }
                else
                  // Draw the not shortened line
                  painter->drawLine(x1, y1, x2, y2);

                if (statsTypeList[i].arrowHead == StatisticsType::arrowHead_t::arrow)
                {
                  // Save the painter state, translate to the arrow tip, rotate the painter and draw the normal triangle.
                  painter->save();

                  // Draw the arrow tip with fixed size
                  painter->translate(QPoint(x2, y2));
                  painter->rotate(qRadiansToDegrees(angle));
                  const QPoint points[3] = {QPoint(0,0), QPoint(-headSize*2, -headSize), QPoint(-headSize*2, headSize)};
                  painter->drawPolygon(points, 3);

                  // Restore. Revert translation/rotation of the painter.
                  painter->restore();
                }
                else if (statsTypeList[i].arrowHead == StatisticsType::arrowHead_t::circle)
                  painter->drawEllipse(x2-headSize/2, y2-headSize/2, headSize, headSize);
              }

              if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
              {
                // Also draw the vector value next to the arrow head
                QString txt = QString("x %1\ny %2").arg(vx).arg(vy);
                QRect textRect = painter->boundingRect(QRect(), Qt::AlignLeft, txt);
                textRect.moveCenter(QPoint(x2,y2));
                int a = qRadiansToDegrees(angle);
                if (a < 45 && a > -45)
                  textRect.moveLeft(x2);
                else if (a <= -45 && a > -135)
                  textRect.moveBottom(y2);
                else if (a >= 45 && a < 135)
                  textRect.moveTop(y2);
                else
                  textRect.moveRight(x2);
                painter->drawText(textRect, Qt::AlignLeft, txt);
              }
            }
            else
            {
              // No arrow head is drawn. Only draw a line.
              painter->drawLine(x1, y1, x2, y2);
            }
          }
        }

        // optionally, draw a grid around the region that the arrow is defined for
        if (statsTypeList[i].renderGrid && rectVisible)
        {
          QPen gridPen = statsTypeList[i].gridPen;
          if (statsTypeList[i].scaleGridToZoom)
            gridPen.setWidthF(gridPen.widthF() * zoomFactor);

          painter->setPen(gridPen);
          painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush));  // no fill color

          painter->drawRect(displayRect);
        }
      }
    }
  }

  // Picture updated
  lastFrameIdx = frameIdx;

  // Restore the state the state of the painter from before this function was called.
  // This will reset the set pens and the translation.
  painter->restore();
}

StatisticsType* statisticHandler::getStatisticsType(int typeID)
{
  for (int i = 0; i<statsTypeList.count(); i++)
  {
    if( statsTypeList[i].typeID == typeID )
      return &statsTypeList[i];
  }

  return nullptr;
}

// return raw(!) value of front-most, active statistic item at given position
// Info is always read from the current buffer. So these values are only valid if a draw event occurred first.
ValuePairList statisticHandler::getValuesAt(const QPoint &pos)
{
  ValuePairList valueList;

  for (int i = 0; i<statsTypeList.count(); i++)
  {
    if (statsTypeList[i].render)  // only show active values
    {
      int typeID = statsTypeList[i].typeID;
      if (typeID == INT_INVALID) // no active statistics
        continue;

      const StatisticsType* aType = getStatisticsType(typeID);

      // Get all value data entries
      bool foundStats = false;
      for (const statisticsItem_Value &valueItem : statsCache[typeID].valueData)
      {
        QRect rect = QRect(valueItem.pos[0], valueItem.pos[1], valueItem.size[0], valueItem.size[1]);
        if (rect.contains(pos))
        {
          int value = valueItem.value;
          QString valTxt  = statsTypeList[i].getValueTxt(value);
          if (!statsTypeList[i].valMap.contains(value) && statsTypeList[i].scaleValueToBlockSize)
            valTxt = QString("%1").arg(float(value) / (valueItem.size[0] * valueItem.size[1]));
          valueList.append(ValuePair(aType->typeName, valTxt));
          foundStats = true;
        }
      }

      for (const statisticsItem_Vector &vectorItem : statsCache[typeID].vectorData)
      {
        QRect rect = QRect(vectorItem.pos[0], vectorItem.pos[1], vectorItem.size[0], vectorItem.size[1]);
        if (rect.contains(pos))
        {
          float vectorValue1, vectorValue2;
          if (vectorItem.isLine)
          {
           vectorValue1 = (float)(vectorItem.point[1].x() - vectorItem.point[0].x()) / statsTypeList[i].vectorScale;
           vectorValue2 = (float)(vectorItem.point[1].y() - vectorItem.point[0].y()) / statsTypeList[i].vectorScale;
          }
          else
          {
            vectorValue1 = (float)vectorItem.point[0].x() / statsTypeList[i].vectorScale;
            vectorValue2 = (float)vectorItem.point[0].y() / statsTypeList[i].vectorScale;
          }
          valueList.append(ValuePair(QString("%1[x]").arg(aType->typeName), QString::number(vectorValue1)));
          valueList.append(ValuePair(QString("%1[y]").arg(aType->typeName), QString::number(vectorValue2)));
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
bool statisticHandler::setStatisticsTypeList(const StatisticsTypeList &typeList)
{
  bool bChanged = false;
  for (const StatisticsType &aType : typeList)
  {
    StatisticsType* internalType = getStatisticsType(aType.typeID);

    if (internalType->typeName != aType.typeName)
      continue;

    if (internalType->render != aType.render)
    {
      internalType->render = aType.render;
      bChanged = true;
    }
    if (internalType->renderValueData != aType.renderValueData)
    {
      internalType->renderValueData = aType.renderValueData;
      bChanged = true;
    }
    if (internalType->renderVectorData != aType.renderVectorData)
    {
      internalType->renderVectorData = aType.renderVectorData;
      bChanged = true;
    }
    if (internalType->renderGrid != aType.renderGrid)
    {
      internalType->renderGrid = aType.renderGrid;
      bChanged = true;
    }
    if (internalType->alphaFactor != aType.alphaFactor)
    {
      internalType->alphaFactor = aType.alphaFactor;
      bChanged = true;
    }
  }

  return bChanged;
}

QLayout *statisticHandler::createStatisticsHandlerControls(bool recreateControlsOnly)
{
  if (!recreateControlsOnly)
  {
    // Absolutely always only do this once
    Q_ASSERT_X(!ui.created(), "statisticHandler::createStatisticsHandlerControls", "The primary statistics controls must only be created once.");

    ui.setupUi();
  }

  // Add the controls to the gridLayer
  for (int row = 0; row < statsTypeList.length(); ++row)
  {
    // Append the name (with the check box to enable/disable the statistics item)
    QCheckBox *itemNameCheck = new QCheckBox( statsTypeList[row].typeName, ui.scrollAreaWidgetContents);
    itemNameCheck->setChecked( statsTypeList[row].render );
    ui.gridLayout->addWidget(itemNameCheck, row+2, 0);
    connect(itemNameCheck, &QCheckBox::stateChanged, this, &statisticHandler::onStatisticsControlChanged);
    itemNameCheckBoxes[0].append(itemNameCheck);

    // Append the opacity slider
    QSlider *opacitySlider = new QSlider( Qt::Horizontal );
    opacitySlider->setMinimum(0);
    opacitySlider->setMaximum(100);
    opacitySlider->setValue(statsTypeList[row].alphaFactor);
    ui.gridLayout->addWidget(opacitySlider, row+2, 1);
    connect(opacitySlider, &QSlider::valueChanged, this, &statisticHandler::onStatisticsControlChanged);
    itemOpacitySliders[0].append(opacitySlider);

    // Append the change style buttons
    QPushButton *pushButton = new QPushButton(QIcon(":img_edit.png"), QString(), ui.scrollAreaWidgetContents);
    ui.gridLayout->addWidget(pushButton,row+2,2);
    connect(pushButton, &QPushButton::released, this, [=]{ onStyleButtonClicked(row); });
    itemStyleButtons[0].append(pushButton);
  }

  // Add a spacer at the very bottom
  QSpacerItem *verticalSpacer = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
  ui.gridLayout->addItem(verticalSpacer, statsTypeList.length()+2, 0, 1, 1);
  spacerItems[0] = verticalSpacer;

  // Update all controls
  onStatisticsControlChanged();

  return ui.verticalLayout;
}

QWidget *statisticHandler::getSecondaryStatisticsHandlerControls(bool recreateControlsOnly)
{
  if (!ui2.created() || recreateControlsOnly)
  {
    if (!ui2.created())
    {
      secondaryControlsWidget = new QWidget;
      ui2.setupUi();
      secondaryControlsWidget->setLayout(ui2.verticalLayout);
    }

    // Add the controls to the gridLayer
    for (int row = 0; row < statsTypeList.length(); ++row)
    {
      // Append the name (with the check box to enable/disable the statistics item)
      QCheckBox *itemNameCheck = new QCheckBox( statsTypeList[row].typeName, ui2.scrollAreaWidgetContents);
      itemNameCheck->setChecked( statsTypeList[row].render );
      ui2.gridLayout->addWidget(itemNameCheck, row+2, 0);
      connect(itemNameCheck, &QCheckBox::stateChanged, this, &statisticHandler::onSecondaryStatisticsControlChanged);
      itemNameCheckBoxes[1].append(itemNameCheck);

      // Append the opacity slider
      QSlider *opacitySlider = new QSlider( Qt::Horizontal );
      opacitySlider->setMinimum(0);
      opacitySlider->setMaximum(100);
      opacitySlider->setValue(statsTypeList[row].alphaFactor);
      ui2.gridLayout->addWidget(opacitySlider, row+2, 1);
      connect(opacitySlider, &QSlider::valueChanged, this, &statisticHandler::onSecondaryStatisticsControlChanged);
      itemOpacitySliders[1].append(opacitySlider);

      // Append the change style buttons
      QPushButton *pushButton = new QPushButton(QIcon(":img_edit.png"), QString(), ui2.scrollAreaWidgetContents);
      ui2.gridLayout->addWidget(pushButton,row+2,2);
      connect(pushButton, &QPushButton::released, this, [=]{ onStyleButtonClicked(row); });
      itemStyleButtons[1].append(pushButton);
    }

    // Add a spacer at the very bottom
    // TODO FIXME Should we always add the spacer or only when
    // the controls were created?
    if (true || ui2.created()) {
      QSpacerItem *verticalSpacer = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
      ui2.gridLayout->addItem(verticalSpacer, statsTypeList.length()+2, 0, 1, 1);
      spacerItems[1] = verticalSpacer;
    }

    // Update all controls
    onSecondaryStatisticsControlChanged();
  }

  return secondaryControlsWidget;
}

// One of the primary controls changed. Update the secondary controls (if they were created) without emitting
// further signals and of course update the statsTypeList to render the stats correctly.
void statisticHandler::onStatisticsControlChanged()
{
  for (int row = 0; row < statsTypeList.length(); ++row)
  {
    // Get the values of the statistics type from the controls
    statsTypeList[row].render      = itemNameCheckBoxes[0][row]->isChecked();
    statsTypeList[row].alphaFactor = itemOpacitySliders[0][row]->value();

    // Enable/disable the slider and grid check box depending on the item name check box
    bool enable = itemNameCheckBoxes[0][row]->isChecked();
    itemOpacitySliders[0][row]->setEnabled( enable );

    // Update the secondary controls if they were created
    if (ui2.created() && itemNameCheckBoxes[1].length() > 0)
    {
      // Update the controls that changed
      if (itemNameCheckBoxes[0][row]->isChecked() != itemNameCheckBoxes[1][row]->isChecked())
      {
        const QSignalBlocker blocker(itemNameCheckBoxes[1][row]);
        itemNameCheckBoxes[1][row]->setChecked( itemNameCheckBoxes[0][row]->isChecked() );
      }

      if (itemOpacitySliders[0][row]->value() != itemOpacitySliders[1][row]->value())
      {
        const QSignalBlocker blocker(itemOpacitySliders[1][row]);
        itemOpacitySliders[1][row]->setValue( itemOpacitySliders[0][row]->value() );
      }
    }
  }

  emit updateItem(true);
}

// One of the secondary controls changed. Perform the inverse thing to onStatisticsControlChanged(). Update the primary
// controls without emitting further signals and of course update the statsTypeList to render the stats correctly.
void statisticHandler::onSecondaryStatisticsControlChanged()
{
  for (int row = 0; row < statsTypeList.length(); ++row)
  {
    // Get the values of the statistics type from the controls
    statsTypeList[row].render      = itemNameCheckBoxes[1][row]->isChecked();
    statsTypeList[row].alphaFactor = itemOpacitySliders[1][row]->value();

    // Enable/disable the slider and grid check box depending on the item name check box
    bool enable = itemNameCheckBoxes[1][row]->isChecked();
    itemOpacitySliders[1][row]->setEnabled( enable );

    // Update the primary controls that changed
    if (itemNameCheckBoxes[0][row]->isChecked() != itemNameCheckBoxes[1][row]->isChecked())
    {
      const QSignalBlocker blocker(itemNameCheckBoxes[0][row]);
      itemNameCheckBoxes[0][row]->setChecked( itemNameCheckBoxes[1][row]->isChecked() );
    }

    if (itemOpacitySliders[0][row]->value() != itemOpacitySliders[1][row]->value())
    {
      const QSignalBlocker blocker(itemOpacitySliders[0][row]);
      itemOpacitySliders[0][row]->setValue( itemOpacitySliders[1][row]->value() );
    }
  }

  emit updateItem(true);
}

void statisticHandler::deleteSecondaryStatisticsHandlerControls()
{
  secondaryControlsWidget->deleteLater();
  ui2.clear();
  itemNameCheckBoxes[1].clear();
  itemOpacitySliders[1].clear();
  itemStyleButtons[1].clear();
}

void statisticHandler::savePlaylist(QDomElementYUView &root) const
{
  for (int row = 0; row < statsTypeList.length(); ++row)
    statsTypeList[row].savePlaylist(root);
}

void statisticHandler::loadPlaylist(const QDomElementYUView &root)
{
  for (int row = 0; row < statsTypeList.length(); ++row)
    statsTypeList[row].loadPlaylist(root);
}

void statisticHandler::updateStatisticsHandlerControls()
{
  // First run a check if all statisticsTypes are identical
  bool controlsStillValid = true;
  if (statsTypeList.length() != itemNameCheckBoxes[0].count())
    // There are more or less statistics types as before
    controlsStillValid = false;
  else
  {
    for (int row = 0; row < statsTypeList.length(); ++row)
    {
      if (itemNameCheckBoxes[0][row]->text() != statsTypeList[row].typeName)
      {
        // One of the statistics types changed it's name or the order of statistics types changed.
        // Either way, we will create new controls.
        controlsStillValid = false;
        break;
      }
    }
  }

  if (controlsStillValid)
  {
    // Update the controls from the current settings in statsTypeList
    onStatisticsControlChanged();
    if (ui2.created())
      onSecondaryStatisticsControlChanged();
  }
  else
  {
    // Delete all old controls
    for (int i = 0; i < itemNameCheckBoxes[0].length(); i++)
    {
      Q_ASSERT(itemNameCheckBoxes[0].length() == itemOpacitySliders[0].length());
      Q_ASSERT(itemStyleButtons[0].length()   == itemOpacitySliders[0].length());

      // Delete the primary controls
      delete itemNameCheckBoxes[0][i];
      delete itemOpacitySliders[0][i];
      delete itemStyleButtons[0][i];

      if (ui2.created())
      {
        Q_ASSERT(itemNameCheckBoxes[1].length() == itemOpacitySliders[1].length());
        Q_ASSERT(itemStyleButtons[1].length()   == itemOpacitySliders[1].length());


        // Delete the secondary controls
        delete itemNameCheckBoxes[1][i];
        delete itemOpacitySliders[1][i];
        delete itemStyleButtons[1][i];
      }
    }

    // Delete the spacer items at the bottom.
    assert(spacerItems[0] != nullptr);
    ui.gridLayout->removeItem(spacerItems[0]);
    delete spacerItems[0];
    spacerItems[0] = nullptr;

    // Delete all pointers to the widgets. The layout has the ownership and removing the
    // widget should delete it.
    itemNameCheckBoxes[0].clear();
    itemOpacitySliders[0].clear();
    itemStyleButtons[0].clear();

    if (ui2.created())
    {
      // Delete all pointers to the widgets. The layout has the ownership and removing the
      // widget should delete it.
      itemNameCheckBoxes[1].clear();
      itemOpacitySliders[1].clear();
      itemStyleButtons[1].clear();

      // Delete the spacer items at the bottom.
      assert(spacerItems[1] != nullptr);
      ui2.gridLayout->removeItem(spacerItems[1]);
      delete spacerItems[1];
      spacerItems[1] = nullptr;
    }

    // We have a backup of the old statistics types. Maybe some of the old types (with the same name) are still in the new list.
    // If so, we can update the status of those statistics types (are they drawn, transparency ...).
    for (int i = 0; i < statsTypeListBackup.length(); i++)
    {
      for (int j = 0; j < statsTypeList.length(); j++)
      {
        if (statsTypeListBackup[i].typeName == statsTypeList[j].typeName)
        {
          // In the new list of statistics types we found one that has the same name as this one.
          // This is enough indication. Apply the old settings to this new type.
          statsTypeList[j].render           = statsTypeListBackup[i].render;
          statsTypeList[j].renderValueData  = statsTypeListBackup[i].renderValueData;
          statsTypeList[j].renderVectorData = statsTypeListBackup[i].renderVectorData;
          statsTypeList[j].renderGrid       = statsTypeListBackup[i].renderGrid;
          statsTypeList[j].alphaFactor      = statsTypeListBackup[i].alphaFactor;
        }
      }
    }

    // Create new controls
    createStatisticsHandlerControls(true);
    if (ui2.created())
      getSecondaryStatisticsHandlerControls(true);
  }
}

void statisticHandler::clearStatTypes()
{
  // Create a backup of the types list. This backup is used if updateStatisticsHandlerControls is called
  // to revert the new controls. This way we can see which statistics were drawn / how.
  statsTypeListBackup = statsTypeList;

  // Clear the old list. New items can be added now.
  statsTypeList.clear();
}

void statisticHandler::onStyleButtonClicked(int id)
{
  statisticsStyleUI.setStatsItem(&statsTypeList[id]);
  statisticsStyleUI.show();
}
