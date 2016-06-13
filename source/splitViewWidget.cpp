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

#include "splitViewWidget.h"

#include <QPainter>
#include <QColor>
#include <QSettings>
#include <QDebug>
#include <QTextDocument>

#include "typedef.h"
#include "playlistTreeWidget.h"
#include "playbackController.h"
#include "videoHandler.h"

splitViewWidget::splitViewWidget(QWidget *parent, bool separateView)
  : QWidget(parent)
{
  setFocusPolicy(Qt::NoFocus);
  isSeparateWidget = separateView;
  otherWidget = NULL;

  linkViews = false;
  playbackPrimary = false;
  isViewFrozen = false;

  // Setup the controls for the primary splitviewWidget. The separateView will use (connect to) the primarys controls.
  controls = NULL;
  if (!separateView)
    controls = new Ui::splitViewControlsWidget;

  splittingPoint = 0.5;
  splittingDragging = false;
  setSplitEnabled(false);
  viewDragging = false;
  viewMode = SIDE_BY_SIDE;
  drawZoomBox = false;
  drawRegularGrid = false;
  regularGridSize = 64;
  zoomBoxMousePosition = QPoint();

  playlist = NULL;
  playback = NULL;

  updateSettings();

  centerOffset = QPoint(0, 0);
  zoomFactor = 1.0;

  // Initialize the font and the position of the zoom factor indication
  zoomFactorFont = QFont(SPLITVIEWWIDGET_ZOOMFACTOR_FONT, SPLITVIEWWIDGET_ZOOMFACTOR_FONTSIZE);
  QFontMetrics fm(zoomFactorFont);
  zoomFactorFontPos = QPoint( 10, fm.height() );

  // We want to have all mouse events (even move)
  setMouseTracking(true);
}

void splitViewWidget::setSplitEnabled(bool flag)
{
  if (splitting != flag)
  {
    // Value changed
    splitting = flag;

    // Update (redraw) widget
    update();
  }
}

/** The common settings might have changed.
  * Reload all settings from the Qsettings and set them.
  */
void splitViewWidget::updateSettings()
{
  // load the background color from settings and set it
  QPalette Pal(palette());
  QSettings settings;
  QColor bgColor = settings.value("Background/Color").value<QColor>();
  Pal.setColor(QPalette::Background, bgColor);
  setAutoFillBackground(true);
  setPalette(Pal);

  // Load the split line style from the settings and set it
  QString splittingStyleString = settings.value("SplitViewLineStyle").value<QString>();
  if (splittingStyleString == "Handlers")
    splittingLineStyle = TOP_BOTTOM_HANDLERS;
  else
    splittingLineStyle = SOLID_LINE;

  // Load zoom box background color
  zoomBoxBackgroundColor = settings.value("Background/Color").value<QColor>();
}

void splitViewWidget::paintEvent(QPaintEvent *paint_event)
{
  //qDebug() << paint_event->rect();
  Q_UNUSED(paint_event);

  if (!playlist)
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;

  QPainter painter(this);

  // Get the full size of the area that we can draw on (from the paint device base)
  QPoint drawArea_botR(width(), height());

  if (isViewFrozen)
  {
    // Just draw the pixmap of the frozen view in the center of the current widget
    int x = (drawArea_botR.x() - frozenViewImage.size().width()) / 2;
    int y = (drawArea_botR.y() - frozenViewImage.size().height()) / 2;
    painter.drawPixmap(x, y, frozenViewImage);
    return;
  }

  // Get the current frame to draw
  int frame = playback->getCurrentFrame();

  // Get the playlist item(s) to draw
  playlistItem *item[2];
  playlist->getSelectedItems(item[0], item[1]);
  bool anyItemsSelected = item[0] != NULL || item[1] != NULL;

  // The x position of the split (if splitting)
  int xSplit = int(drawArea_botR.x() * splittingPoint);

  // First determine the center points per of each view
  QPoint centerPoints[2];
  if (viewMode == COMPARISON || !splitting)
  {
    // For comparison mode, both items have the same center point, in the middle of the view widget
    // This is equal to the scenario of not splitting
    centerPoints[0] = drawArea_botR / 2;
    centerPoints[1] = centerPoints[0];
  }
  else
  {
    // For side by side mode, the center points are centered in each individual split view
    int y = drawArea_botR.y() / 2;
    centerPoints[0] = QPoint( xSplit / 2, y );
    centerPoints[1] = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, y );
  }

  // For the zoom box, calculate the pixel position under the cursor for each view. The following
  // things are calculated in this function:
  bool    pixelPosInItem[2] = {false, false};  //< Is the pixel position under the curser within the item?
  QRect   zoomPixelRect[2];                    //< A rect around the pixel that is under the cursor
  if (anyItemsSelected && drawZoomBox)
  {
    // We now have the pixel difference value for the item under the cursor.
    // We now draw one zoom box per view
    int viewNum = (splitting && item[1]) ? 2 : 1;
    for (int view=0; view<viewNum; view++)
    {
      // Get the size of the item
      double itemSize[2];
      itemSize[0] = item[view]->getSize().width();
      itemSize[1] = item[view]->getSize().height();

      // Is the pixel under the cursor within the item?
      pixelPosInItem[view] = (zoomBoxPixelUnderCursor[view].x() >= 0 && zoomBoxPixelUnderCursor[view].x() < itemSize[0]) &&
                             (zoomBoxPixelUnderCursor[view].y() >= 0 && zoomBoxPixelUnderCursor[view].y() < itemSize[1]);

      // Mark the pixel under the cursor with a rect around it.
      if (pixelPosInItem[view])
      {
        int pixelPoint[2];
        pixelPoint[0] = -((itemSize[0] / 2 - zoomBoxPixelUnderCursor[view].x()) * zoomFactor);
        pixelPoint[1] = -((itemSize[1] / 2 - zoomBoxPixelUnderCursor[view].y()) * zoomFactor);
        zoomPixelRect[view] = QRect(pixelPoint[0], pixelPoint[1], zoomFactor, zoomFactor);
      }
    }
  }

  if (splitting)
  {
    // Draw two items (or less, if less items are selected)
    if (item[0])
    {
      // Set clipping to the left region
      QRegion clip = QRegion(0, 0, xSplit, drawArea_botR.y());
      painter.setClipRegion( clip );

      // Translate the painter to the position where we want the item to be
      painter.translate( centerPoints[0] + centerOffset );

      // Draw the item at position (0,0)
      item[0]->drawItem( &painter, frame, zoomFactor, playback->playing() );

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[0]);

      if (pixelPosInItem[0])
      {
        // If the zoom box is active, draw a rect around the pixel currently under the cursor
        frameHandler *vid = item[0]->getFrameHandler();
        if (vid)
        {
          painter.setPen( vid->isPixelDark(zoomBoxPixelUnderCursor[0]) ? Qt::white : Qt::black );
          painter.drawRect( zoomPixelRect[0] );
        }
      }

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(0, &painter, xSplit, drawArea_botR, item[0], frame, zoomBoxPixelUnderCursor[0], pixelPosInItem[0], zoomFactor );
    }
    if (item[1])
    {
      // Set clipping to the right region
      QRegion clip = QRegion(xSplit, 0, drawArea_botR.x() - xSplit, drawArea_botR.y());
      painter.setClipRegion( clip );

      // Translate the painter to the position where we want the item to be
      painter.translate( centerPoints[1] + centerOffset );

      // Draw the item at position (0,0)
      item[1]->drawItem( &painter, frame, zoomFactor, playback->playing() );

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[1]);

      if (pixelPosInItem[1])
      {
        // If the zoom box is active, draw a rect around the pixel currently under the cursor
        frameHandler *vid = item[1]->getFrameHandler();
        if (vid)
        {
          painter.setPen( vid->isPixelDark(zoomBoxPixelUnderCursor[1]) ? Qt::white : Qt::black );
          painter.drawRect( zoomPixelRect[1] );
        }
      }

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(1, &painter, xSplit, drawArea_botR, item[1], frame, zoomBoxPixelUnderCursor[1], pixelPosInItem[1], zoomFactor );
    }

    // Disable clipping
    painter.setClipping( false );
  }
  else // (!splitting)
  {
    // Draw one item (if one item is selected)
    if (item[0])
    {
      centerPoints[0] = drawArea_botR / 2;

      // Translate the painter to the position where we want the item to be
      painter.translate( centerPoints[0] + centerOffset );

      // Draw the item at position (0,0)
      item[0]->drawItem( &painter, frame, zoomFactor, playback->playing() );

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[0]);

      if (pixelPosInItem[0])
      {
        // If the zoom box is active, draw a rect around the pixel currently under the cursor
        frameHandler *vid = item[0]->getFrameHandler();
        if (vid)
        {
          painter.setPen( vid->isPixelDark(zoomBoxPixelUnderCursor[0]) ? Qt::white : Qt::black );
          painter.drawRect( zoomPixelRect[0] );
        }
      }

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(0, &painter, xSplit, drawArea_botR, item[0], frame, zoomBoxPixelUnderCursor[0], pixelPosInItem[0], zoomFactor );
    }
  }

  if (splitting)
  {
    if (splittingLineStyle == TOP_BOTTOM_HANDLERS)
    {
      // Draw small handlers at the top and bottom
      QPainterPath triangle;
      triangle.moveTo( xSplit-10, 0 );
      triangle.lineTo( xSplit   , 10);
      triangle.lineTo( xSplit+10,  0);
      triangle.closeSubpath();

      triangle.moveTo( xSplit-10, drawArea_botR.y() );
      triangle.lineTo( xSplit   , drawArea_botR.y() - 10);
      triangle.lineTo( xSplit+10, drawArea_botR.y() );
      triangle.closeSubpath();

      painter.fillPath( triangle, Qt::white );
    }
    else
    {
      // Draw the splitting line at position xSplit. All pixels left of the line
      // belong to the left view, and all pixels on the right belong to the right one.
      QLine line(xSplit, 0, xSplit, drawArea_botR.y());
      QPen splitterPen(Qt::white);
      //splitterPen.setStyle(Qt::DashLine);
      painter.setPen(splitterPen);
      painter.drawLine(line);
    }
  }

  // Draw the zoom factor
  if (zoomFactor != 1.0)
  {
    QString zoomString = QString("x%1").arg(zoomFactor);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(Qt::black));
    painter.setFont(zoomFactorFont);
    painter.drawText(zoomFactorFontPos, zoomString);
  }
}

void splitViewWidget::updatePixelPositions()
{
  // Get the selected item(s)
  playlistItem *item[2];
  playlist->getSelectedItems(item[0], item[1]);
  bool anyItemsSelected = item[0] != NULL || item[1] != NULL;

  // Get the full size of the area that we can draw on (from the paint device base)
  QPoint drawArea_botR(width(), height());

  // The x position of the split (if splitting)
  int xSplit = int(drawArea_botR.x() * splittingPoint);

  // First determine the center points per of each view
  QPoint centerPoints[2];
  if (viewMode == COMPARISON || !splitting)
  {
    // For comparison mode, both items have the same center point, in the middle of the view widget
    // This is equal to the scenario of not splitting
    centerPoints[0] = drawArea_botR / 2;
    centerPoints[1] = centerPoints[0];
  }
  else
  {
    // For side by side mode, the center points are centered in each individual split view
    int y = drawArea_botR.y() / 2;
    centerPoints[0] = QPoint( xSplit / 2, y );
    centerPoints[1] = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, y );
  }

  if (anyItemsSelected && drawZoomBox && geometry().contains(zoomBoxMousePosition))
  {
    // Is the mouse over the left or the right item? (mouseInLeftOrRightView: false=left, true=right)
    int xSplit = int(drawArea_botR.x() * splittingPoint);
    bool mouseInLeftOrRightView = (splitting && (zoomBoxMousePosition.x() > xSplit));

    // The absolute center point of the item under the cursor
    QPoint itemCenterMousePos = (mouseInLeftOrRightView) ? centerPoints[1] + centerOffset : centerPoints[0] + centerOffset;

    // The difference in the item under the mouse (normalized by zoom factor)
    double diffInItem[2] = {(double)(itemCenterMousePos.x() - zoomBoxMousePosition.x()) / zoomFactor + 0.5,
                            (double)(itemCenterMousePos.y() - zoomBoxMousePosition.y()) / zoomFactor + 0.5};

    // We now have the pixel difference value for the item under the cursor.
    // We now draw one zoom box per view
    int viewNum = (splitting && item[1]) ? 2 : 1;
    for (int view=0; view<viewNum; view++)
    {
      // Get the size of the item
      double itemSize[2];
      itemSize[0] = item[view]->getSize().width();
      itemSize[1] = item[view]->getSize().height();

      // Calculate the position under the mouse cursor in pixels in the item under the mouse.
      {
        // Divide and round. We want valuew from 0...-1 to be quantized to -1 and not 0
        // so subtract 1 from the value if it is < 0.
        double pixelPosX = -diffInItem[0] + (itemSize[0] / 2) + 0.5;
        double pixelPoxY = -diffInItem[1] + (itemSize[1] / 2) + 0.5;
        if (pixelPosX < 0)
          pixelPosX -= 1;
        if (pixelPoxY < 0)
          pixelPoxY -= 1;

        zoomBoxPixelUnderCursor[view] = QPoint(pixelPosX, pixelPoxY);
      }
    }
  }
}

void splitViewWidget::paintZoomBox(int view, QPainter *painter, int xSplit, QPoint drawArea_botR, playlistItem *item, int frame, QPoint pixelPos, bool pixelPosInItem, double zoomFactor)
{
  if (!drawZoomBox)
    return;

  const int zoomBoxFactor = 32;
  const int srcSize = 5;
  const int margin = 11;
  const int padding = 6;
  int zoomBoxSize = srcSize*zoomBoxFactor;

  // Where will the zoom view go?
  QRect zoomViewRect(0,0, zoomBoxSize, zoomBoxSize);

  bool drawInfoPanel = true;  // Do we draw the info panel?
  if (view == 1 && xSplit > (drawArea_botR.x() - margin - zoomBoxSize))
  {
    if (xSplit > drawArea_botR.x() - margin)
      // The split line is so far on the right, that the whole zoom box in view 1 is not visible
      return;

    // The split line is so far right, that part of the zoom box is hidden.
    // Resize the zoomViewRect to the part that is visible.
    zoomViewRect.setWidth( drawArea_botR.x() - xSplit - margin );

    drawInfoPanel = false;  // Info panel not visible
  }

  // Do not draw the zoom view if the zoomFactor is equal or greater than that of the zoom box
  if (zoomFactor < zoomBoxFactor)
  {
    if (view == 0 && splitting)
      zoomViewRect.moveBottomRight( QPoint(xSplit - margin, drawArea_botR.y() - margin) );
    else
      zoomViewRect.moveBottomRight( drawArea_botR - QPoint(margin, margin) );

    // Fill the viewRect with the background color
    painter->setPen( Qt::black );
    painter->fillRect(zoomViewRect, painter->background());

    // Restrict drawing to the zoom view rect. Save the old clipping region (if any) so we can
    // reset it later
    QRegion clipRegion;
    if (painter->hasClipping())
      clipRegion = painter->clipRegion();
    painter->setClipRegion( zoomViewRect );

    // Translate the painter to the point where the center of the zoom view will be
    painter->translate( zoomViewRect.center() );

    // Now we have to calculate the translation of the item, so that the pixel position
    // is in the center of the view (so we can draw it at (0,0)).
    QPointF itemZoomBoxTranslation = QPointF( item->getSize().width()  / 2 - pixelPos.x() - 0.5,
                                              item->getSize().height() / 2 - pixelPos.y() - 0.5 );
    painter->translate( itemZoomBoxTranslation * zoomBoxFactor );

    // Draw the item again, but this time with a high zoom factor into the clipped region
    item->drawItem( painter, frame, zoomBoxFactor, playback->playing() );

    // Reset transform and reset clipping to the previous clip region (if there was one)
    painter->resetTransform();
    if (clipRegion.isEmpty())
      painter->setClipping(false);
    else
      painter->setClipRegion(clipRegion);

    // Draw a rect around the zoom view
    painter->drawRect(zoomViewRect);
  }
  else
  {
    // If we don't draw the zoom box, consider the size to be 0.
    zoomBoxSize = 0;
  }

  if (drawInfoPanel)
  {
    // Draw pixel info. First, construct the text and see how the size is going to be.
    QString pixelInfoString = QString("<h4>Coordinates</h4>"
                              "<table width=\"100%\">"
                              "<tr><td>X:</td><td align=\"right\">%1</td></tr>"
                              "<tr><td>Y:</td><td align=\"right\">%2</td></tr>"
                              "</table>"
                              ).arg(pixelPos.x()).arg(pixelPos.y());

    // If the pixel position is within the item, append information on the pixel vale
    if (pixelPosInItem)
    {
      ValuePairListSets pixelListSets = item->getPixelValues(pixelPos, frame);
      // if we have some values, show them
      if( pixelListSets.size() > 0 )
      {
        for (int i = 0; i < pixelListSets.count(); i++)
        {
          QString title = pixelListSets[i].first;
          ValuePairList pixelValues = pixelListSets[i].second;
          pixelInfoString.append( QString("<h4>%1</h4>"
                                  "<table width=\"100%\">").arg(title) );
          for (int j = 0; j < pixelValues.size(); ++j)
            pixelInfoString.append( QString("<tr><td><nobr>%1:</nobr></td><td align=\"right\"><nobr>%2</nobr></td></tr>").arg(pixelValues[j].first).arg(pixelValues[j].second) );
          pixelInfoString.append( "</table>" );
        }
      }
    }

    // Create a QTextDocument. This object can tell us the size of the rendered text.
    QTextDocument textDocument;
    textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
    textDocument.setHtml(pixelInfoString);
    textDocument.setTextWidth(textDocument.size().width());

    // Translate to the position where the text box shall be
    if (view == 0 && splitting)
      painter->translate(xSplit - margin - zoomBoxSize - textDocument.size().width() - padding*2 + 1, drawArea_botR.y() - margin - textDocument.size().height() - padding*2 + 1);
    else
      painter->translate(drawArea_botR.x() - margin - zoomBoxSize - textDocument.size().width() - padding*2 + 1, drawArea_botR.y() - margin - textDocument.size().height() - padding*2 + 1);

    // Draw a black rect and then the text on top of that
    QRect rect(QPoint(0, 0), textDocument.size().toSize() + QSize(2*padding, 2*padding));
    QBrush originalBrush;
    painter->setBrush(QColor(0, 0, 0, 70));
    painter->setPen( Qt::black );
    painter->drawRect(rect);
    painter->translate(padding, padding);
    textDocument.drawContents(painter);
    painter->setBrush(originalBrush);

    painter->resetTransform();
  }
}

void splitViewWidget::paintRegularGrid(QPainter *painter, playlistItem *item)
{
  QSize itemSize = item->getSize() * zoomFactor;

  // Draw horizontal lines
  const int xMin = -itemSize.width() / 2;
  const int xMax =  itemSize.width() / 2;
  const int gridZoom = regularGridSize * zoomFactor;
  for (int y = 1; y <= (itemSize.height() - 1) / gridZoom; y++)
  {
    int yPos = (-itemSize.height() / 2) + y * gridZoom;
    painter->drawLine(xMin, yPos, xMax, yPos);
  }

  // Draw vertical lines
  const int yMin = -itemSize.height() / 2;
  const int yMax =  itemSize.height() / 2;
  for (int x = 1; x <= (itemSize.width() - 1) / gridZoom; x++)
  {
    int xPos = (-itemSize.width() / 2) + x * gridZoom;
    painter->drawLine(xPos, yMin, xPos, yMax);
  }
}

void splitViewWidget::mouseMoveEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::NoButton)
  {
    // We want this event
    mouse_event->accept();

    if (splitting && splittingDragging)
    {
      // The user is currently dragging the splitter. Calculate the new splitter point.
      int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2- SPLITVIEWWIDGET_SPLITTER_CLIPX));
      splittingPoint = (double)xClip / (double)(width()-2);

      // The splitter was moved. Update the widget.
      update();

      if (linkViews)
      {
        // Also set the new values in the other linked view
        otherWidget->splittingPoint = splittingPoint;
        otherWidget->update();
      }
    }
    else if (viewDragging)
    {
      // The user is currently dragging the view. Calculate the new offset from the center position
      centerOffset = viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart);

      // The view was moved. Update the widget.
      update();

      if (linkViews)
      {
        // Also set the new values in the other linked view
        otherWidget->centerOffset = centerOffset;
        otherWidget->update();
      }
    }
    else if (splitting)
    {
      // No buttons pressed, the view is split and we are not dragging.
      int splitPosPix = int((width()-2) * splittingPoint);

      if (mouse_event->x() > (splitPosPix-SPLITVIEWWIDGET_SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITVIEWWIDGET_SPLITTER_MARGIN))
      {
        // Mouse is over the line in the middle (plus minus 4 pixels)
        setCursor(Qt::SplitHCursor);
      }
      else
      {
        // Mouse is not over the splitter line
        setCursor(Qt::ArrowCursor);
      }
    }
  }

  if (drawZoomBox)
  {
    // If the mouse position changed, save the current point of the mouse and update the view (this will update the zoom box)
    if (zoomBoxMousePosition != mouse_event->pos())
    {
      zoomBoxMousePosition = mouse_event->pos();
      updatePixelPositions();
      update();

      if (linkViews)
      {
        otherWidget->zoomBoxPixelUnderCursor[0] = zoomBoxPixelUnderCursor[0];
        otherWidget->zoomBoxPixelUnderCursor[1] = zoomBoxPixelUnderCursor[1];
        otherWidget->update();
      }
    }
  }
}

void splitViewWidget::mousePressEvent(QMouseEvent *mouse_event)
{
  if (isViewFrozen)
    return;

  if (mouse_event->button() == Qt::LeftButton)
  {
    // Left mouse buttons pressed
    int splitPosPix = int((width()-2) * splittingPoint);

    // TODO: plus minus 4 pixels for the handle might be way to little for high DPI displays. This should depend on the screens DPI.
    if (splitting && mouse_event->x() > (splitPosPix-SPLITVIEWWIDGET_SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITVIEWWIDGET_SPLITTER_MARGIN))
    {
      // Mouse is over the splitter. Activate dragging of splitter.
      splittingDragging = true;

      // We handeled this event
      mouse_event->accept();
    }
  }
  else if (mouse_event->button() == Qt::RightButton)
  {
    // The user pressed the right mouse button. In this case drag the view.
    viewDragging = true;

    // Reset the cursor if it was another cursor (over the splitting line for example)
    setCursor(Qt::ArrowCursor);

    // Save the position where the user grabbed the item (screen), and the current value of
    // the centerOffset. So when the user moves the mouse, the new offset is just the old one
    // plus the difference between the position of the mouse and the position where the
    // user grabbed the item (screen).
    viewDraggingMousePosStart = mouse_event->pos();
    viewDraggingStartOffset = centerOffset;

    //qDebug() << "MouseGrab - Center: " << centerPoint << " rel: " << grabPosRelative;

    // We handeled this event
    mouse_event->accept();
  }
}

void splitViewWidget::mouseReleaseEvent(QMouseEvent *mouse_event)
{
  if (isViewFrozen)
    return;

  if (mouse_event->button() == Qt::LeftButton && splitting && splittingDragging)
  {
    // We want this event
    mouse_event->accept();

    // The left mouse button was released, we are showing a split view and the user is dragging the splitter.
    // End dragging.
    splittingDragging = false;

    // Update current splitting position / update last time
    int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2-SPLITVIEWWIDGET_SPLITTER_CLIPX));
    splittingPoint = (double)xClip / (double)(width()-2);

    // The view was moved. Update the widget.
    update();

    if (linkViews)
    {
      // Also set the new values in the other linked view
      otherWidget->splittingPoint = splittingPoint;
      otherWidget->update();
    }
  }
  else if (mouse_event->button() == Qt::RightButton && viewDragging)
  {
    // We want this event
    mouse_event->accept();

    // Calculate the new center offset one last time
    centerOffset = viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart);

    // The view was moved. Update the widget.
    update();

    // End dragging
    viewDragging = false;

    if (linkViews)
    {
      // Also set the new values in the other linked view
      otherWidget->centerOffset = centerOffset;
      otherWidget->update();
    }
  }
}

void splitViewWidget::wheelEvent (QWheelEvent *e)
{
  if (isViewFrozen)
    return;

  QPoint p = e->pos();
  e->accept();
  if (e->delta() > 0)
  {
    zoomIn(p);
  }
  else
  {
    zoomOut(p);
  }
}

void splitViewWidget::zoomIn(QPoint zoomPoint)
{
  // The zoom point works like this: After the zoom operation the pixel at zoomPoint shall
  // still be at the same position (zoomPoint)

  if (!zoomPoint.isNull())
  {
    // The center point has to be moved relative to the zoomPoint

    // Get the absolute center point of the item
    QPoint drawArea_botR(width(), height());
    QPoint centerPoint = drawArea_botR / 2;

    if (splitting && viewMode == SIDE_BY_SIDE)
    {
      // For side by side mode, the center points are centered in each individual split view

      // Which side of the split view are we zooming in?
      // Get the center point of that view
      int xSplit = int(drawArea_botR.x() * splittingPoint);
      if (zoomPoint.x() > xSplit)
        // Zooming in the right view
        centerPoint = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2 );
      else
        // Tooming in the left view
        centerPoint = QPoint( xSplit / 2, drawArea_botR.y() / 2 );
    }

    // The absolute center point of the item under the cursor
    QPoint itemCenter = centerPoint + centerOffset;

    // Move this item center point
    QPoint diff = itemCenter - zoomPoint;
    diff *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
    itemCenter = zoomPoint + diff;

    // Calculate the new cente offset
    centerOffset = itemCenter - centerPoint;
  }
  else
  {
    // Zoom in without considering the mouse position
    centerOffset *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }

  zoomFactor *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  update();

  if (linkViews)
  {
    // Also set the new values in the other linked view
    otherWidget->centerOffset = centerOffset;
    otherWidget->zoomFactor = zoomFactor;
    otherWidget->update();
  }
}

void splitViewWidget::zoomOut(QPoint zoomPoint)
{
  if (!zoomPoint.isNull() && SPLITVIEWWIDGET_ZOOM_OUT_MOUSE == 1)
  {
    // The center point has to be moved relative to the zoomPoint

    // Get the absolute center point of the item
    QPoint drawArea_botR(width(), height());
    QPoint centerPoint = drawArea_botR / 2;

    if (splitting && viewMode == SIDE_BY_SIDE)
    {
      // For side by side mode, the center points are centered in each individual split view

      // Which side of the split view are we zooming in?
      // Get the center point of that view
      int xSplit = int(drawArea_botR.x() * splittingPoint);
      if (zoomPoint.x() > xSplit)
        // Zooming in the right view
        centerPoint = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2 );
      else
        // Tooming in the left view
        centerPoint = QPoint( xSplit / 2, drawArea_botR.y() / 2 );
    }

    // The absolute center point of the item under the cursor
    QPoint itemCenter = centerPoint + centerOffset;

    // Move this item center point
    QPoint diff = itemCenter - zoomPoint;
    diff /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
    itemCenter = zoomPoint + diff;

    // Calculate the new cente offset
    centerOffset = itemCenter - centerPoint;
  }
  else
  {
    // Zoom out without considering the mouse position.
    centerOffset /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }

  zoomFactor /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  update();

  if (linkViews)
  {
    // Also set the new values in the other linked view
    otherWidget->centerOffset = centerOffset;
    otherWidget->zoomFactor = zoomFactor;
    otherWidget->update();
  }
}

void splitViewWidget::resetViews()
{
  centerOffset = QPoint(0,0);
  zoomFactor = 1.0;
  splittingPoint = 0.5;

  update();

  if (linkViews)
  {
    // Also reset the other view
    otherWidget->centerOffset = QPoint(0,0);
    otherWidget->zoomFactor = 1.0;
    otherWidget->splittingPoint = 0.5;

    otherWidget->update();
  }
}

void splitViewWidget::zoomToFit()
{
  if (!playlist)
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;

  centerOffset = QPoint(0,0);

  playlistItem *item[2];
  playlist->getSelectedItems(item[0], item[1]);

  if (item[0] == NULL)
    // We cannot zoom to anything
    return;

  double fracZoom = 1.0;
  if (!splitting)
  {
    // Get the size of item 0 and the size of the widget and set the zomm factor so that this fits
    QSize item0Size = item[0]->getSize();
    if (item0Size.width() <= 0 || item0Size.height() <= 0)
      return;

    double zoomH = (double)size().width() / item0Size.width();
    double zoomV = (double)size().height() / item0Size.height();

    fracZoom = std::min(zoomH, zoomV);
  }
  else if (splitting && viewMode == COMPARISON)
  {
    // We can just zoom to an item that is the size of the bigger of the two items
    QSize virtualItemSize = item[0]->getSize();

    if (item[1])
    {
      // Extend the size of the virtual item if a second item is available
      QSize item1Size = item[1]->getSize();
      if (item1Size.width() > virtualItemSize.width())
        virtualItemSize.setWidth( item1Size.width() );
      if (item1Size.height() > virtualItemSize.height())
        virtualItemSize.setHeight( item1Size.height() );
    }

    double zoomH = (double)size().width() / virtualItemSize.width();
    double zoomV = (double)size().height() / virtualItemSize.height();

    fracZoom = std::min(zoomH, zoomV);
  }
  else if (splitting && viewMode == SIDE_BY_SIDE)
  {
    // We have to know the size of the split parts and calculate a zoom factor for each part
    int xSplit = int(size().width() * splittingPoint);

    // Left item
    QSize item0Size = item[0]->getSize();
    if (item0Size.width() <= 0 || item0Size.height() <= 0)
      return;

    double zoomH = (double)xSplit / item0Size.width();
    double zoomV = (double)size().height() / item0Size.height();
    fracZoom = std::min(zoomH, zoomV);

    // Right item
    if (item[1])
    {
      QSize item1Size = item[1]->getSize();
      if (item1Size.width() > 0 && item1Size.height() > 0)
      {
        double zoomH2 = (double)(size().width() - xSplit) / item1Size.width();
        double zoomV2 = (double)size().height() / item1Size.height();
        double item2FracZoom = std::min(zoomH2, zoomV2);

        // If we need to zoom out more for item 2, then do so.
        if (item2FracZoom < fracZoom)
          fracZoom = item2FracZoom;
      }
    }
  }

  // We have a fractional zoom factor but we can only set multiples of SPLITVIEWWIDGET_ZOOM_STEP_FACTOR.
  // Find the next SPLITVIEWWIDGET_ZOOM_STEP_FACTOR multitude that fits.
  double newZoomFactor = 1.0;
  if (fracZoom < 1.0)
  {
    while (newZoomFactor > fracZoom)
      newZoomFactor /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }
  else
  {
    while (newZoomFactor * SPLITVIEWWIDGET_ZOOM_STEP_FACTOR < fracZoom)
      newZoomFactor *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }

  // Set new zoom factor and update
  zoomFactor = newZoomFactor;
  update();

  if (linkViews)
  {
    // Also set the new values in the other linked view
    otherWidget->centerOffset = centerOffset;
    otherWidget->zoomFactor = zoomFactor;
    otherWidget->update();
  }
}

void splitViewWidget::setupControls(QDockWidget *dock)
{
  // Initialize the controls and add them to the given widget.
  QWidget *controlsWidget = new QWidget(dock);
  controls->setupUi( controlsWidget );
  dock->setWidget( controlsWidget );

  // Connect signals/slots
  connect(controls->SplitViewgroupBox, SIGNAL(toggled(bool)), this, SLOT(on_SplitViewgroupBox_toggled(bool)));
  connect(controls->viewComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_viewComboBox_currentIndexChanged(int)));
  connect(controls->regularGridCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_regularGridCheckBox_toggled(bool)));
  connect(controls->gridSizeBox, SIGNAL(valueChanged(int)), this, SLOT(on_gridSizeBox_valueChanged(int)));
  connect(controls->zoomBoxCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_zoomBoxCheckBox_toggled(bool)));
  connect(controls->separateViewGroupBox, SIGNAL(toggled(bool)), this, SLOT(on_separateViewGroupBox_toggled(bool)));
  connect(controls->linkViewsCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_linkViewsCheckBox_toggled(bool)));
  connect(controls->playbackPrimaryCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_playbackPrimaryCheckBox_toggled(bool)));
}

void splitViewWidget::on_viewComboBox_currentIndexChanged(int index)
{
  switch (index)
  {
    case 0: // SIDE_BY_SIDE
      setViewMode(SIDE_BY_SIDE);
      break;
    case 1: // COMPARISON
      setViewMode(COMPARISON);
      break;
  }
}

void splitViewWidget::setPrimaryWidget(splitViewWidget *primary)
{
  Q_ASSERT_X(isSeparateWidget, "setPrimaryWidget", "Call this function only on the separate widget.");
  otherWidget = primary;

  // The primary splitViewWidget did set up controls for the widget. Connect signals/slots from these controls also here.
  connect(primary->controls->SplitViewgroupBox, SIGNAL(toggled(bool)), this, SLOT(on_SplitViewgroupBox_toggled(bool)));
  connect(primary->controls->viewComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_viewComboBox_currentIndexChanged(int)));
  connect(primary->controls->regularGridCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_regularGridCheckBox_toggled(bool)));
  connect(primary->controls->gridSizeBox, SIGNAL(valueChanged(int)), this, SLOT(on_gridSizeBox_valueChanged(int)));
  connect(primary->controls->zoomBoxCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_zoomBoxCheckBox_toggled(bool)));
  connect(primary->controls->linkViewsCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_linkViewsCheckBox_toggled(bool)));
}

void splitViewWidget::setSeparateWidget(splitViewWidget *separate)
{
  Q_ASSERT_X(!isSeparateWidget, "setSeparateWidget", "Call this function only on the primary widget.");
  otherWidget = separate;
}

void splitViewWidget::on_linkViewsCheckBox_toggled(bool state)
{
  linkViews = state;
  if (isSeparateWidget && linkViews)
  {
    // The user just switched on linking the views and this is the secondary view.
    // Get the view values from the primary view.
    centerOffset = otherWidget->centerOffset;
    zoomFactor = otherWidget->zoomFactor;
    splittingPoint = otherWidget->splittingPoint;
    update();
  }
}

void splitViewWidget::separateViewShow()
{
  Q_ASSERT_X(!isSeparateWidget, "setSeparateWidget", "Call this function only on the primary widget.");

  if (!controls->separateViewGroupBox->isChecked())
  {
    controls->separateViewGroupBox->setChecked(true);
  }
}

void splitViewWidget::separateViewHide()
{
  Q_ASSERT_X(!isSeparateWidget, "setSeparateWidget", "Call this function only on the primary widget.");

  if (controls->separateViewGroupBox->isChecked())
  {
    controls->separateViewGroupBox->setChecked(false);
  }
}

QPixmap splitViewWidget::getScreenshot()
{
  playlistItem *item[2];
  playlist->getSelectedItems(item[0], item[1]);
  if (item[0] || (item[0] && item[1] && splitting && viewMode == COMPARISON ))
  {
    QSize itemSize =item[0]->getSize();
    itemSize *= zoomFactor;
    QPixmap pixmap(itemSize);
    QPoint drawOffset = QPoint(size().width()/2,size().height()/2);
    QPoint topLeftOffset = QPoint(itemSize.width()/2,itemSize.height()/2);
    drawOffset += (centerOffset - topLeftOffset);
    QRegion captureRegion = QRegion(drawOffset.x()+1,drawOffset.y()+1,itemSize.width(),itemSize.height());
    render(&pixmap, QPoint(), captureRegion,RenderFlags(~DrawWindowBackground|DrawChildren));
    return pixmap;
  }
  else
  {
    //TODO: do it right for other modes
    QPixmap pixmap(size());
    render(&pixmap, QPoint(), QRegion(geometry()),RenderFlags(~DrawWindowBackground|DrawChildren));
    return pixmap;
  }
}

void splitViewWidget::freezeView(bool freeze)
{
  if (isViewFrozen && !freeze)
  {
    // View is frozen and should be unfrozen
    isViewFrozen = false;
    setMouseTracking(true);
    update();
  }
  if (!isViewFrozen && freeze)
  {
    if (!isSeparateWidget && controls->separateViewGroupBox->isChecked() && !playbackPrimary)
    {
      // Freeze the view. Get a screenshot and convert it to grayscale.
      QImage grayscaleImage = getScreenshot().toImage();

      // Convert the image to grayscale
      for (int i = 0; i < grayscaleImage.height(); i++)
      {
        uchar* scan = grayscaleImage.scanLine(i);
        for (int j = 0; j < grayscaleImage.width(); j++)
        {
          QRgb* rgbpixel = reinterpret_cast<QRgb*>(scan + j * 4);
          int gray = qGray(*rgbpixel);
          *rgbpixel = QColor(gray, gray, gray).rgba();
        }
      }

      frozenViewImage = QPixmap::fromImage(grayscaleImage);

      isViewFrozen = true;
      setMouseTracking(false);
      update();
    }
  }
}

void splitViewWidget::on_playbackPrimaryCheckBox_toggled(bool state)
{
  playbackPrimary = state;

  if (!isSeparateWidget && controls->separateViewGroupBox->isChecked() && playback->playing())
  {
    // We have to freeze/unfreeze the widget
    freezeView(!state);
  }
}

void splitViewWidget::on_separateViewGroupBox_toggled(bool state)
{
  // Unfreeze the view if the separate view is disabled
  if (!state && isViewFrozen)
    freezeView(false);

  if (state && playback->playing())
    freezeView(true);

  emit signalShowSeparateWindow(state);
}
