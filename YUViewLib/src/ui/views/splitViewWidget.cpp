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

#include <QActionGroup>
#include <QBackingStore>
#include <QDockWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QTextDocument>

#include "ui/playbackController.h"
#include "playlistitem/playlistItem.h"
#include "video/frameHandler.h"
#include "video/videoCache.h"

// The splitter can be grabbed with a certain margin of pixels to the left and right. The margin
// in pixels is calculated depending on the logical DPI of the user using:
//    logicalDPI() / SPLITVIEWWIDGET_SPLITTER_MARGIN_DPI_DIV
// From the MS docs: "The standard DPI settings are 100% (96 DPI), 125% (120 DPI), and 150% (144 DPI). 
// The user can also apply a custom setting. Starting in Windows 7, DPI is a per-user setting."
// For 96 a divisor of 24 will results in +-4 pixels and 150% will result in +-6 pixels.
const int SPLITVIEWWIDGET_SPLITTER_MARGIN_DPI_DIV = 24;
// The splitter cannot be moved closer to the border of the widget than SPLITTER_CLIPX pixels
// If the splitter is moved closer it cannot be moved back into view and is "lost"
const int SPLITVIEWWIDGET_SPLITTER_CLIPX = 10;
// The font and size of the text that will be drawn in the top left corner indicating the zoom factor
const QString SPLITVIEWWIDGET_ZOOMFACTOR_FONT = "helvetica";
const int SPLITVIEWWIDGET_ZOOMFACTOR_FONTSIZE = 24;
// The font and the font size of the "loading..." message
const QString SPLITVIEWWIDGET_LOADING_FONT = "helvetica";
const int SPLITVIEWWIDGET_LOADING_FONTSIZE = 10;
// The font and the font size when drawing the item path in split view mode
const QString SPLITVIEWWIDGET_SPLITPATH_FONT = "helvetica";
const int SPLITVIEWWIDGET_SPLITPATH_FONTSIZE = 10;
const int SPLITVIEWWIDGET_SPLITPATH_PADDING = 20;
const int SPLITVIEWWIDGET_SPLITPATH_TOP_OFFSET = 10;
// The font and the font size when drawing the pixel values
const QString SPLITVIEWWIDGET_PIXEL_VALUES_FONT = "helvetica";
const int SPLITVIEWWIDGET_PIXEL_VALUES_FONTSIZE = 10;
// When zooming in or out, you can only step by factors of x
const int SPLITVIEWWIDGET_ZOOM_STEP_FACTOR = 2;
// Set the zooming behavior. If zooming out, two approaches can be taken:
// 0: After the zoom out operation, the item point in the center of the widget will still be in the center of the widget.
// 1: After the zoom out operation, the item point under the mouse cursor will still be under the mouse.
const int SPLITVIEWWIDGET_ZOOM_OUT_MOUSE = 1;
// What message is shown when a playlist item is loading.
const QString SPLITVIEWWIDGET_LOADING_TEXT = "Loading...";


// Activate this if you want to know when which item is triggered to load and draw
#define SPLITVIEWWIDGET_DEBUG_LOAD_DRAW 0
#if SPLITVIEWWIDGET_DEBUG_LOAD_DRAW && !NDEBUG
#define DEBUG_LOAD_DRAW(fmt) qDebug() << fmt
#else
#define DEBUG_LOAD_DRAW(fmt) ((void)0)
#endif

splitViewWidget::splitViewWidget(QWidget *parent)
  : MoveAndZoomableView(parent)
{
  setFocusPolicy(Qt::NoFocus);
  setViewSplitMode(DISABLED);
  updateSettings();
  setContextMenuPolicy(Qt::PreventContextMenu);

  // No test running yet
  connect(&testProgrssUpdateTimer, &QTimer::timeout, this, [=]{ updateTestProgress(); });

  // Initialize the font and the position of the zoom factor indication
  zoomFactorFont = QFont(SPLITVIEWWIDGET_ZOOMFACTOR_FONT, SPLITVIEWWIDGET_ZOOMFACTOR_FONTSIZE);
  QFontMetrics fm(zoomFactorFont);
  zoomFactorFontPos = QPoint(10, fm.height());

  // Load the caching pixmap
  waitingForCachingPixmap = QPixmap(":/img_hourglass.png");
  
  this->createMenuActions();
}

void splitViewWidget::setPlaylistTreeWidget(PlaylistTreeWidget *p) { playlist = p; }
void splitViewWidget::setPlaybackController(PlaybackController *p) { playback = p; }
void splitViewWidget::setVideoCache        (videoCache         *p) { cache = p;    }

/** The common settings might have changed.
  * Reload all settings from the QSettings and set them.
  */
void splitViewWidget::updateSettings()
{
  MoveAndZoomableView::updateSettings();

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

  zoomBoxBackgroundColor = settings.value("Background/Color").value<QColor>();
  drawItemPathAndNameEnabled = settings.value("ShowFilePathInSplitMode", true).toBool();

  // Something about how we draw might have been changed
  update();
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
    MoveAndZoomableView::updateMouseCursor();

    return;
  }

  DEBUG_LOAD_DRAW("splitViewWidget::paintEvent drawing " << (isMasterView ? " separate widget" : ""));

  // Get the current frame to draw
  const auto frame = playback->getCurrentFrame();

  // Is playback running?
  const bool playing = (playback) ? playback->playing() : false;
  // If yes, is is currently stalled because we are waiting for caching of an item to finish first?
  const bool waitingForCaching = playback->isWaitingForCaching();

  // Get the playlist item(s) to draw
  const auto item = playlist->getSelectedItems();
  const bool anyItemsSelected = item[0] != nullptr || item[1] != nullptr;

  // The x position of the split (if splitting)
  const int xSplit = int(drawArea_botR.x() * splittingPoint);

  // Calculate the zoom to use
  const double zoom = this->zoomFactor;
  const auto offset = this->moveOffset;

  const bool drawRawValues = showRawData() && !playing;

  // First determine the center points per of each view
  QPoint centerPoints[2];
  if (viewSplitMode == COMPARISON || viewSplitMode == DISABLED)
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
  if (anyItemsSelected && this->drawZoomBox)
  {
    // We now have the pixel difference value for the item under the cursor.
    // We now draw one zoom box per view
    int viewNum = (isSplitting() && item[1]) ? 2 : 1;
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

  if (isSplitting())
  {
    QStringPair itemNamesToDraw = determineItemNamesToDraw(item[0], item[1]);
    const bool drawItemNames = (drawItemPathAndNameEnabled && 
                                item[0] != nullptr && item[1] != nullptr &&
                                !itemNamesToDraw.first.isEmpty() && !itemNamesToDraw.second.isEmpty() &&
                                item[0]->isFileSource() && item[1]->isFileSource());

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

      if (drawItemNames)
        drawItemPathAndName(&painter, 0, xSplit, itemNamesToDraw.first);
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

      if (drawItemNames)
        drawItemPathAndName(&painter, xSplit, drawArea_botR.x() - xSplit, itemNamesToDraw.second);
    }

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

  if (isSplitting())
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

  if (this->viewAction == ViewAction::ZOOM_RECT)
  {
    if (isSplitting())
    {
      QRegion clipping;
      if (viewZoomingMousePosStart.x() < xSplit)
        clipping = QRegion(0, 0, xSplit, drawArea_botR.y());
      else
        clipping = QRegion(xSplit, 0, drawArea_botR.x() - xSplit, drawArea_botR.y());
      painter.setClipRegion(clipping);
    }
    this->drawZoomRect(painter);
    if (isSplitting())
      painter.setClipping(false);
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
  MoveAndZoomableView::updateMouseCursor();

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
  const auto item = playlist->getSelectedItems();
  const bool anyItemsSelected = item[0] != nullptr || item[1] != nullptr;

  // Get the full size of the area that we can draw on (from the paint device base)
  const auto drawAreaBotR = this->rect().bottomRight();

  // The x position of the split (if splitting)
  int xSplit = int(drawAreaBotR.x() * splittingPoint);

  // First determine the center points per of each view
  QPoint centerPoints[2];
  if (viewSplitMode == COMPARISON || !isSplitting())
  {
    // For comparison mode, both items have the same center point, in the middle of the view widget
    // This is equal to the scenario of not splitting
    centerPoints[0] = drawAreaBotR / 2;
    centerPoints[1] = centerPoints[0];
  }
  else
  {
    // For side by side mode, the center points are centered in each individual split view
    const auto y = drawAreaBotR.y() / 2;
    centerPoints[0] = QPoint(xSplit / 2, y);
    centerPoints[1] = QPoint(xSplit + (drawAreaBotR.x() - xSplit) / 2, y);
  }

  if (anyItemsSelected && this->drawZoomBox && geometry().contains(this->zoomBoxMousePosition))
  {
    // Is the mouse over the left or the right item? (mouseInLeftOrRightView: false=left, true=right)
    const auto xSplit = int(drawAreaBotR.x() * splittingPoint);
    const bool mouseInLeftOrRightView = (isSplitting() && (this->zoomBoxMousePosition.x() > xSplit));

    // The absolute center point of the item under the cursor
    const QPoint itemCenterMousePos = (mouseInLeftOrRightView) ? centerPoints[1] + this->moveOffset : centerPoints[0] + this->moveOffset;

    // The difference in the item under the mouse (normalized by zoom factor)
    double diffInItem[2] = {(double)(itemCenterMousePos.x() - this->zoomBoxMousePosition.x()) / this->zoomFactor + 0.5,
                            (double)(itemCenterMousePos.y() - this->zoomBoxMousePosition.y()) / this->zoomFactor + 0.5};

    // We now have the pixel difference value for the item under the cursor.
    // We now draw one zoom box per view
    const auto viewNum = (isSplitting() && item[1]) ? 2 : 1;
    for (int view = 0; view < viewNum; view++)
    {
      // Get the size of the item
      int itemSize[] = {item[view]->getSize().width(), item[view]->getSize().height()};

      // Calculate the position under the mouse cursor in pixels in the item under the mouse.
      {
        // Divide and round. We want value from 0...-1 to be quantized to -1 and not 0
        // so subtract 1 from the value if it is < 0.
        double pixelPosX = -diffInItem[0] + (double(itemSize[0]) / 2) + 0.5;
        double pixelPoxY = -diffInItem[1] + (double(itemSize[1]) / 2) + 0.5;
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
  if (!this->drawZoomBox)
    return;

  const int zoomBoxWindowZoomFactor = 32;
  const int srcSize = 5;
  const int margin = 11;
  const int padding = 6;
  int zoomBoxSize = srcSize * zoomBoxWindowZoomFactor;

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
  if (zoomFactor < zoomBoxWindowZoomFactor)
  {
    if (view == 0 && isSplitting())
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
    painter.translate(itemZoomBoxTranslation * zoomBoxWindowZoomFactor);

    // Draw the item again, but this time with a high zoom factor into the clipped region
    // Never draw the raw values in the zoom box.
    item->drawItem(&painter, frame, zoomBoxWindowZoomFactor, false);

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
    if (view == 0 && isSplitting())
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
  if (regularGridSize == 0)
    return;

  QSize itemSize = item->getSize() * this->zoomFactor;
  painter->setPen(regularGridColor);

  // Draw horizontal lines
  const int xMin = -itemSize.width() / 2;
  const int xMax =  itemSize.width() / 2;
  const double gridZoom = regularGridSize * this->zoomFactor;
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

  // Create the QRect to draw to
  QFontMetrics metrics(painter->font());
  QSize textSize = metrics.size(0, SPLITVIEWWIDGET_LOADING_TEXT);
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
  MoveAndZoomableView::mouseMoveEvent(mouse_event);

  if (this->drawZoomBox)
  {
    // If the mouse position changed, save the current point of the mouse and update the view (this will update the zoom box)
    if (zoomBoxMousePosition != mouse_event->pos())
    {
      zoomBoxMousePosition = mouse_event->pos();
      updatePixelPositions();
      update();
    }
    mouse_event->accept();
  }

  DEBUG_LOAD_DRAW("splitViewWidget::mouseMoveEvent isSplitting() " << isSplitting() << " splittingDragging " << this->splittingDragging);
  if (isSplitting() && this->splittingDragging)
  {
    mouse_event->accept();

    // The user is currently dragging the splitter. Calculate the new splitter point.
    int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width() - 2 - SPLITVIEWWIDGET_SPLITTER_CLIPX));
    setSplittingPoint((double)xClip / (double)(width()-2));

    update();
  }
}

void splitViewWidget::mousePressEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->source() == Qt::MouseEventSynthesizedBySystem && this->viewAction == ViewAction::PINCHING)
    // The mouse event was generated by the system from a touch event which is already handled by the touch pinch handler.
    return;

  if (isViewFrozen)
    return;

  // Are we over the split line?
  int splitPosPix = int((width()-2) * splittingPoint);
  bool mouseOverSplitLine = false;
  if (isSplitting())
  {
    // Calculate the margin of the split line according to the display DPI.
    int margin = logicalDpiX() / SPLITVIEWWIDGET_SPLITTER_MARGIN_DPI_DIV;
    mouseOverSplitLine = (mouse_event->x() > (splitPosPix-margin) && mouse_event->x() < (splitPosPix+margin));
  }

  if (mouse_event->button() == Qt::LeftButton && mouseOverSplitLine)
  {
    // Left mouse buttons pressed over the split line. Activate dragging of splitter.
    splittingDragging = true;

    mouse_event->accept();
    updateMouseCursor(mouse_event->pos());
  }
  else
    MoveAndZoomableView::mousePressEvent(mouse_event);
}

void splitViewWidget::mouseReleaseEvent(QMouseEvent *mouse_event)
{
  if (isViewFrozen)
    return;

  if (mouse_event->button() == Qt::LeftButton && isSplitting() && splittingDragging)
  {
    mouse_event->accept();
    splittingDragging = false;

    // Update current splitting position / update last time
    int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2-SPLITVIEWWIDGET_SPLITTER_CLIPX));
    setSplittingPoint((double)xClip / (double)(width()-2));

    update();
  }
  else
    MoveAndZoomableView::mouseReleaseEvent(mouse_event);
}

void splitViewWidget::wheelEvent (QWheelEvent *event)
{
  if (!isViewFrozen)
    MoveAndZoomableView::wheelEvent(event);
}

void splitViewWidget::setMoveOffset(QPoint offset)
{
  MoveAndZoomableView::setMoveOffset(offset);

  if (this->isMasterView)
  {
    // Save the center offset in the currently selected item
    auto item = playlist->getSelectedItems();
    for (int i = 0; i < 2; i++)
    {
      if (item[i])
      {
        DEBUG_LOAD_DRAW("splitViewWidget::setMoveOffset item " << item[i]->getID() << " (" << offset.x() << "," << offset.y() << ")");
        item[i]->saveCenterOffset(this->moveOffset, !isMasterView);
        item[i]->saveCenterOffset(this->getOtherWidget()->moveOffset, isMasterView);
      }
    }
  }
}

QPoint splitViewWidget::getMoveOffsetCoordinateSystemOrigin(const QPoint zoomPoint) const
{
  if (viewSplitMode == SIDE_BY_SIDE)
  {
    const auto drawAreaBotR = QWidget::rect().bottomRight();

    int xSplit = int(drawAreaBotR.x() * splittingPoint);
    const auto zoomPointInRightView = zoomPoint.x() > xSplit;
    if (zoomPointInRightView)
    {
      const auto centerOfRightView = QPoint(xSplit + (drawAreaBotR.x() - xSplit) / 2, drawAreaBotR.y() / 2);
      return centerOfRightView;
    }
    else
    {
      const auto centerOfLeftView = QPoint(xSplit / 2, drawAreaBotR.y() / 2);
      return centerOfLeftView;
    }
  }
  else
    return QWidget::rect().center();
}

void splitViewWidget::onZoomRectUpdateOffsetAndZoom(QRect zoomRect, double additionalZoomFactor)
{
  const QPoint zoomRectCenterOffset = zoomRect.center() - this->getMoveOffsetCoordinateSystemOrigin(this->viewZoomingMousePosStart);
  this->setMoveOffset((this->moveOffset - zoomRectCenterOffset) * additionalZoomFactor);
  this->setZoomFactor(this->zoomFactor * additionalZoomFactor);
}

void splitViewWidget::setSplittingPoint(double point, bool setLinkedViews)
{
  if (this->enableLink && setLinkedViews)
    this->getOtherWidget()->setSplittingPoint(point, false);

  splittingPoint = point;
}

void splitViewWidget::setZoomFactor(double zoom)
{
  MoveAndZoomableView::setZoomFactor(zoom);

  if (this->isMasterView)
  {
    // We are not calling the function in the other function
    // Save the zoom factor in the currently selected item
    auto item = playlist->getSelectedItems();
    for (int i = 0; i < 2; i++)
    {
      if (item[i])
      {
        DEBUG_LOAD_DRAW("splitViewWidget::setthis->getZoomFactor() item " << item[0]->getID() << " (" << zoom << ")");
        item[i]->saveZoomFactor(this->zoomFactor, !this->isMasterView);
        item[i]->saveZoomFactor(this->getOtherWidget()->zoomFactor, this->isMasterView);
      }
    }
  }
}

bool splitViewWidget::updateMouseCursor(const QPoint &mousePos)
{
  if (!MoveAndZoomableView::updateMouseCursor(mousePos))
    return false;
  
  if (this->viewAction == ViewAction::NONE)
  {
    // Get the item(s)
    auto item = playlist->getSelectedItems();

    if (isSplitting())
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

  return true;
}

void splitViewWidget::gridSetCustom(bool checked)
{
  Q_UNUSED(checked);
  bool ok;
  int newValue = QInputDialog::getInt(this, "Custom grid", "Please select a grid size value in pixels", 64, 1, 2147483647, 1, &ok);
  if (ok)
  {
    regularGridSize = newValue;
    update();
  }
}

void splitViewWidget::toggleZoomBox(bool checked)
{ 
  Q_UNUSED(checked); 
  this->drawZoomBox = !this->drawZoomBox;
  this->setMouseTracking(this->drawZoomBox);
  update(); 
}

void splitViewWidget::toggleSeparateWindow(bool checked) 
{
  Q_ASSERT_X(this->isMasterView, Q_FUNC_INFO, "This should only be toggled in the main widget.");

  actionSeparateViewLink.setEnabled(checked);
  actionSeparateViewPlaybackBoth.setEnabled(checked);

  emit signalShowSeparateWindow(checked);
}

void splitViewWidget::resetView(bool checked)
{
  this->setSplittingPoint(0.5);
  MoveAndZoomableView::resetView(checked);
}

void splitViewWidget::zoomToFit(bool checked)
{
  Q_UNUSED(checked);
  if (!playlist)
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;

  this->setMoveOffset(QPoint(0,0));

  auto item = playlist->getSelectedItems();

  if (item[0] == nullptr)
    // We cannot zoom to anything
    return;

  double fracZoom = 1.0;
  if (!isSplitting())
  {
    // Get the size of item 0 and the size of the widget and set the zoom factor so that this fits
    QSize item0Size = item[0]->getSize();
    if (item0Size.width() <= 0 || item0Size.height() <= 0)
      return;

    double zoomH = (double)size().width() / item0Size.width();
    double zoomV = (double)size().height() / item0Size.height();

    fracZoom = std::min(zoomH, zoomV);
  }
  else if (viewSplitMode == COMPARISON)
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
  else if (viewSplitMode == SIDE_BY_SIDE)
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
  this->setZoomFactor(newZoomFactor);
  update();
}

void splitViewWidget::setViewSplitMode(ViewSplitMode mode, bool setOtherViewIfLinked, bool callUpdate)
{
  if (this->enableLink && setOtherViewIfLinked)
    this->getOtherWidget()->setViewSplitMode(mode, false, callUpdate);

  if (viewSplitMode == mode)
    return;

  viewSplitMode = mode;

  // Check if the actions are selected correctly since this function could be called by an action or by some other source.
  for (size_t i = 0; i < 3; i++)
  {
    QSignalBlocker actionSplitViewBlocker(actionSplitView[i]);
    actionSplitView[i].setChecked(viewSplitMode == ViewSplitMode(i));
  }
  
  if (callUpdate)
    update();
}

void splitViewWidget::setRegularGridSize(unsigned int size, bool setOtherViewIfLinked, bool callUpdate)
{
  if (this->enableLink && setOtherViewIfLinked)
    this->getOtherWidget()->setRegularGridSize(size, false, callUpdate);

  if (regularGridSize == size)
    return;

  regularGridSize = size;

  // Check if the actions are selected correctly since this function could be called by an action or by some other source.
  const unsigned int actionGridValues[] = {0, 16, 32, 64, 128};
  bool valueFound = false;
  for (size_t i = 0; i < 5; i++)
  {
    QSignalBlocker actionSplitViewBlocker(actionGrid[i]);
    actionGrid[i].setChecked(regularGridSize == actionGridValues[i]);
    valueFound |= regularGridSize == actionGridValues[i];
  }
  if (!valueFound)
  {
    QSignalBlocker actionSplitViewBlocker(actionGrid[5]);
    actionGrid[5].setChecked(true);
  }

  if (callUpdate)
    update();
}

void splitViewWidget::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  Q_ASSERT_X(this->isMasterView, Q_FUNC_INFO, "Call this function only on the primary widget.");

  if (!item1 && !item2)
    return;

  QSettings settings;
  bool savePositionAndZoomPerItem = settings.value("SavePositionAndZoomPerItem", false).toBool();
  if (savePositionAndZoomPerItem)
  {
    // Restore the zoom and position which was saved in the playlist item
    const bool getOtherViewValuesFromOtherSlot = !enableLink;
    if (item1)
    {
      item1->getZoomAndPosition(this->moveOffset, this->zoomFactor, false);
      item1->getZoomAndPosition(this->getOtherWidget()->moveOffset, this->getOtherWidget()->zoomFactor, getOtherViewValuesFromOtherSlot);
    }
    else if (item2)
    {
      item2->getZoomAndPosition(moveOffset, this->zoomFactor, false);
      item2->getZoomAndPosition(this->getOtherWidget()->moveOffset, this->getOtherWidget()->zoomFactor, getOtherViewValuesFromOtherSlot);
    }
    DEBUG_LOAD_DRAW("splitViewWidget::currentSelectedItemsChanged restore from item " << item1->getID() << " moveOffset " << this->moveOffset << " zoom " << this->zoomFactor);
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
  if (!this->isMasterView)
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
      DEBUG_LOAD_DRAW("splitViewWidget::playbackStarted item 0 load frame " << frameIdx);
      cache->loadFrame(item[0], frameIdx, 0);
    }
  }
  if (isSplitting() && item[1])
  {
    if (item[1]->needsLoading(nextFrameIdx, false) == LoadingNeeded)
    {
      // The current frame is loaded but the double buffer is not loaded yet. Start loading it.
      DEBUG_LOAD_DRAW("splitViewWidget::playbackStarted item 1 load frame " << frameIdx);
      cache->loadFrame(item[1], frameIdx, 1);
    }
  }
}

void splitViewWidget::update(bool newFrame, bool itemRedraw)
{
  if (!this->isMasterView && !isVisible())
    // This is the separate view and it is not enabled. Nothing to update.
    return;

  bool playing = (playback) ? playback->playing() : false;
  DEBUG_LOAD_DRAW("splitViewWidget::update" << (!this->isMasterView ? " separate" : "") << (newFrame ? " newFrame" : "") << (playing ? " playing" : ""));

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
        if (this->isMasterView)
          cache->loadFrame(item[0], frameIdx, 0);
        itemLoading[0] = true;
      }
      else if (playing && state == LoadingNeededDoubleBuffer)
      {
        // We can immediately draw the new frame but then we need to update the double buffer
        if (this->isMasterView)
        {
          item[0]->activateDoubleBuffer();
          cache->loadFrame(item[0], frameIdx, 0);
        }
      }
    }
    if (isSplitting() && item[1])
    {
      auto state = item[1]->needsLoading(frameIdx, loadRawData);
      if (state == LoadingNeeded)
      {
        // The frame needs to be loaded first.
        if (this->isMasterView)
          cache->loadFrame(item[1], frameIdx, 1);
        itemLoading[1] = true;
      }
      else if (playing && state == LoadingNeededDoubleBuffer)
      {
        // We can immediately draw the new frame but then we need to update the double buffer
        if (this->isMasterView)
        {
          item[1]->activateDoubleBuffer();
          cache->loadFrame(item[1], frameIdx, 1);
        }
      }
    }

    DEBUG_LOAD_DRAW("splitViewWidget::update" << (this->isMasterView ? "" : " seperate") << " itemLoading[" << itemLoading[0] << "," << itemLoading[1] << "]");

    if ((itemLoading[0] || itemLoading[1]) && playing)
      // In case of playback, the item will let us know when it can be drawn.
      return;
    // We only need to redraw the items if a new frame is now loading and the "Loading..." message was not drawn yet.
    if (!playing && itemLoading[0] && drawingLoadingMessage[0])
      if (!isSplitting() || (itemLoading[1] && drawingLoadingMessage[1]))
        return;
  }

  DEBUG_LOAD_DRAW("splitViewWidget::update%s trigger QWidget::update" << (this->isMasterView ? "" : " separate"));
  MoveAndZoomableView::update();
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
    const bool isSeparateViewEnabled = actionSeparateView.isChecked();
    const bool playbackPrimary = actionSeparateViewPlaybackBoth.isChecked();
    if (this->isMasterView && isSeparateViewEnabled && !playbackPrimary)
    {
      isViewFrozen = true;
      setMouseTracking(false);
      update();
    }
  }
}

void splitViewWidget::getViewState(QPoint &offset, double &zoom, double &splitPoint, int &mode) const
{
  offset = this->moveOffset;
  zoom = this->zoomFactor;
  splitPoint = splittingPoint;
  if (viewSplitMode == DISABLED)
    mode = 0;
  else if (viewSplitMode == SIDE_BY_SIDE)
    mode = 1;
  else if (viewSplitMode == COMPARISON)
    mode = 2;
}

void splitViewWidget::setViewState(const QPoint &offset, double zoom, double splitPoint, int mode)
{
  this->setMoveOffset(offset);
  this->setZoomFactor(zoom);
  this->setSplittingPoint(splitPoint);
  this->setViewSplitMode(ViewSplitMode(mode));
  if (enableLink)
    this->getOtherWidget()->setViewSplitMode(viewSplitMode);

  update();
}

void splitViewWidget::keyPressEvent(QKeyEvent *event)
{
  if (!handleKeyPress(event))
    // If this widget does not handle the key press event, pass it up to the widget so that
    // it is propagated to the parent.
    QWidget::keyPressEvent(event);
}

void splitViewWidget::onSwipeLeft()
{ 
  playback->nextFrame();
}

void splitViewWidget::onSwipeRight()
{ 
  playback->previousFrame();
}

void splitViewWidget::onSwipeUp()
{ 
  playlist->selectNextItem();
}

void splitViewWidget::onSwipeDown()
{ 
  playlist->selectPreviousItem();
}

void splitViewWidget::createMenuActions()
{
  const bool menuActionsNoteCreatedYet = actionSplitViewGroup.isNull();
  Q_ASSERT_X(menuActionsNoteCreatedYet, Q_FUNC_INFO, "Only call this initialization function once.");

  auto configureCheckableAction = [this](QAction &action, QActionGroup *actionGroup, QString text, bool checked, void(splitViewWidget::*func)(bool), const QKeySequence &shortcut = {}, bool isEnabled = true)
  {
    action.setParent(this);
    action.setCheckable(true);
    action.setChecked(checked);
    action.setText(text);
    action.setShortcut(shortcut);
    if (actionGroup)
      actionGroup->addAction(&action);
    if (!isEnabled)
      action.setEnabled(false);
    connect(&action, &QAction::triggered, this, func);
  };

  actionSplitViewGroup.reset(new QActionGroup(this));
  configureCheckableAction(actionSplitView[0], actionSplitViewGroup.data(), "Disabled", viewSplitMode == DISABLED, &splitViewWidget::splitViewDisable);
  configureCheckableAction(actionSplitView[1], actionSplitViewGroup.data(), "Side-by-Side", viewSplitMode == SIDE_BY_SIDE, &splitViewWidget::splitViewSideBySide);
  configureCheckableAction(actionSplitView[2], actionSplitViewGroup.data(), "Comparison", viewSplitMode == COMPARISON, &splitViewWidget::splitViewComparison);
  actionSplitView[0].setToolTip("Show only one single Item.");
  actionSplitView[1].setToolTip("Show two items side-by-side so that the same part of each item is visible.");
  actionSplitView[2].setToolTip("Show two items at the same position with a split line that can be moved to reveal either item.");

  actionGridGroup.reset(new QActionGroup(this));
  configureCheckableAction(actionGrid[0], actionGridGroup.data(), "Disabled", regularGridSize == 0, &splitViewWidget::gridDisable);
  configureCheckableAction(actionGrid[1], actionGridGroup.data(), "16x16", regularGridSize == 16, &splitViewWidget::gridSet16);
  configureCheckableAction(actionGrid[2], actionGridGroup.data(), "32x32", regularGridSize == 32, &splitViewWidget::gridSet32);
  configureCheckableAction(actionGrid[3], actionGridGroup.data(), "64x64", regularGridSize == 64, &splitViewWidget::gridSet64);
  configureCheckableAction(actionGrid[4], actionGridGroup.data(), "128x128", regularGridSize == 128, &splitViewWidget::gridSet128);
  configureCheckableAction(actionGrid[5], actionGridGroup.data(), "Custom...", regularGridSize != 0 && regularGridSize != 16 && regularGridSize != 32 && regularGridSize != 64 && regularGridSize != 128, &splitViewWidget::gridSetCustom);

  configureCheckableAction(actionZoomBox, nullptr, "Zoom Box", drawZoomBox, &splitViewWidget::toggleZoomBox);
  actionZoomBox.setToolTip("Activate the Zoom Box which renders a zoomed portion of the screen and shows pixel information.");

  if (this->isMasterView)
  {
    configureCheckableAction(actionSeparateView, nullptr, "&Show Separate Window", false, &splitViewWidget::toggleSeparateWindow, Qt::CTRL + Qt::Key_W);
    configureCheckableAction(actionSeparateViewLink, nullptr, "Link Views", false, &MoveAndZoomableView::setLinkState, {}, false);
    configureCheckableAction(actionSeparateViewPlaybackBoth, nullptr, "Playback in both Views", false, &splitViewWidget::toggleSeparateWindowPlaybackBoth, {}, false);
    actionSeparateView.setToolTip("Show a second window with another view to the same item. Especially helpfull for multi screen setups.");
    actionSeparateViewLink.setToolTip("Link the second view so that any change in one view is also applied in the other view.");
    actionSeparateViewPlaybackBoth.setToolTip("For performance reasons playback only runs in one (the second) view. Activate this to run playback in both views siultaneously.");
  }
}

// Handle the key press event (if this widgets handles it). If not, return false.
bool splitViewWidget::handleKeyPress(QKeyEvent *event)
{
  //qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz")<<"Key: "<< event;

  int key = event->key();
  bool controlOnly = event->modifiers() == Qt::ControlModifier;

  if (key == Qt::Key_W && controlOnly)
  {
    if (!this->isMasterView)
      emit signalShowSeparateWindow(false);
    return true;
  }
  else if (key == Qt::Key_0 && controlOnly)
  {
    this->resetView();
    return true;
  }
  else if (key == Qt::Key_9 && controlOnly)
  {
    zoomToFit();
    return true;
  }
  else if (key == Qt::Key_Plus && controlOnly)
  {
    this->zoom(ZoomMode::IN);
    return true;
  }
  else if (key == Qt::Key_BracketRight && controlOnly)
  {
    // This seems to be a bug in the Qt localization routine. On the German keyboard layout this key is returned if Ctrl + is pressed.
    this->zoom(ZoomMode::IN);
    return true;
  }
  else if (key == Qt::Key_Minus && controlOnly)
  {
    this->zoom(ZoomMode::OUT);
    return true;
  }

  return false;
}

QStringPair splitViewWidget::determineItemNamesToDraw(playlistItem *item1, playlistItem *item2)
{
  if (item1 == nullptr || item2 == nullptr)
    return QStringPair();

  auto sep = QDir::separator();
  auto name1 = item1->getName().split(sep);
  auto name2 = item2->getName().split(sep);
  if (name1.empty() || name2.empty())
    return QStringPair();

  auto it1 = name1.constEnd() - 1;
  auto it2 = name2.constEnd() - 1;
  
  if (*it1 != *it2)
    return QStringPair(*it1, *it2);

  QStringPair ret = QStringPair(*it1, *it2);
  --it1;
  --it2;
  bool foundDiff = false;

  while (it1 != name1.constBegin() - 1 && it2 != name2.constBegin() - 1)
  {
    ret.first = *it1 + sep + ret.first;
    ret.second = *it2 + sep + ret.second;
    if (*it1 != *it2)
    {
      foundDiff = true;
      break;
    }
    
    --it1;
    --it2;
  }

  if(!foundDiff)
  {
    while (it1 != name1.constBegin() - 1)
    {
      ret.first = *it1 + sep + ret.first;
      --it1;
    }

    while (it2 != name2.constBegin() - 1)
    {
      ret.second = *it2 + sep + ret.second;
      --it2;
    }
  }

  return ret;
}

void splitViewWidget::drawItemPathAndName(QPainter *painter, int posX, int width, QString path)
{
  DEBUG_LOAD_DRAW("splitViewWidget::drawItemPathAndName");
  QString drawString;

  auto sep = QDir::separator();
  auto pathSplit = path.split(sep);

  // The metrics for evaluating the width of the rendered text
  QFont valueFont = QFont(SPLITVIEWWIDGET_SPLITPATH_FONT, SPLITVIEWWIDGET_SPLITPATH_FONTSIZE);
  QFontMetrics metrics(valueFont);
  
  QString currentLine = "";
  if (pathSplit.size() > 0)
  {
    for (auto it = pathSplit.begin(); it != pathSplit.end(); it++)
    {
      const bool isLast = (it == pathSplit.end() - 1);
      if (currentLine.isEmpty())
      {
        currentLine = *it;
        if (!isLast)
          currentLine += sep;
      }
      else
      {
        // Will the part fit into the the current line?
        QString lineTemp = currentLine + *it;
        if (!isLast)
          lineTemp += sep;
        QSize textSize = metrics.size(0, lineTemp);
        if (textSize.width() > width - SPLITVIEWWIDGET_SPLITPATH_PADDING)
        {
          // This won't fit. Put it to the next line
          drawString += currentLine + "\n";
          currentLine = *it;
          if (!isLast)
            currentLine += sep;
        }
        else
          currentLine = lineTemp;
      }
    }
  }
  if (!currentLine.isEmpty())
    drawString += currentLine;

  // Create the QRect to draw to
  QSize textSize = metrics.size(0, drawString);
  QRect textRect;
  textRect.setSize(textSize);
  textRect.moveCenter(QPoint(posX + width / 2, 0));
  textRect.moveTop(SPLITVIEWWIDGET_SPLITPATH_TOP_OFFSET);

  // Draw a rectangle around the text in white with a black border
  QRect boxRect = textRect + QMargins(5, 5, 5, 5);
  painter->setPen(QPen(Qt::black, 1));
  painter->fillRect(boxRect,Qt::white);
  painter->drawRect(boxRect);

  // Draw the text
  painter->drawText(textRect, Qt::AlignCenter, drawString);

  return;
}

void splitViewWidget::testDrawingSpeed()
{
  DEBUG_LOAD_DRAW("splitViewWidget::testDrawingSpeed");

  auto selection = playlist->getSelectedItems();
  if (selection[0] == nullptr)
  {
    QMessageBox::information(this, "Test error", "Please select an item from the playlist to perform the test on.");
    return;
  }

  // Stop playback if running
  if (playback->playing())
    playback->on_stopButton_clicked();
  
  assert(testProgressDialog.isNull());
  testProgressDialog = new QProgressDialog("Running draw test...", "Cancel", 0, 1000, this);
  testProgressDialog->setWindowModality(Qt::WindowModal);

  testLoopCount = 1000;
  testMode = true;
  testProgrssUpdateTimer.start(200);
  testDuration.start();

  update();
}

void splitViewWidget::addMenuActions(QMenu *menu)
{
  QMenu *splitViewMenu = menu->addMenu("Split View");
  for (size_t i = 0; i < 3; i++)
    splitViewMenu->addAction(&actionSplitView[i]);
  splitViewMenu->setToolTipsVisible(true);

  QMenu *drawGridMenu = menu->addMenu("Draw Grid");
  for (size_t i = 0; i < 6; i++)
    drawGridMenu->addAction(&actionGrid[i]);

  menu->addAction(&actionZoomBox);
  MoveAndZoomableView::addMenuActions(menu);
  menu->addSeparator();

  QMenu *separateViewMenu = menu->addMenu("Separate View");
  separateViewMenu->addAction(!isMasterView ? &this->getOtherWidget()->actionSeparateView : &actionSeparateView);
  separateViewMenu->addAction(!isMasterView ? &this->getOtherWidget()->actionSeparateViewLink : &actionSeparateViewLink);
  separateViewMenu->addAction(!isMasterView ? &this->getOtherWidget()->actionSeparateViewPlaybackBoth : &actionSeparateViewPlaybackBoth);
  separateViewMenu->setToolTipsVisible(true);

  menu->setToolTipsVisible(true);
}

void splitViewWidget::updateTestProgress()
{
  if (testProgressDialog.isNull())
    return;

  DEBUG_LOAD_DRAW("splitViewWidget::updateTestProgress " << testLoopCount);

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
  QMessageBox::information(this, "Test results", QString("We drew 1000 frames in %1 msec. The draw rate is %2 frames per second.").arg(msec).arg(rate));
}

QPointer<splitViewWidget> splitViewWidget::getOtherWidget() const
{
  if (this->isMasterView)
  {
    Q_ASSERT_X(this->slaveViews.size() == 1, Q_FUNC_INFO, "The other view is not set as slave.");
    return QPointer<splitViewWidget>(qobject_cast<splitViewWidget*>(this->slaveViews[0]));
  }
  else
  {
    Q_ASSERT_X(this->masterView, Q_FUNC_INFO, "The master view is not set.");
    return QPointer<splitViewWidget>(qobject_cast<splitViewWidget*>(this->masterView));
  }
}
