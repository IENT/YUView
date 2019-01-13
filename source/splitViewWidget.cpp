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

#include "splitViewWidget.h"

#include <QBackingStore>
#include <QDockWidget>
#include <QGestureEvent>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QTextDocument>
#include "frameHandler.h"
#include "playbackController.h"
#include "playlistItem.h"
#include "videoCache.h"

// Activate this if you want to know when which item is triggered to load and draw
#define SPLITVIEWWIDGET_DEBUG_LOAD_DRAW 0
#if SPLITVIEWWIDGET_DEBUG_LOAD_DRAW && !NDEBUG
#define DEBUG_LOAD_DRAW qDebug
#else
#define DEBUG_LOAD_DRAW(fmt,...) ((void)0)
#endif

splitViewWidget::splitViewWidget(QWidget *parent, bool separateView)
  : QWidget(parent), isSeparateWidget(separateView), parentWidget(parent)
{
  setFocusPolicy(Qt::NoFocus);
  setSplitEnabled(false);
  updateSettings();

  centerOffset = QPoint(0, 0);

  // No test running yet
  connect(&testProgrssUpdateTimer, &QTimer::timeout, this, [=]{ updateTestProgress(); });

  // Initialize the font and the position of the zoom factor indication
  zoomFactorFont = QFont(SPLITVIEWWIDGET_ZOOMFACTOR_FONT, SPLITVIEWWIDGET_ZOOMFACTOR_FONTSIZE);
  QFontMetrics fm(zoomFactorFont);
  zoomFactorFontPos = QPoint(10, fm.height());

  // Grab some touch gestures
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::PinchGesture);

  // Load the caching pixmap
  waitingForCachingPixmap = QPixmap(":/img_hourglass.png");

  // We want to have all mouse events (even move)
  setMouseTracking(true);
}

void splitViewWidget::setPlaylistTreeWidget(PlaylistTreeWidget *p) { playlist = p; }
void splitViewWidget::setPlaybackController(PlaybackController *p) { playback = p; }
void splitViewWidget::setVideoCache        (videoCache         *p) { cache = p;    }

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
  * Reload all settings from the QSettings and set them.
  */
void splitViewWidget::updateSettings()
{
  // Update the palette in the next draw event.
  // We don't do this here because Qt overwrites the setting if the theme is changed.
  paletteNeedsUpdate = true;

  // Get the color of the regular grid
  QSettings settings;
  regularGridColor = settings.value("OverlayGrid/Color").value<QColor>();

  // Load the split line style from the settings and set it
  QString splittingStyleString = settings.value("SplitViewLineStyle").toString();
  if (splittingStyleString == "Handlers")
    splittingLineStyle = TOP_BOTTOM_HANDLERS;
  else
    splittingLineStyle = SOLID_LINE;

  // Load the mouse mode
  QString mouseModeString = settings.value("MouseMode", "Left Zoom, Right Move").toString();
  if (mouseModeString == "Left Zoom, Right Move")
    mouseMode = MOUSE_RIGHT_MOVE;
  else
    mouseMode = MOUSE_LEFT_MOVE;

  // Load zoom box background color
  zoomBoxBackgroundColor = settings.value("Background/Color").value<QColor>();
}

void splitViewWidget::paintEvent(QPaintEvent *paint_event)
{
  Q_UNUSED(paint_event);

  if (paletteNeedsUpdate)
  {
    // load the background color from settings and set it
    QPalette Pal(palette());
    QSettings settings;
    QColor bgColor = settings.value("Background/Color").value<QColor>();
    Pal.setColor(QPalette::Background, bgColor);
    setAutoFillBackground(true);
    setPalette(Pal);
    paletteNeedsUpdate = false;
  }

  if (!playlist)
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;

  QPainter painter(this);

  // Get the full size of the area that we can draw on (from the paint device base)
  QPoint drawArea_botR(width(), height());

  if (isViewFrozen)
  {
    QString text = "Playback is running in the separate view only.\nCheck 'Playback in primary view' if you want playback to run here too.";

    // Set the QRect where to show the text
    QFont displayFont = painter.font();
    QFontMetrics metrics(displayFont);
    QSize textSize = metrics.size(0, text);

    QRect textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(drawArea_botR / 2);

    // Draw a rectangle around the text in white with a black border
    QRect boxRect = textRect + QMargins(5, 5, 5, 5);
    painter.setPen(QPen(Qt::black, 1));
    painter.fillRect(boxRect,Qt::white);
    painter.drawRect(boxRect);

    // Draw the text
    painter.drawText(textRect, Qt::AlignCenter, text);

    // Update the mouse cursor
    updateMouseCursor();

    return;
  }

  DEBUG_LOAD_DRAW("splitViewWidget::paintEvent drawing %s", (isSeparateWidget) ? " separate widget" : "");

  // Get the current frame to draw
  int frame = playback->getCurrentFrame();

  // Is playback running?
  const bool playing = (playback) ? playback->playing() : false;
  // If yes, is is currently stalled because we are waiting for caching of an item to finish first?
  const bool waitingForCaching = playback->isWaitingForCaching();

  // Get the playlist item(s) to draw
  auto item = playlist->getSelectedItems();
  bool anyItemsSelected = item[0] != nullptr || item[1] != nullptr;

  // The x position of the split (if splitting)
  int xSplit = int(drawArea_botR.x() * splittingPoint);

  // Calculate the zoom to use
  double zoom = zoomFactor * currentStepScaleFactor;
  QPoint offset = QPointF(QPointF(centerOffset) * currentStepScaleFactor + currentStepCenterPointOffset).toPoint();

  bool drawRawValues = showRawData() && !playing;

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
    centerPoints[0] = QPoint(xSplit / 2, y);
    centerPoints[1] = QPoint(xSplit + (drawArea_botR.x() - xSplit) / 2, y);
  }

  // For the zoom box, calculate the pixel position under the cursor for each view. The following
  // things are calculated in this function:
  bool    pixelPosInItem[2] = {false, false};  //< Is the pixel position under the cursor within the item?
  QRect   zoomPixelRect[2];                    //< A QRect around the pixel that is under the cursor
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

      // Mark the pixel under the cursor with a rectangle around it.
      if (pixelPosInItem[view])
      {
        int pixelPoint[2];
        pixelPoint[0] = -((itemSize[0] / 2 - zoomBoxPixelUnderCursor[view].x()) * zoom);
        pixelPoint[1] = -((itemSize[1] / 2 - zoomBoxPixelUnderCursor[view].y()) * zoom);
        zoomPixelRect[view] = QRect(pixelPoint[0], pixelPoint[1], zoom, zoom);
      }
    }
  }

  if (splitting)
  {
    // Draw two items (or less, if less items are selected)
    if (item[0])
    {
      // Set clipping to the left region
      QRegion clipping = QRegion(0, 0, xSplit, drawArea_botR.y());
      painter.setClipRegion(clipping);

      // Translate the painter to the position where we want the item to be
      painter.translate(centerPoints[0] + offset);

      // Draw the item at position (0,0)
      if (!waitingForCaching)
      {
        painter.setFont(QFont(SPLITVIEWWIDGET_PIXEL_VALUES_FONT, SPLITVIEWWIDGET_PIXEL_VALUES_FONTSIZE));
        item[0]->drawItem(&painter, frame, zoom, drawRawValues);
      }

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[0]);

      if (pixelPosInItem[0])
      {
        // If the zoom box is active, draw a rectangle around the pixel currently under the cursor
        frameHandler *vid = item[0]->getFrameHandler();
        if (vid)
        {
          painter.setPen(vid->isPixelDark(zoomBoxPixelUnderCursor[0]) ? Qt::white : Qt::black);
          painter.drawRect(zoomPixelRect[0]);
        }
      }

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(0, painter, xSplit, drawArea_botR, item[0], frame, zoomBoxPixelUnderCursor[0], pixelPosInItem[0], zoom, playing);

      // Paint the x pixel values ruler at the top
      paintPixelRulersX(painter, item[0], 0, xSplit, zoom, centerPoints[0], offset);
      paintPixelRulersY(painter, item[0], drawArea_botR.y(), 0 , zoom, centerPoints[0], offset);

      // Draw the "loading" message (if needed)
      drawingLoadingMessage[0] = (!playing && item[0]->isLoading());
      if (drawingLoadingMessage[0])
        drawLoadingMessage(&painter, QPoint(xSplit / 2, drawArea_botR.y() / 2));
    }
    if (item[1])
    {
      // Set clipping to the right region
      QRegion clipping = QRegion(xSplit, 0, drawArea_botR.x() - xSplit, drawArea_botR.y());
      painter.setClipRegion(clipping);

      // Translate the painter to the position where we want the item to be
      painter.translate(centerPoints[1] + offset);

      // Draw the item at position (0,0)
      if (!waitingForCaching)
      {
        painter.setFont(QFont(SPLITVIEWWIDGET_PIXEL_VALUES_FONT, SPLITVIEWWIDGET_PIXEL_VALUES_FONTSIZE));
        item[1]->drawItem(&painter, frame, zoom, drawRawValues);
      }

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[1]);

      if (pixelPosInItem[1])
      {
        // If the zoom box is active, draw a rectangle around the pixel currently under the cursor
        frameHandler *vid = item[1]->getFrameHandler();
        if (vid)
        {
          painter.setPen(vid->isPixelDark(zoomBoxPixelUnderCursor[1]) ? Qt::white : Qt::black);
          painter.drawRect(zoomPixelRect[1]);
        }
      }

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(1, painter, xSplit, drawArea_botR, item[1], frame, zoomBoxPixelUnderCursor[1], pixelPosInItem[1], zoom, playing);

      // Paint the x pixel values ruler at the top
      paintPixelRulersX(painter, item[1], xSplit, drawArea_botR.x(), zoom, centerPoints[1], offset);
      // Paint another y ruler at the split line if the resolution in Y direction for the two items is not identical.
      if (item[0]->getSize().height() != item[1]->getSize().height())
        paintPixelRulersY(painter, item[1], drawArea_botR.y(), xSplit, zoom, centerPoints[1], offset);

      // Draw the "loading" message (if needed)
      drawingLoadingMessage[1] = (!playing && item[1]->isLoading());
      if (drawingLoadingMessage[1])
        drawLoadingMessage(&painter, QPoint(xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2));
    }

    // Disable clipping
    painter.setClipping(false);
  }
  else // (!splitting)
  {
    // Draw one item (if one item is selected)
    if (item[0])
    {
      centerPoints[0] = drawArea_botR / 2;

      // Translate the painter to the position where we want the item to be
      painter.translate(centerPoints[0] + offset);

      // Draw the item at position (0,0)
      if (!waitingForCaching)
      {
        painter.setFont(QFont(SPLITVIEWWIDGET_PIXEL_VALUES_FONT, SPLITVIEWWIDGET_PIXEL_VALUES_FONTSIZE));
        item[0]->drawItem(&painter, frame, zoom, drawRawValues);
      }

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[0]);

      if (pixelPosInItem[0])
      {
        // If the zoom box is active, draw a rectangle around the pixel currently under the cursor
        frameHandler *vid = item[0]->getFrameHandler();
        if (vid)
        {
          painter.setPen(vid->isPixelDark(zoomBoxPixelUnderCursor[0]) ? Qt::white : Qt::black);
          painter.drawRect(zoomPixelRect[0]);
        }
      }

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(0, painter, xSplit, drawArea_botR, item[0], frame, zoomBoxPixelUnderCursor[0], pixelPosInItem[0], zoom, playing);

      // Paint the x pixel values ruler at the top
      paintPixelRulersX(painter, item[0], 0, drawArea_botR.x(), zoom, centerPoints[0], offset);
      paintPixelRulersY(painter, item[0], drawArea_botR.y(), 0 , zoom, centerPoints[0], offset);

      // Draw the "loading" message (if needed)
      drawingLoadingMessage[0] = (!playing && item[0]->isLoading());
      if (drawingLoadingMessage[0])
        drawLoadingMessage(&painter, centerPoints[0]);
    }
  }

  if (splitting)
  {
    if (splittingLineStyle == TOP_BOTTOM_HANDLERS)
    {
      // Draw small handlers at the top and bottom
      QPainterPath triangle;
      triangle.moveTo(xSplit-10, 0 );
      triangle.lineTo(xSplit   , 10);
      triangle.lineTo(xSplit+10,  0);
      triangle.closeSubpath();

      triangle.moveTo(xSplit-10, drawArea_botR.y());
      triangle.lineTo(xSplit   , drawArea_botR.y() - 10);
      triangle.lineTo(xSplit+10, drawArea_botR.y());
      triangle.closeSubpath();

      painter.fillPath(triangle, Qt::white);
    }
    else
    {
      // Draw the splitting line at position xSplit. All pixels left of the line
      // belong to the left view, and all pixels on the right belong to the right one.
      QLine line(xSplit, 0, xSplit, drawArea_botR.y());
      painter.setPen(Qt::white);
      painter.drawLine(line);
    }
  }

  if (viewZooming)
  {
    // Draw the zoom rectangle. Draw black rectangle, then a white dashed/dotted one.
    // This is visible in dark and bright areas
    if (splitting && viewMode == SIDE_BY_SIDE)
    {
      // Only draw the zoom rectangle in the view that it was started in
      if ((viewZoomingMousePosStart.x() < xSplit && viewZoomingMousePos.x() >= xSplit) ||
        (viewZoomingMousePosStart.x() >= xSplit && viewZoomingMousePos.x() < xSplit))
        viewZoomingMousePos.setX(xSplit);
    }
    painter.setPen(QPen(Qt::black));
    painter.drawRect(QRect(viewZoomingMousePosStart, viewZoomingMousePos));
    painter.setPen(QPen(Qt::white, 1, Qt::DashDotDotLine));
    painter.drawRect(QRect(viewZoomingMousePosStart, viewZoomingMousePos));
  }

  if (zoom != 1.0)
  {
    // Draw the zoom factor
    QString zoomString = QString("x") + QString::number(zoom, 'g', (zoom < 0.5) ? 4 : 2);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(QColor(Qt::black));
    painter.setFont(zoomFactorFont);
    painter.drawText(zoomFactorFontPos, zoomString);
  }

  if (playback->isWaitingForCaching())
  {
    // The playback is halted because we are waiting for the caching of the next item.
    // Draw a small indicator on the bottom left
    QPoint pos = QPoint(10, drawArea_botR.y() - 10 - waitingForCachingPixmap.height());
    painter.drawPixmap(pos, waitingForCachingPixmap);
  }

  // Update the mouse cursor
  updateMouseCursor();

  if (testMode)
  {
    if (testLoopCount < 0)
      testFinished(false);
    else
    {
      testLoopCount--;
      update();
    }
  }
}

void splitViewWidget::updatePixelPositions()
{
  // Get the selected item(s)
  auto item = playlist->getSelectedItems();
  bool anyItemsSelected = item[0] != nullptr || item[1] != nullptr;

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
    centerPoints[0] = QPoint(xSplit / 2, y);
    centerPoints[1] = QPoint(xSplit + (drawArea_botR.x() - xSplit) / 2, y);
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
        // Divide and round. We want value from 0...-1 to be quantized to -1 and not 0
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

void splitViewWidget::paintZoomBox(int view, QPainter &painter, int xSplit, const QPoint &drawArea_botR, playlistItem *item, int frame, const QPoint &pixelPos, bool pixelPosInItem, double zoomFactor, bool playing)
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

  bool drawInfoPanel = !playing;  // Do we draw the info panel?
  if (view == 1 && xSplit > (drawArea_botR.x() - margin - zoomBoxSize))
  {
    if (xSplit > drawArea_botR.x() - margin)
      // The split line is so far on the right, that the whole zoom box in view 1 is not visible
      return;

    // The split line is so far right, that part of the zoom box is hidden.
    // Resize the zoomViewRect to the part that is visible.
    zoomViewRect.setWidth(drawArea_botR.x() - xSplit - margin);

    drawInfoPanel = false;  // Info panel not visible
  }

  // Do not draw the zoom view if the zoomFactor is equal or greater than that of the zoom box
  if (zoomFactor < zoomBoxFactor)
  {
    if (view == 0 && splitting)
      zoomViewRect.moveBottomRight(QPoint(xSplit - margin, drawArea_botR.y() - margin));
    else
      zoomViewRect.moveBottomRight(drawArea_botR - QPoint(margin, margin));

    // Fill the viewRect with the background color
    painter.setPen(Qt::black);
    painter.fillRect(zoomViewRect, painter.background());

    // Restrict drawing to the zoom view rectangle. Save the old clipping region (if any) so we can reset it later
    QRegion clipRegion;
    if (painter.hasClipping())
      clipRegion = painter.clipRegion();
    painter.setClipRegion(zoomViewRect);

    // Translate the painter to the point where the center of the zoom view will be
    painter.translate(zoomViewRect.center());

    // Now we have to calculate the translation of the item, so that the pixel position
    // is in the center of the view (so we can draw it at (0,0)).
    QPointF itemZoomBoxTranslation = QPointF(item->getSize().width()  / 2 - pixelPos.x() - 0.5,
                                             item->getSize().height() / 2 - pixelPos.y() - 0.5);
    painter.translate(itemZoomBoxTranslation * zoomBoxFactor);

    // Draw the item again, but this time with a high zoom factor into the clipped region
    // Never draw the raw values in the zoom box.
    item->drawItem(&painter, frame, zoomBoxFactor, false);

    // Reset transform and reset clipping to the previous clip region (if there was one)
    painter.resetTransform();
    if (clipRegion.isEmpty())
      painter.setClipping(false);
    else
      painter.setClipRegion(clipRegion);

    // Draw a rectangle around the zoom view
    painter.drawRect(zoomViewRect);
  }
  else
    // If we don't draw the zoom box, consider the size to be 0.
    zoomBoxSize = 0;

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
      if(pixelListSets.size() > 0)
        for (int i = 0; i < pixelListSets.count(); i++)
        {
          QString title = pixelListSets[i].first;
          QStringPairList pixelValues = pixelListSets[i].second;
          pixelInfoString.append(QString("<h4>%1</h4><table width=\"100%\">").arg(title));
          for (int j = 0; j < pixelValues.size(); ++j)
            pixelInfoString.append(QString("<tr><td><nobr>%1:</nobr></td><td align=\"right\"><nobr>%2</nobr></td></tr>").arg(pixelValues[j].first).arg(pixelValues[j].second));
          pixelInfoString.append("</table>");
        }
    }

    // Create a QTextDocument. This object can tell us the size of the rendered text.
    QTextDocument textDocument;
    textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
    textDocument.setHtml(pixelInfoString);
    textDocument.setTextWidth(textDocument.size().width());

    // Translate to the position where the text box shall be
    if (view == 0 && splitting)
      painter.translate(xSplit - margin - zoomBoxSize - textDocument.size().width() - padding*2 + 1, drawArea_botR.y() - margin - textDocument.size().height() - padding*2 + 1);
    else
      painter.translate(drawArea_botR.x() - margin - zoomBoxSize - textDocument.size().width() - padding*2 + 1, drawArea_botR.y() - margin - textDocument.size().height() - padding*2 + 1);

    // Draw a black rectangle and then the text on top of that
    QRect rect(QPoint(0, 0), textDocument.size().toSize() + QSize(2*padding, 2*padding));
    QBrush originalBrush;
    painter.setBrush(QColor(0, 0, 0, 70));
    painter.setPen(Qt::black);
    painter.drawRect(rect);
    painter.translate(padding, padding);
    textDocument.drawContents(&painter);
    painter.setBrush(originalBrush);

    painter.resetTransform();
  }
}

void splitViewWidget::paintRegularGrid(QPainter *painter, playlistItem *item)
{
  QSize itemSize = item->getSize() * zoomFactor;
  painter->setPen(regularGridColor);

  // Draw horizontal lines
  const int xMin = -itemSize.width() / 2;
  const int xMax =  itemSize.width() / 2;
  const double gridZoom = regularGridSize * zoomFactor;
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

void splitViewWidget::paintPixelRulersX(QPainter &painter, playlistItem *item, int xPixMin, int xPixMax, double zoom, QPoint centerPoints, QPoint offset)
{
  if (zoom < 32)
    return;

  // Set the font for drawing the values
  QFont valueFont = QFont(SPLITVIEWWIDGET_ZOOMFACTOR_FONT, 10);
  painter.setFont(valueFont);

  // Get the pixel values that are visible on screen
  QSize frameSize = item->getSize();
  QSize videoRect = frameSize * zoom;
  QPoint worldTransform = centerPoints + offset;
  int xMin = (videoRect.width() / 2 - worldTransform.x() - xPixMin) / zoom;
  int xMax = (videoRect.width() / 2 - (worldTransform.x() - xPixMax)) / zoom;
  xMin = clip(xMin, 0, frameSize.width());
  xMax = clip(xMax, 0, frameSize.width());

  // Draw the X Pixel indicators on the top
  for (int x = xMin; x < xMax+1; x++)
  {
    // Where is the x position of the pixel in the item on screen?
    int xPosOnScreen = x * zoom - videoRect.width() / 2 + worldTransform.x();
    painter.setPen(QPen(Qt::white));
    painter.drawLine(xPosOnScreen, 0, xPosOnScreen, 5);
    painter.setPen(QPen(Qt::black));
    painter.drawLine(xPosOnScreen+1, 0, xPosOnScreen+1, 5);

    // Draw the values (every fifth value, all values for zoom >= 128)
    if ((zoom >= 128 || x % 5 == 0) && x != frameSize.width())
    {
      QString numberText = QString::number(x);

      // How large will the drawn text be?
      QFontMetrics metrics(valueFont);
      QSize rectSize = metrics.size(0, numberText) + QSize(4, 0);
      QPoint rectPosTopLeft(xPosOnScreen + zoom / 2 - rectSize.width() / 2, 2);
      QRect textRect(rectPosTopLeft, rectSize);

      // Draw a white rect ...
      painter.fillRect(textRect, Qt::white);
      // ... and the text
      painter.setPen(QPen(Qt::black));
      painter.drawText(textRect, Qt::AlignCenter, numberText);
    }
  }
}

void splitViewWidget::paintPixelRulersY(QPainter &painter, playlistItem *item, int yPixMax, int xPos, double zoom, QPoint centerPoints, QPoint offset)
{
  if (zoom < 32)
    return;

  // Set the font for drawing the values
  QFont valueFont = QFont(SPLITVIEWWIDGET_ZOOMFACTOR_FONT, 10);
  painter.setFont(valueFont);

  QSize frameSize = item->getSize();
  QSize videoRect = frameSize * zoom;
  QPoint worldTransform = centerPoints + offset;

  // Get the pixel values that are visible on screen
  int yMin = (videoRect.height() / 2 - worldTransform.y()) / zoom;
  int yMax = (videoRect.height() / 2 - (worldTransform.y() - yPixMax)) / zoom;
  yMin = clip(yMin, 0, frameSize.height());
  yMax = clip(yMax, 0, frameSize.height());

  // Draw pixel indicatoes on the left
  for (int y = yMin; y < yMax+1; y++)
  {
    int yPosOnScreen = y * zoom - videoRect.height() / 2 + worldTransform.y();
    painter.setPen(QPen(Qt::white));
    painter.drawLine(xPos, yPosOnScreen, xPos + 5, yPosOnScreen);
    painter.setPen(QPen(Qt::black));
    painter.drawLine(xPos, yPosOnScreen+1, xPos + 5, yPosOnScreen+1);

    // Draw the values (every fifth value, all values for zoom >= 128)
    if ((zoom >= 128 || y % 5 == 0) && y != frameSize.height())
    {
      QString numberText = QString::number(y);

      // How large will the drawn text be?
      QFontMetrics metrics(valueFont);
      QSize rectSize = metrics.size(0, numberText) + QSize(4, 0);
      QPoint rectPosTopLeft(xPos + 2, yPosOnScreen + zoom / 2 - rectSize.height() / 2);
      QRect textRect(rectPosTopLeft, rectSize);

      // Draw a white rect ...
      painter.fillRect(textRect, Qt::white);
      // ... and the text
      painter.setPen(QPen(Qt::black));
      painter.drawText(textRect, Qt::AlignCenter, numberText);
    }
  }
}

void splitViewWidget::drawLoadingMessage(QPainter *painter, const QPoint &pos)
{
  DEBUG_LOAD_DRAW("splitViewWidget::drawLoadingMessage");

  // Set the font for drawing the values
  QFont valueFont = QFont(SPLITVIEWWIDGET_LOADING_FONT, SPLITVIEWWIDGET_LOADING_FONTSIZE);
  painter->setFont(valueFont);

  // Draw the message at centerPoints[0]
  QFontMetrics metrics(painter->font());
  QSize textSize = metrics.size(0, SPLITVIEWWIDGET_LOADING_TEXT);

  // Create the QRect to draw to
  QRect textRect;
  textRect.setSize(textSize);
  textRect.moveCenter(pos);

  // Draw a rectangle around the text in white with a black border
  QRect boxRect = textRect + QMargins(5, 5, 5, 5);
  painter->setPen(QPen(Qt::black, 1));
  painter->fillRect(boxRect,Qt::white);
  painter->drawRect(boxRect);

  // Draw the text
  painter->drawText(textRect, Qt::AlignCenter, SPLITVIEWWIDGET_LOADING_TEXT);
}

void splitViewWidget::mouseMoveEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->source() == Qt::MouseEventSynthesizedBySystem && currentlyPinching)
    // The mouse event was generated by the system from a touch event which is already handled by the touch pinch handler.
    return;

  if (mouse_event->buttons() == Qt::NoButton)
  {
    // The mouse is moved, but no button is pressed. This should not be caught here. Maybe a mouse press/release event
    // got lost somewhere. In this case go to the normal mode.
    if (splitting && splittingDragging)
      // End dragging.
      splittingDragging = false;
    else if (viewDragging)
      // End dragging
      viewDragging = false;
    else if (viewZooming)
      viewZooming = false;
  }

  // We want this event
  mouse_event->accept();

  if (splitting && splittingDragging)
  {
    // The user is currently dragging the splitter. Calculate the new splitter point.
    int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2- SPLITVIEWWIDGET_SPLITTER_CLIPX));
    setSplittingPoint((double)xClip / (double)(width()-2));

    // The splitter was moved. Update the widget.
    update();
  }
  else if (viewDragging)
  {
    // The user is currently dragging the view. Calculate the new offset from the center position
    setCenterOffset(viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart));

    // The view was moved. Update the widget.
    update();
  }
  else if (viewZooming)
  {
    // The user is currently using the mouse to zoom. Save the current mouse position so that we can draw a zooming rectangle.
    viewZoomingMousePos = mouse_event->pos();

    // Update the view to draw the zoom box.
    update();
  }
  else
    updateMouseCursor(mouse_event->pos());

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
  if (mouse_event->source() == Qt::MouseEventSynthesizedBySystem && currentlyPinching)
    // The mouse event was generated by the system from a touch event which is already handled by the touch pinch handler.
    return;

  if (isViewFrozen)
    return;

  // Are we over the split line?
  int splitPosPix = int((width()-2) * splittingPoint);
  bool mouseOverSplitLine = false;
  if (splitting)
  {
    // Calculate the margin of the split line according to the display DPI.
    int margin = logicalDpiX() / SPLITVIEWWIDGET_SPLITTER_MARGIN_DPI_DIV;
    mouseOverSplitLine = (mouse_event->x() > (splitPosPix-margin) && mouse_event->x() < (splitPosPix+margin));
  }

  if (mouse_event->button() == Qt::LeftButton && mouseOverSplitLine)
  {
    // Left mouse buttons pressed over the split line. Activate dragging of splitter.
    splittingDragging = true;

    // We handled this event
    mouse_event->accept();
  }
  else if ((mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_RIGHT_MOVE))
  {
    // The user pressed the 'move' mouse button. In this case drag the view.
    viewDragging = true;

    // Save the position where the user grabbed the item (screen), and the current value of
    // the centerOffset. So when the user moves the mouse, the new offset is just the old one
    // plus the difference between the position of the mouse and the position where the
    // user grabbed the item (screen).
    viewDraggingMousePosStart = mouse_event->pos();
    viewDraggingStartOffset = centerOffset;

    //qDebug() << "MouseGrab - Center: " << centerPoint << " rel: " << grabPosRelative;

    // We handled this event
    mouse_event->accept();
  }
  else if ((mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_RIGHT_MOVE))
  {
    // The user pressed the 'zoom' mouse button. In this case start drawing the zoom box.
    viewZooming = true;

    // Save the position of the mouse where the user started the zooming.
    viewZoomingMousePosStart = mouse_event->pos();
    viewZoomingMousePos = viewZoomingMousePosStart;

    // We handled this event
    mouse_event->accept();
  }

  updateMouseCursor(mouse_event->pos());
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
    setSplittingPoint((double)xClip / (double)(width()-2));

    // The view was moved. Update the widget.
    update();
  }
  else if (viewDragging && (
           (mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_RIGHT_MOVE)))
  {
    // The user released the mouse 'move' button and was dragging the view.

    // We want this event
    mouse_event->accept();

    // Calculate the new center offset one last time
    setCenterOffset(viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart));

    // The view was moved. Update the widget.
    update();

    // End dragging
    viewDragging = false;
  }
  else if (viewZooming && (
           (mouse_event->button() == Qt::RightButton  && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::LeftButton && mouseMode == MOUSE_RIGHT_MOVE)))
  {
    // The user used the mouse to zoom. End this operation.

    // We want this event
    mouse_event->accept();

    // Zoom so that the whole rectangle is visible and center it in the view.
    QRect zoomRect = QRect(viewZoomingMousePosStart, mouse_event->pos());
    if (abs(zoomRect.width()) < 2 && abs(zoomRect.height()) < 2)
    {
      // The user just pressed the button without moving the mouse.
      viewZooming = false;
      update();
      return;
    }

    // Get the absolute center point of the view
    QPoint drawArea_botR(width(), height());
    QPoint centerPoint = drawArea_botR / 2;

    if (splitting && viewMode == SIDE_BY_SIDE)
    {
      // For side by side mode, the center points are centered in each individual split view

      // Which side of the split view are we zooming in?
      // Get the center point of that view
      int xSplit = int(drawArea_botR.x() * splittingPoint);
      if (viewZoomingMousePosStart.x() >= xSplit)
        // Zooming in the right view
        centerPoint = QPoint(xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2);
      else
        // Zooming in the left view
        centerPoint = QPoint(xSplit / 2, drawArea_botR.y() / 2);
    }

    // Calculate the new center offset
    QPoint zoomRectCenterOffset = zoomRect.center() - centerPoint;
    setCenterOffset(centerOffset - zoomRectCenterOffset);

    // Now we zoom in as far as possible
    double additionalZoomFactor = 1.0;
    while (abs(zoomRect.width())  * additionalZoomFactor * SPLITVIEWWIDGET_ZOOM_STEP_FACTOR <= width() &&
           abs(zoomRect.height()) * additionalZoomFactor * SPLITVIEWWIDGET_ZOOM_STEP_FACTOR <= height())
    {
      // We can zoom in more
      setZoomFactor(zoomFactor * SPLITVIEWWIDGET_ZOOM_STEP_FACTOR);
      if (controls.zoomFactorSpinBox)
      {
        const QSignalBlocker block(controls.zoomFactorSpinBox);
        controls.zoomFactorSpinBox->setValue(int(zoomFactor * 100));
      }
      additionalZoomFactor *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
      setCenterOffset(centerOffset * SPLITVIEWWIDGET_ZOOM_STEP_FACTOR);
    }

    // End zooming
    viewZooming = false;

    // The view was moved. Update the widget.
    update();
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

bool splitViewWidget::event(QEvent *event)
{
  if (event->type() == QEvent::Gesture)
  {
    QGestureEvent *gestureEvent = static_cast<QGestureEvent*>(event);

    // Handle the gesture event
    if (QGesture *swipeGesture = gestureEvent->gesture(Qt::SwipeGesture))
    {
      QSwipeGesture *swipe = static_cast<QSwipeGesture*>(swipeGesture);

      if (swipe->state() == Qt::GestureStarted)
        // The gesture was just started. This will prevent (generated) mouse events from being interpreted.
        currentlyPinching = true;

      if (swipe->state() == Qt::GestureFinished)
      {
        if (swipe->horizontalDirection() == QSwipeGesture::NoDirection && swipe->verticalDirection() == QSwipeGesture::Up)
          playlist->selectNextItem();
        else if (swipe->horizontalDirection() == QSwipeGesture::NoDirection && swipe->verticalDirection() == QSwipeGesture::Down)
          playlist->selectPreviousItem();
        else if (swipe->horizontalDirection() == QSwipeGesture::Left && swipe->verticalDirection() == QSwipeGesture::NoDirection)
          playback->nextFrame();
        else if (swipe->horizontalDirection() == QSwipeGesture::Right && swipe->verticalDirection() == QSwipeGesture::NoDirection)
          playback->previousFrame();
        else
        {
          // The swipe was both horizontal and vertical. What is the dominating direction?
          double a = swipe->swipeAngle();
          if (a < 45 || a > 315)
            playback->previousFrame();      // Right
          else if (a >= 45 && a < 135)
            playlist->selectNextItem();     // Up
          else if (a >= 135 && a < 225)
            playback->nextFrame();          // Left
          else
            playlist->selectPreviousItem(); // Down
        }

        currentlyPinching = false;
      }

      event->accept();
      update();
    }
    if (QGesture *pinchGesture = gestureEvent->gesture(Qt::PinchGesture))
    {
      QPinchGesture *pinch = static_cast<QPinchGesture*>(pinchGesture);

      if (pinch->state() == Qt::GestureStarted)
      {
        // The gesture was just started. This will prevent (generated) mouse events from being interpreted.
        currentlyPinching = true;
      }

      // See what changed in this pinch gesture (the scale factor and/or the position)
      QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();
      if (changeFlags & QPinchGesture::ScaleFactorChanged)
        currentStepScaleFactor = pinch->totalScaleFactor();
      if (changeFlags & QPinchGesture::CenterPointChanged)
        currentStepCenterPointOffset += pinch->centerPoint() - pinch->lastCenterPoint();

      // Check if the gesture just finished
      if (pinch->state() == Qt::GestureFinished)
      {
        // Set the new position/zoom
        setZoomFactor(zoomFactor * currentStepScaleFactor);
        setCenterOffset(QPointF(QPointF(centerOffset) * currentStepScaleFactor + currentStepCenterPointOffset).toPoint());
        if (controls.zoomFactorSpinBox)
        {
          const QSignalBlocker block(controls.zoomFactorSpinBox);
          controls.zoomFactorSpinBox->setValue(int(zoomFactor * 100));
        }

        // Reset the dynamic values
        currentStepScaleFactor = 1;
        currentStepCenterPointOffset = QPointF(0, 0);
        currentlyPinching = false;
      }

      if (linkViews)
      {
        otherWidget->currentlyPinching = currentlyPinching;
        otherWidget->currentStepScaleFactor = currentStepScaleFactor;
        otherWidget->currentStepCenterPointOffset = currentStepCenterPointOffset;
      }

      event->accept();
      update();
    }

    return true;
  }
  return QWidget::event(event);
}

void splitViewWidget::updateMouseCursor()
{
  updateMouseCursor(mapFromGlobal(QCursor::pos()));
}

void splitViewWidget::setCenterOffset(QPoint offset)
{
  centerOffset = offset;
  if (linkViews)
    otherWidget->centerOffset = offset;

  // Save the center offset in the currently selected item
  auto item = playlist->getSelectedItems();
  if (item[0])
  {
    DEBUG_LOAD_DRAW("splitViewWidget::setCenterOffset item %d (%d,%d)", item[0]->getID(), offset.x(), offset.y());
    item[0]->saveCenterOffset(centerOffset, isSeparateWidget);
    item[0]->saveCenterOffset(otherWidget->centerOffset, !isSeparateWidget);
  }
  if (item[1])
  {
    DEBUG_LOAD_DRAW("splitViewWidget::setCenterOffset item %d (%d,%d)", item[1]->getID(), offset.x(), offset.y());
    item[1]->saveCenterOffset(centerOffset, isSeparateWidget);
    item[1]->saveCenterOffset(otherWidget->centerOffset, !isSeparateWidget);
  }
}

void splitViewWidget::setSplittingPoint(double point)
{
  splittingPoint = point;
  if (linkViews)
    otherWidget->splittingPoint = point;
}

void splitViewWidget::setZoomFactor(double zoom)
{
  zoomFactor = zoom;
  if (linkViews)
    otherWidget->zoomFactor = zoom;

  // Save the zoom factor in the currently selected item
  auto item = playlist->getSelectedItems();
  if (item[0])
  {
    DEBUG_LOAD_DRAW("splitViewWidget::setZoomFactor item %d (%f)", item[0]->getID(), zoom);
    item[0]->saveZoomFactor(zoomFactor, isSeparateWidget);
    item[0]->saveZoomFactor(otherWidget->zoomFactor, !isSeparateWidget);
  }
  if (item[1])
  {
    DEBUG_LOAD_DRAW("splitViewWidget::setZoomFactor item %d (%f)", item[0]->getID(), zoom);
    item[1]->saveZoomFactor(zoomFactor, isSeparateWidget);
    item[1]->saveZoomFactor(otherWidget->zoomFactor, !isSeparateWidget);
  }
}

void splitViewWidget::updateMouseCursor(const QPoint &mousePos)
{
  // Check if the position is within the widget
  if (mousePos.x() < 0 || mousePos.x() > width() || mousePos.y() < 0 || mousePos.y() > height())
    return;

  if (viewDragging)
    // Dragging the view around. Show the closed hand cursor.
    setCursor(Qt::ClosedHandCursor);
  else if (viewZooming || isViewFrozen)
    // Drawing the zoom box or the view is frozen. Show the normal cursor.
    setCursor(Qt::ArrowCursor);
  else
  {
    // Not dragging or zooming. Show the normal cursor.

    // Get the item(s)
    auto item = playlist->getSelectedItems();

    if (splitting)
    {
      // Get the splitting line position
      int splitPosPix = int((width()-2) * splittingPoint);
      // Calculate the margin of the split line according to the display DPI.
      int margin = logicalDpiX() / SPLITVIEWWIDGET_SPLITTER_MARGIN_DPI_DIV;

      if (mousePos.x() > (splitPosPix-margin) && mousePos.x() < (splitPosPix+margin))
        // Mouse is over the line in the middle (plus minus 4 pixels)
        setCursor(Qt::SplitHCursor);
      else if ((mousePos.x() < splitPosPix && item[0] && item[0]->isLoading()) || (mousePos.x() > splitPosPix && item[1] && item[1]->isLoading()))
        // Mouse is not over the splitter line but the item that the mouse is currently over is loading.
        setCursor(Qt::BusyCursor);
      else
        if (mouseMode == MOUSE_LEFT_MOVE)
          setCursor(Qt::OpenHandCursor);
        else
          setCursor(Qt::ArrowCursor);
    }
    else
    {
      if (item[0] && item[0]->isLoading())
        // The mouse is over an item that is currently loading.
        setCursor(Qt::BusyCursor);
      else
        if (mouseMode == MOUSE_LEFT_MOVE)
          setCursor(Qt::OpenHandCursor);
        else
          setCursor(Qt::ArrowCursor);
    }
  }
}

void splitViewWidget::zoomIn(const QPoint &zoomPoint)
{
  // The zoom point works like this: After the zoom operation the pixel at zoomPoint shall
  // still be at the same position (zoomPoint)

  // What is the factor that we will zoom in by?
  // The zoom factor could currently not be a multiple of SPLITVIEWWIDGET_ZOOM_STEP_FACTOR
  // if the user used pinch zoom. So let's go back to the step size of SPLITVIEWWIDGET_ZOOM_STEP_FACTOR
  // and calculate the next higher zoom which is a multiple of SPLITVIEWWIDGET_ZOOM_STEP_FACTOR.
  // E.g.: If the zoom factor currently is 1.9 we want it to become 2 after zooming.
  double newZoom = 1.0;
  if (zoomFactor > 1.0)
  {
    double inf = std::numeric_limits<double>::infinity();
    while (newZoom <= zoomFactor && newZoom < inf)
      newZoom *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }
  else
  {
    while (newZoom > zoomFactor)
      newZoom /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
    newZoom *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }
  // So what is the zoom factor that we use in this step?
  double stepZoomFactor = newZoom / zoomFactor;

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
        centerPoint = QPoint(xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2);
      else
        // Zooming in the left view
        centerPoint = QPoint(xSplit / 2, drawArea_botR.y() / 2);
    }

    // The absolute center point of the item under the cursor
    QPoint itemCenter = centerPoint + centerOffset;

    // Move this item center point
    QPoint diff = itemCenter - zoomPoint;
    diff *= stepZoomFactor;
    itemCenter = zoomPoint + diff;

    // Calculate the new center offset
    setCenterOffset(itemCenter - centerPoint);
  }
  else
  {
    // Zoom in without considering the mouse position
    setCenterOffset(centerOffset * stepZoomFactor);
  }

  setZoomFactor(newZoom);
  if (controls.zoomFactorSpinBox)
  {
    const QSignalBlocker block(controls.zoomFactorSpinBox);
    controls.zoomFactorSpinBox->setValue(int(zoomFactor * 100));
  }
  update(false, true);  // We zoomed in. Check if one of the items now needs loading
}

void splitViewWidget::zoomOut(const QPoint &zoomPoint)
{
  // What is the factor that we will zoom out by?
  // The zoom factor could currently not be a multiple of SPLITVIEWWIDGET_ZOOM_STEP_FACTOR
  // if the user used pinch zoom. So let's go back to the step size of SPLITVIEWWIDGET_ZOOM_STEP_FACTOR
  // and calculate the next higher zoom which is a multiple of SPLITVIEWWIDGET_ZOOM_STEP_FACTOR.
  // E.g.: If the zoom factor currently is 2.1 we want it to become 2 after zooming.
  double newZoom = 1.0;
  if (zoomFactor > 1.0)
  {
    while (newZoom < zoomFactor)
      newZoom *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
    newZoom /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }
  else
  {
    while (newZoom >= zoomFactor && newZoom > 0.0)
      newZoom /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }
  // So what is the zoom factor that we use in this step?
  double stepZoomFactor = newZoom / zoomFactor;

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
        centerPoint = QPoint(xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2);
      else
        // Zooming in the left view
        centerPoint = QPoint(xSplit / 2, drawArea_botR.y() / 2);
    }

    // The absolute center point of the item under the cursor
    QPoint itemCenter = centerPoint + centerOffset;

    // Move this item center point
    QPoint diff = itemCenter - zoomPoint;
    diff *= stepZoomFactor;
    itemCenter = zoomPoint + diff;

    // Calculate the new center offset
    setCenterOffset(itemCenter - centerPoint);
  }
  else
  {
    // Zoom out without considering the mouse position.
    setCenterOffset(centerOffset * stepZoomFactor);
  }

  setZoomFactor(newZoom);
  if (controls.zoomFactorSpinBox)
  {
    const QSignalBlocker block(controls.zoomFactorSpinBox);
    controls.zoomFactorSpinBox->setValue(int(zoomFactor * 100));
  }
  update();
}

void splitViewWidget::on_zoomFactorSpinBox_valueChanged(int val)
{
  double newZoom = double(val) / 100;
  if (newZoom < 0.001)
    newZoom = 0.001;
  setZoomFactor(newZoom);
  update();
}

void splitViewWidget::resetViews()
{
  setCenterOffset(QPoint(0,0));
  setZoomFactor(1.0);
  setSplittingPoint(0.5);

  update();
}

void splitViewWidget::zoomToFit()
{
  if (!playlist)
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;

  setCenterOffset(QPoint(0,0));

  auto item = playlist->getSelectedItems();

  if (item[0] == nullptr)
    // We cannot zoom to anything
    return;

  double fracZoom = 1.0;
  if (!splitting)
  {
    // Get the size of item 0 and the size of the widget and set the zoom factor so that this fits
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
        virtualItemSize.setWidth(item1Size.width());
      if (item1Size.height() > virtualItemSize.height())
        virtualItemSize.setHeight(item1Size.height());
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
  setZoomFactor(newZoomFactor);
  if (controls.zoomFactorSpinBox)
  {
    const QSignalBlocker block(controls.zoomFactorSpinBox);
    controls.zoomFactorSpinBox->setValue(int(zoomFactor * 100));
  }
  update();
}

void splitViewWidget::setupControls(QDockWidget *dock)
{
  // Initialize the controls and add them to the given widget.
  QWidget *controlsWidget = new QWidget(dock);
  controls.setupUi(controlsWidget);
  dock->setWidget(controlsWidget);

  // Connect signals/slots
  connect(controls.SplitViewgroupBox, &QGroupBox::toggled, this, &splitViewWidget::on_SplitViewgroupBox_toggled);
  connect(controls.viewComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &splitViewWidget::on_viewComboBox_currentIndexChanged);
  connect(controls.regularGridCheckBox, &QCheckBox::toggled, this, &splitViewWidget::on_regularGridCheckBox_toggled);
  connect(controls.gridSizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &splitViewWidget::on_gridSizeBox_valueChanged);
  connect(controls.zoomBoxCheckBox, &QCheckBox::toggled, this, &splitViewWidget::on_zoomBoxCheckBox_toggled);
  connect(controls.separateViewGroupBox, &QGroupBox::toggled, this, &splitViewWidget::on_separateViewGroupBox_toggled);
  connect(controls.linkViewsCheckBox, &QCheckBox::toggled, this, &splitViewWidget::on_linkViewsCheckBox_toggled);
  connect(controls.playbackPrimaryCheckBox, &QCheckBox::toggled, this, &splitViewWidget::on_playbackPrimaryCheckBox_toggled);
  connect(controls.zoomFactorSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &splitViewWidget::on_zoomFactorSpinBox_valueChanged);
}

void splitViewWidget::on_viewComboBox_currentIndexChanged(int index)
{
  switch (index)
  {
    case 0: // SIDE_BY_SIDE
      if (viewMode != SIDE_BY_SIDE)
      {
        viewMode = SIDE_BY_SIDE;
        resetViews();
      }
      break;
    case 1: // COMPARISON
      if (viewMode != COMPARISON)
      {
        viewMode = COMPARISON;
        resetViews();
      }
      break;
  }
}

void splitViewWidget::setViewMode(ViewMode v, bool emitSignal)
{
  if (isSeparateWidget)
  {
    // If this is the separate view, redirect this call to the primary view
    otherWidget->setViewMode(v, emitSignal);
    return;
  }

  if (viewMode == v)
    // This mode is already selected
    return;

  const QSignalBlocker blocker(emitSignal ? nullptr : controls.viewComboBox);

  if (v == SIDE_BY_SIDE)
    controls.viewComboBox->setCurrentIndex(0);
  else if (v == COMPARISON)
    controls.viewComboBox->setCurrentIndex(1);

  viewMode = v;
  otherWidget->viewMode = v;
}

void splitViewWidget::setPrimaryWidget(splitViewWidget *primary)
{
  Q_ASSERT_X(isSeparateWidget, "setPrimaryWidget", "Call this function only on the separate widget.");
  otherWidget = primary;

  // The primary splitViewWidget did set up controls for the widget. Connect signals/slots from these controls also here.
  connect(primary->controls.SplitViewgroupBox, &QGroupBox::toggled, this, &splitViewWidget::on_SplitViewgroupBox_toggled);
  connect(primary->controls.viewComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &splitViewWidget::on_viewComboBox_currentIndexChanged);
  connect(primary->controls.regularGridCheckBox, &QCheckBox::toggled, this, &splitViewWidget::on_regularGridCheckBox_toggled);
  connect(primary->controls.gridSizeBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &splitViewWidget::on_gridSizeBox_valueChanged);
  connect(primary->controls.zoomBoxCheckBox, &QCheckBox::toggled, this, &splitViewWidget::on_zoomBoxCheckBox_toggled);
  connect(primary->controls.linkViewsCheckBox, &QCheckBox::toggled, this, &splitViewWidget::on_linkViewsCheckBox_toggled);
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

void splitViewWidget::toggleSeparateViewHideShow()
{
  Q_ASSERT_X(!isSeparateWidget, "setSeparateWidget", "Call this function only on the primary widget.");

  if (!controls.separateViewGroupBox->isChecked())
    controls.separateViewGroupBox->setChecked(true);
  else
    controls.separateViewGroupBox->setChecked(false);
}

void splitViewWidget::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  Q_ASSERT_X(!isSeparateWidget, "setSeparateWidget", "Call this function only on the primary widget.");

  if (!item1 && !item2)
    return;

  QSettings settings;
  bool savePositionAndZoomPerItem = settings.value("SavePositionAndZoomPerItem", false).toBool();
  if (savePositionAndZoomPerItem)
  {
    // Restore the zoom and position which was saved in the playlist item
    const bool getOtherViewValuesFromOtherSlot = !linkViews;
    if (item1)
    {
      item1->getZoomAndPosition(centerOffset, zoomFactor, false);
      item1->getZoomAndPosition(otherWidget->centerOffset, otherWidget->zoomFactor, getOtherViewValuesFromOtherSlot);
    }
    else if (item2)
    {
      item2->getZoomAndPosition(centerOffset, zoomFactor, false);
      item2->getZoomAndPosition(otherWidget->centerOffset, otherWidget->zoomFactor, getOtherViewValuesFromOtherSlot);
    }
    DEBUG_LOAD_DRAW("splitViewWidget::currentSelectedItemsChanged restore from item %d (%d,%d-%f)", item1->getID(), centerOffset.x(), centerOffset.y(), zoomFactor);
  }
}

QImage splitViewWidget::getScreenshot(bool fullItem)
{
  QImage::Format fmt = dynamic_cast<QImage*>(backingStore()->paintDevice())->format();

  if (fullItem)
  {
    // Get the playlist item to draw
    auto item = playlist->getSelectedItems();
    if (item[0] == nullptr)
      return QImage();

    // Create an image buffer with the size of the item.
    QImage screenshot(item[0]->getSize(), fmt);
    QPainter painter(&screenshot);

    // Get the current frame to draw
    int frame = playback->getCurrentFrame();

    // Translate the painter to the position where we want the item to be
    QPoint center = QRect(QPoint(0,0), item[0]->getSize()).center();
    painter.translate(center);

    // TODO: What if loading is still in progress?

    // Draw the item at position (0,0)
    item[0]->drawItem(&painter, frame, 1, showRawData());

    // Do the inverse translation of the painter
    painter.resetTransform();

    return screenshot;
  }
  else
  {
    QImage screenshot(size(), fmt);
    render(&screenshot);
    return screenshot;
  }
}

void splitViewWidget::playbackStarted(int nextFrameIdx)
{
  if (isSeparateWidget)
    return;
  if (nextFrameIdx == -1)
    // The next frame is not within the currently selected item.
    // TODO: Maybe this can also be optimized. We could look for the next item and doubel buffer the first frame there.
    return;

  auto item = playlist->getSelectedItems();
  int frameIdx = playback->getCurrentFrame();
  if (item[0])
  {
    if (item[0]->needsLoading(nextFrameIdx, false) == LoadingNeeded)
    {
      // The current frame is loaded but the double buffer is not loaded yet. Start loading it.
      DEBUG_LOAD_DRAW("splitViewWidget::playbackStarted item 0 load frame %d", frameIdx);
      cache->loadFrame(item[0], frameIdx, 0);
    }
  }
  if (splitting && item[1])
  {
    if (item[1]->needsLoading(nextFrameIdx, false) == LoadingNeeded)
    {
      // The current frame is loaded but the double buffer is not loaded yet. Start loading it.
      DEBUG_LOAD_DRAW("splitViewWidget::playbackStarted item 1 load frame %d", frameIdx);
      cache->loadFrame(item[1], frameIdx, 1);
    }
  }
}

void splitViewWidget::update(bool newFrame, bool itemRedraw, bool updateOtherWidget)
{
  if (isSeparateWidget && !isVisible())
    // This is the separate view and it is not enabled. Nothing to update.
    return;
  if (linkViews && updateOtherWidget)
    otherWidget->update(newFrame, itemRedraw, false);

  bool playing = (playback) ? playback->playing() : false;
  DEBUG_LOAD_DRAW("splitViewWidget::update%s%s%s", (isSeparateWidget) ? " separate" : "", (newFrame) ? " newFrame" : "", (playing) ? " playing" : "");

  if (newFrame || itemRedraw)
  {
    // A new frame was selected (by the user directly or by playback).
    // That does not necessarily mean a paint event. First check if one of the items needs to load first.
    auto item = playlist->getSelectedItems();
    int frameIdx = playback->getCurrentFrame();
    bool loadRawData = showRawData() && !playing;
    bool itemLoading[2] = {false, false};
    if (item[0])
    {
      auto state = item[0]->needsLoading(frameIdx, loadRawData);
      if (state == LoadingNeeded)
      {
        // The frame needs to be loaded first.
        if (!isSeparateWidget)
          cache->loadFrame(item[0], frameIdx, 0);
        itemLoading[0] = true;
      }
      else if (playing && state == LoadingNeededDoubleBuffer)
      {
        // We can immediately draw the new frame but then we need to update the double buffer
        if (!isSeparateWidget)
        {
          item[0]->activateDoubleBuffer();
          cache->loadFrame(item[0], frameIdx, 0);
        }
      }
    }
    if (splitting && item[1])
    {
      auto state = item[1]->needsLoading(frameIdx, loadRawData);
      if (state == LoadingNeeded)
      {
        // The frame needs to be loaded first.
        if (!isSeparateWidget)
          cache->loadFrame(item[1], frameIdx, 1);
        itemLoading[1] = true;
      }
      else if (playing && state == LoadingNeededDoubleBuffer)
      {
        // We can immediately draw the new frame but then we need to update the double buffer
        if (!isSeparateWidget)
        {
          item[1]->activateDoubleBuffer();
          cache->loadFrame(item[1], frameIdx, 1);
        }
      }
    }

    DEBUG_LOAD_DRAW("splitViewWidget::update%s itemLoading[%d %d]", (isSeparateWidget) ? " separate" : "", itemLoading[0], itemLoading[1]);

    if ((itemLoading[0] || itemLoading[1]) && playing)
      // In case of playback, the item will let us know when it can be drawn.
      return;
    // We only need to redraw the items if a new frame is now loading and the "Loading..." message was not drawn yet.
    if (!playing && itemLoading[0] && drawingLoadingMessage[0])
      if (!splitting || (itemLoading[1] && drawingLoadingMessage[1]))
        return;
  }

  DEBUG_LOAD_DRAW("splitViewWidget::update%s trigger QWidget::update", (isSeparateWidget) ? " separate" : "");
  QWidget::update();
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
    if (!isSeparateWidget && controls.separateViewGroupBox->isChecked() && !playbackPrimary)
    {
      isViewFrozen = true;
      setMouseTracking(false);
      update();
    }
  }
}

void splitViewWidget::on_playbackPrimaryCheckBox_toggled(bool state)
{
  playbackPrimary = state;

  if (!isSeparateWidget && controls.separateViewGroupBox->isChecked() && playback->playing())
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

void splitViewWidget::getViewState(QPoint &offset, double &zoom, bool &split, double &splitPoint, int &mode) const
{
  offset = centerOffset;
  zoom = zoomFactor;
  split = splitting;
  splitPoint = splittingPoint;
  if (viewMode == SIDE_BY_SIDE)
    mode = 0;
  else if (viewMode == COMPARISON)
    mode = 1;
}

void splitViewWidget::setViewState(const QPoint &offset, double zoom, bool split, double splitPoint, int mode)
{
  if (isSeparateWidget)
    otherWidget->controls.SplitViewgroupBox->setChecked(split);
  else
    controls.SplitViewgroupBox->setChecked(split);
  setCenterOffset(offset);
  setZoomFactor(zoom);
  if (controls.zoomFactorSpinBox)
  {
    const QSignalBlocker block(controls.zoomFactorSpinBox);
    controls.zoomFactorSpinBox->setValue(int(zoomFactor * 100));
  }
  setSplittingPoint(splitPoint);
  if (mode == 0)
    setViewMode(SIDE_BY_SIDE);
  else if (mode == 1)
    setViewMode(COMPARISON);

  if (linkViews)
    otherWidget->setViewMode(viewMode);

  update();
}

void splitViewWidget::keyPressEvent(QKeyEvent *event)
{
  if (!handleKeyPress(event))
    // If this widget does not handle the key press event, pass it up to the widget so that
    // it is propagated to the parent.
    QWidget::keyPressEvent(event);
}

// Handle the key press event (if this widgets handles it). If not, return false.
bool splitViewWidget::handleKeyPress(QKeyEvent *event)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<< event;

  int key = event->key();
  bool controlOnly = event->modifiers() == Qt::ControlModifier;

  if (key == Qt::Key_W && controlOnly)
  {
    if (isSeparateWidget)
      emit signalShowSeparateWindow(false);
    else
      toggleSeparateViewHideShow();
    return true;
  }
  else if (key == Qt::Key_0 && controlOnly)
  {
    resetViews();
    return true;
  }
  else if (key == Qt::Key_9 && controlOnly)
  {
    zoomToFit();
    return true;
  }
  else if (key == Qt::Key_Plus && controlOnly)
  {
    zoomIn();
    return true;
  }
  else if (key == Qt::Key_BracketRight && controlOnly)
  {
    // This seems to be a bug in the Qt localization routine. On the German keyboard layout this key is returned if Ctrl + is pressed.
    zoomIn();
    return true;
  }
  else if (key == Qt::Key_Minus && controlOnly)
  {
    zoomOut();
    return true;
  }

  return false;
}

void splitViewWidget::testDrawingSpeed()
{
  DEBUG_LOAD_DRAW("splitViewWidget::testDrawingSpeed");

  auto selection = playlist->getSelectedItems();
  if (selection[0] == nullptr)
  {
    QMessageBox::information(parentWidget, "Test error", "Please select an item from the playlist to perform the test on.");
    return;
  }

  // Stop playback if running
  if (playback->playing())
    playback->on_stopButton_clicked();
  
  assert(parentWidget != nullptr);
  assert(testProgressDialog.isNull());
  testProgressDialog = new QProgressDialog("Running draw test...", "Cancel", 0, 1000, parentWidget);
  testProgressDialog->setWindowModality(Qt::WindowModal);

  testLoopCount = 1000;
  testMode = true;
  testProgrssUpdateTimer.start(200);
  testDuration.start();

  update();
}

void splitViewWidget::updateTestProgress()
{
  if (testProgressDialog.isNull())
    return;

  DEBUG_LOAD_DRAW("splitViewWidget::updateTestProgress %d", testLoopCount);

  // Check if the dialog was canceled
  if (testProgressDialog->wasCanceled())
  {
    testMode = false;
    testFinished(true);
  }
  else
    // Update the dialog progress
    testProgressDialog->setValue(1000-testLoopCount);
}

void splitViewWidget::testFinished(bool canceled)
{
  DEBUG_LOAD_DRAW("splitViewWidget::testFinished");

  // Quit test mode
  testMode = false;
  testProgrssUpdateTimer.stop();
  delete testProgressDialog;
  testProgressDialog.clear();

  if (canceled)
    // The test was canceled
    return;

  // Calculate and report the time
  int64_t msec = testDuration.elapsed();
  double rate = 1000.0 * 1000 / msec;
  QMessageBox::information(parentWidget, "Test results", QString("We drew 1000 frames in %1 msec. The draw rate is %2 frames per second.").arg(msec).arg(rate));
}