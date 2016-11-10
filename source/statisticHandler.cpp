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

statisticHandler::statisticHandler()
{
  lastFrameIdx = -1;
  statsCacheFrameIdx = -1;
  secondaryControlsWidget = NULL;
  QSettings settings;
  // TODO: Is this ever updated if the user changes the settings? I don't think so.
  mapAllVectorsToColor = settings.value("MapVectorToColor",false).toBool();
  spacerItems[0] = NULL;
  spacerItems[1] = NULL;
}

statisticHandler::~statisticHandler()
{
}

void statisticHandler::paintStatistics(QPainter *painter, int frameIdx, double zoomFactor)
{
  // Save the state of the painter. This is restored when the function is done.
  painter->save();

  QRect statRect;
  statRect.setSize( statFrameSize * zoomFactor );
  statRect.moveCenter( QPoint(0,0) );
  painter->translate( statRect.topLeft() );

  if (frameIdx != statsCacheFrameIdx)
  {
    // New frame to draw. Clear the cache.
    statsCache.clear();
    statsCacheFrameIdx = frameIdx;
  }

  // draw statistics (inverse order)
  for (int i = statsTypeList.count() - 1; i >= 0; i--)
  {
    if (!statsTypeList[i].render)
      continue;

    // If the statistics for this frame index were not loaded yet, do this now.
    int typeIdx = statsTypeList[i].typeID;
    if (!statsCache.contains(typeIdx))
      // Load the statistics
      emit requestStatisticsLoading(frameIdx, typeIdx);
    if (!statsCache.contains(typeIdx))
      // The statistics could not (yet) be loaded. At the next redraw, we will try to load them again.
      continue;

    StatisticsItemList statsList = statsCache[typeIdx];

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
          QColor arrowColor;

          if (mapAllVectorsToColor)
          {
            arrowColor.setHsvF(clip((atan2f(vy,vx)+M_PI)/(2*M_PI),0.0,1.0), 1.0,1.0);
          }
          else
          {
            arrowColor = anItem.color;
          }

          arrowColor.setAlpha( arrowColor.alpha()*((float)statsTypeList[i].alphaFactor / 100.0));

          QPen arrowPen(arrowColor);
          painter->setPen(arrowPen);
          painter->drawLine(startPoint, arrowBase);

          if ((vx != 0 || vy != 0) && statsTypeList[i].showArrow)
          {
            // draw an arrow
            float nx, ny;

            // compress the zoomFactor a bit
            float a = log10(100.0*zoomFactor) * 4;    // length of arrow
            float b = log10(100.0*zoomFactor) * 2;    // base width of arrow

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
          else
          {
            painter->setBrush(arrowColor);
            painter->drawEllipse(arrowBase,2,2);
          }

          break;
        }
        case blockType:
        {
          //draw a rectangle
          QColor rectColor = anItem.color;
          rectColor.setAlpha(rectColor.alpha()*((float)statsTypeList[i].alphaFactor / 100.0));
          painter->setBrush(rectColor);

          QRect aRect = anItem.positionRect;
          QRect displayRect = QRect(aRect.left()*zoomFactor, aRect.top()*zoomFactor, aRect.width()*zoomFactor, aRect.height()*zoomFactor);

          painter->fillRect(displayRect, rectColor);

          break;
        }
      }

      // optionally, draw a grid around the region
      if (statsTypeList[i].renderGrid)
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

    return NULL;
}

// return raw(!) value of frontmost, active statistic item at given position
// Info is always read from the current buffer. So these values are only valid if a draw event occured first.
ValuePairList statisticHandler::getValuesAt(QPoint pos)
{
  ValuePairList valueList;

  for (int i = 0; i<statsTypeList.count(); i++)
  {
    if (statsTypeList[i].render)  // only show active values
    {
      int typeID = statsTypeList[i].typeID;
      StatisticsItemList statsList = statsCache[typeID];

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

        if (aRect.contains(pos))
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
bool statisticHandler::setStatisticsTypeList(const StatisticsTypeList & typeList)
{
  bool bChanged = false;
  foreach(const StatisticsType & aType, typeList)
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
bool statisticHandler::anyStatisticsRendered()
{
  for (int i = 0; i<statsTypeList.count(); i++)
  {
    if( statsTypeList[i].render )
      return true;
  }
  return false;
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
    // Append the name (with the checkbox to enable/disable the statistics item)
    QCheckBox *itemNameCheck = new QCheckBox( statsTypeList[row].typeName, ui.scrollAreaWidgetContents);
    itemNameCheck->setChecked( statsTypeList[row].render );
    ui.gridLayout->addWidget(itemNameCheck, row+2, 0);
    connect(itemNameCheck, SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
    itemNameCheckBoxes[0].append(itemNameCheck);

    // Append the opactiy slider
    QSlider *opacitySlider = new QSlider( Qt::Horizontal );
    opacitySlider->setMinimum(0);
    opacitySlider->setMaximum(100);
    opacitySlider->setValue(statsTypeList[row].alphaFactor);
    ui.gridLayout->addWidget(opacitySlider, row+2, 1);
    connect(opacitySlider, SIGNAL(valueChanged(int)), this, SLOT(onStatisticsControlChanged()));
    itemOpacitySliders[0].append(opacitySlider);

    // Append the grid checkbox
    QCheckBox *gridCheckbox = new QCheckBox( "", ui.scrollAreaWidgetContents );
    gridCheckbox->setChecked( statsTypeList[row].renderGrid );
    ui.gridLayout->addWidget(gridCheckbox, row+2, 2);
    connect(gridCheckbox, SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
    itemGridCheckBoxes[0].append(gridCheckbox);

    if (statsTypeList[row].visualizationType==vectorType)
    {
      // Append the arrow checkbox
      QCheckBox *arrowCheckbox = new QCheckBox( "", ui.scrollAreaWidgetContents );
      arrowCheckbox->setChecked( statsTypeList[row].showArrow );
      ui.gridLayout->addWidget(arrowCheckbox, row+2, 3);
      connect(arrowCheckbox, SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
      itemArrowCheckboxes[0].append(arrowCheckbox);

    }
    else
    {
      QCheckBox *arrowCheckbox = NULL;
      itemArrowCheckboxes[0].append(arrowCheckbox);
    }
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
      secondaryControlsWidget->setLayout( ui2.verticalLayout );
    }

    // Add the controls to the gridLayer
    for (int row = 0; row < statsTypeList.length(); ++row)
    {
      // Append the name (with the checkbox to enable/disable the statistics item)
      QCheckBox *itemNameCheck = new QCheckBox( statsTypeList[row].typeName, ui2.scrollAreaWidgetContents);
      itemNameCheck->setChecked( statsTypeList[row].render );
      ui2.gridLayout->addWidget(itemNameCheck, row+2, 0);
      connect(itemNameCheck, SIGNAL(stateChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
      itemNameCheckBoxes[1].append(itemNameCheck);

      // Append the opactiy slider
      QSlider *opacitySlider = new QSlider( Qt::Horizontal );
      opacitySlider->setMinimum(0);
      opacitySlider->setMaximum(100);
      opacitySlider->setValue(statsTypeList[row].alphaFactor);
      ui2.gridLayout->addWidget(opacitySlider, row+2, 1);
      connect(opacitySlider, SIGNAL(valueChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
      itemOpacitySliders[1].append(opacitySlider);

      // Append the grid checkbox
      QCheckBox *gridCheckbox = new QCheckBox( "", ui2.scrollAreaWidgetContents );
      gridCheckbox->setChecked( statsTypeList[row].renderGrid );
      ui2.gridLayout->addWidget(gridCheckbox, row+2, 2);
      connect(gridCheckbox, SIGNAL(stateChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
      itemGridCheckBoxes[1].append(gridCheckbox);

      if (statsTypeList[row].visualizationType==vectorType)
      {
        // Append the arrow checkbox
        QCheckBox *arrowCheckbox = new QCheckBox( "", ui2.scrollAreaWidgetContents );
        arrowCheckbox->setChecked( statsTypeList[row].showArrow );
        ui2.gridLayout->addWidget(arrowCheckbox, row+2, 3);
        connect(arrowCheckbox, SIGNAL(stateChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
        itemArrowCheckboxes[1].append(arrowCheckbox);

      }
      else
      {
        QCheckBox *arrowCheckbox = NULL;
        itemArrowCheckboxes[1].append(arrowCheckbox);
      }
    }

    // Add a spacer at the very bottom
    QSpacerItem *verticalSpacer = new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui2.gridLayout->addItem(verticalSpacer, statsTypeList.length()+2, 0, 1, 1);
    spacerItems[1] = verticalSpacer;

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
    statsTypeList[row].renderGrid  = itemGridCheckBoxes[0][row]->isChecked();

    if (itemArrowCheckboxes[0][row])
      statsTypeList[row].showArrow = itemArrowCheckboxes[0][row]->isChecked();

    // Enable/disable the slider and grid checkbox depending on the item name check box
    bool enable = itemNameCheckBoxes[0][row]->isChecked();
    itemOpacitySliders[0][row]->setEnabled( enable );
    itemGridCheckBoxes[0][row]->setEnabled( enable );
    if (itemArrowCheckboxes[0][row])
      itemArrowCheckboxes[0][row]->setEnabled( enable );

    // Update the secondary controls if they were created
    if (ui2.created() && itemNameCheckBoxes[1].length() > 0)
    {
      // Update the controls that changed
      if (itemNameCheckBoxes[0][row]->isChecked() != itemNameCheckBoxes[1][row]->isChecked())
      {
        disconnect(itemNameCheckBoxes[1][row], SIGNAL(stateChanged(int)));
        itemNameCheckBoxes[1][row]->setChecked( itemNameCheckBoxes[0][row]->isChecked() );
        connect(itemNameCheckBoxes[1][row], SIGNAL(stateChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
      }

      if (itemOpacitySliders[0][row]->value() != itemOpacitySliders[1][row]->value())
      {
        disconnect(itemOpacitySliders[1][row], SIGNAL(valueChanged(int)));
        itemOpacitySliders[1][row]->setValue( itemOpacitySliders[0][row]->value() );
        connect(itemOpacitySliders[1][row], SIGNAL(valueChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
      }

      if (itemGridCheckBoxes[0][row]->isChecked() != itemGridCheckBoxes[1][row]->isChecked())
      {
        disconnect(itemGridCheckBoxes[1][row], SIGNAL(stateChanged(int)));
        itemGridCheckBoxes[1][row]->setChecked( itemGridCheckBoxes[0][row]->isChecked() );
        connect(itemGridCheckBoxes[1][row], SIGNAL(stateChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
      }

      if (itemArrowCheckboxes[0][row] && itemArrowCheckboxes[0][row]->isChecked() != itemArrowCheckboxes[1][row]->isChecked())
      {
        disconnect(itemArrowCheckboxes[1][row], SIGNAL(stateChanged(int)));
        itemArrowCheckboxes[1][row]->setChecked( itemArrowCheckboxes[0][row]->isChecked() );
        connect(itemArrowCheckboxes[1][row], SIGNAL(stateChanged(int)), this, SLOT(onSecondaryStatisticsControlChanged()));
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
    statsTypeList[row].renderGrid  = itemGridCheckBoxes[1][row]->isChecked();

    if (itemArrowCheckboxes[1][row])
      statsTypeList[row].showArrow = itemArrowCheckboxes[1][row]->isChecked();

    // Enable/disable the slider and grid checkbox depending on the item name check box
    bool enable = itemNameCheckBoxes[1][row]->isChecked();
    itemOpacitySliders[1][row]->setEnabled( enable );
    itemGridCheckBoxes[1][row]->setEnabled( enable );
    if (itemArrowCheckboxes[1][row])
      itemArrowCheckboxes[1][row]->setEnabled( enable );

    // Update the primary controls that changed
    if (itemNameCheckBoxes[0][row]->isChecked() != itemNameCheckBoxes[1][row]->isChecked())
    {
      disconnect(itemNameCheckBoxes[0][row], SIGNAL(stateChanged(int)));
      itemNameCheckBoxes[0][row]->setChecked( itemNameCheckBoxes[1][row]->isChecked() );
      connect(itemNameCheckBoxes[0][row], SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
    }

    if (itemOpacitySliders[0][row]->value() != itemOpacitySliders[1][row]->value())
    {
      disconnect(itemOpacitySliders[0][row], SIGNAL(valueChanged(int)));
      itemOpacitySliders[0][row]->setValue( itemOpacitySliders[1][row]->value() );
      connect(itemOpacitySliders[0][row], SIGNAL(valueChanged(int)), this, SLOT(onStatisticsControlChanged()));
    }

    if (itemGridCheckBoxes[0][row]->isChecked() != itemGridCheckBoxes[1][row]->isChecked())
    {
      disconnect(itemGridCheckBoxes[0][row], SIGNAL(stateChanged(int)));
      itemGridCheckBoxes[0][row]->setChecked( itemGridCheckBoxes[1][row]->isChecked() );
      connect(itemGridCheckBoxes[0][row], SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
    }

    if (itemArrowCheckboxes[0][row] && itemArrowCheckboxes[0][row]->isChecked() != itemArrowCheckboxes[1][row]->isChecked())
    {
      disconnect(itemArrowCheckboxes[0][row], SIGNAL(stateChanged(int)));
      itemArrowCheckboxes[0][row]->setChecked( itemArrowCheckboxes[1][row]->isChecked() );
      connect(itemArrowCheckboxes[0][row], SIGNAL(stateChanged(int)), this, SLOT(onStatisticsControlChanged()));
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
  itemGridCheckBoxes[1].clear();
  itemArrowCheckboxes[1].clear();
}

void statisticHandler::savePlaylist(QDomElementYUView &root)
{
  for (int row = 0; row < statsTypeList.length(); ++row)
  {
    QDomElement newChild = root.ownerDocument().createElement(QString("statType%1").arg(row));
    newChild.appendChild( root.ownerDocument().createTextNode(statsTypeList[row].typeName) );
    newChild.setAttribute("render", statsTypeList[row].render);
    newChild.setAttribute("alpha", statsTypeList[row].alphaFactor);
    newChild.setAttribute("renderGrid", statsTypeList[row].renderGrid);
    if (statsTypeList[row].visualizationType==vectorType)
      newChild.setAttribute("showArrow", statsTypeList[row].showArrow);

    root.appendChild( newChild );
  }
}

void statisticHandler::loadPlaylist(QDomElementYUView &root)
{
  QString statItemName;
  int i = 0;
  do
  {
    ValuePairList attributes;
    statItemName = root.findChildValue(QString("statType%1").arg(i++), attributes);

    for (int row = 0; row < statsTypeList.length(); ++row)
    {
      if (statsTypeList[row].typeName == statItemName)
      {
        // Get the values from the attribute
        bool render = false;
        int alpha = 50;
        bool renderGrid = true;
        bool showArrow = true;
        for (int i = 0; i < attributes.length(); i++)
        {
          if (attributes[i].first == "render")
            render = (attributes[i].second != "0");
          else if (attributes[i].first == "alpha")
            alpha = attributes[i].second.toInt();
          else if (attributes[i].first == "renderGrid")
            renderGrid = (attributes[i].second != "0");
          else if (attributes[i].first == "showArrow")
            showArrow = (attributes[i].second != "0");
        }

        // Set the values
        statsTypeList[row].render = render;
        statsTypeList[row].alphaFactor = alpha;
        statsTypeList[row].renderGrid = renderGrid;
        statsTypeList[row].showArrow = showArrow;
      }
    }
  } while (!statItemName.isEmpty());
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
      Q_ASSERT(itemNameCheckBoxes[0].length() == itemGridCheckBoxes[0].length());
      Q_ASSERT(itemNameCheckBoxes[0].length() == itemArrowCheckboxes[0].length());

      // Delete primary controls
      delete itemNameCheckBoxes[0][i];
      delete itemOpacitySliders[0][i];
      delete itemGridCheckBoxes[0][i];
      delete itemArrowCheckboxes[0][i];

      if (ui2.created())
      {
        Q_ASSERT(itemNameCheckBoxes[1].length() == itemOpacitySliders[1].length());
        Q_ASSERT(itemNameCheckBoxes[1].length() == itemGridCheckBoxes[1].length());
        Q_ASSERT(itemNameCheckBoxes[1].length() == itemArrowCheckboxes[1].length());

        // Delete secondary controls
        delete itemNameCheckBoxes[1][i];
        delete itemOpacitySliders[1][i];
        delete itemGridCheckBoxes[1][i];
        delete itemArrowCheckboxes[1][i];
      }
    }

    // Delete the spacer items at the bottom.
    assert(spacerItems[0] != NULL);
    ui.gridLayout->removeItem(spacerItems[0]);
    delete spacerItems[0];
    spacerItems[0] = NULL;

    // Delete all pointers to the widgets. The layout has the ownership and removing the
    // widget should delete it.
    itemNameCheckBoxes[0].clear();
    itemOpacitySliders[0].clear();
    itemGridCheckBoxes[0].clear();
    itemArrowCheckboxes[0].clear();

    if (ui2.created())
    {
      // Delete all pointers to the widgets. The layout has the ownership and removing the
      // widget should delete it.
      itemNameCheckBoxes[1].clear();
      itemOpacitySliders[1].clear();
      itemGridCheckBoxes[1].clear();
      itemArrowCheckboxes[1].clear();

      // Delete the spacer items at the bottom.
      assert(spacerItems[1] != NULL);
      ui2.gridLayout->removeItem(spacerItems[1]);
      delete spacerItems[1];
      spacerItems[1] = NULL;
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
          statsTypeList[j].render      = statsTypeListBackup[i].render;
          statsTypeList[j].renderGrid  = statsTypeListBackup[i].renderGrid;
          statsTypeList[j].alphaFactor = statsTypeListBackup[i].alphaFactor;
          statsTypeList[j].showArrow   = statsTypeListBackup[i].showArrow;
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
  // to revreate the new controls. This way we can see which statistics were drawn / how.
  statsTypeListBackup = statsTypeList;

  // Clear the old list. New items can be added now.
  statsTypeList.clear();
}