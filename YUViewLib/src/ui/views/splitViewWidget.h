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

#ifndef SPLITVIEWWIDGET_H
#define SPLITVIEWWIDGET_H

#include <QAction>
#include <QElapsedTimer>
#include <QMenu>
#include <QMouseEvent>
#include <QProgressDialog>
#include <QPointer>
#include <QTimer>

#include "MoveAndZoomableView.h"
#include "common/saveUi.h"
#include "common/typedef.h"

class QDockWidget;
class PlaybackController;
class playlistItem;
class PlaylistTreeWidget;
class videoCache;

class splitViewWidget : public MoveAndZoomableView
{
  Q_OBJECT

public:
  explicit splitViewWidget(QWidget *parent = 0);

  // Set pointers to the playlist tree, the playback controller and the cache.
  void setPlaylistTreeWidget(PlaylistTreeWidget *p);
  void setPlaybackController(PlaybackController *p);
  void setVideoCache        (videoCache *p);

  // Set the minimum size hint. This will only be valid until the next showEvent. This is used when adding the widget
  // as a new central widget. Then this size guarantees that the splitVie will have a certain size.
  // Call before adding the widget using setCenterWidget().
  void setMinimumSizeHint(const QSize &size) { minSizeHint = size; }

  // Update the splitView. newFrame should be true if the frame index was changed or the playlistitem needs a redraw.
  // If newFrame is true, this will not automatically trigger a redraw, because first we might need to load the right frame.
  // itemRedraw indicates if the playlist item initiated this redraw (possibly the item also needs to be reloaded).
  void update(bool newFrame=false, bool itemRedraw=false);

  // Called by the playback controller if playback was just started. In this case, we immediately see if the double buffer
  // of the visible item(s) need to be updated.
  void playbackStarted(int nextFrameIdx);

  // Freeze/unfreeze the view. If the view is frozen, it will take a screenshot of the current state and show that
  // in gray-scale until it is unfrozen again.
  void freezeView(bool freeze);

  // Take a screenshot of this widget as the user sees it in the center of the main window wight now.
  // If fullItem is set, instead a full render of the first selected item will be shown.
  QImage getScreenshot(bool fullItem=false);

  // This can be called from the parent widget. It will return false if the event is not handled here so it can be passed on.
  bool handleKeyPress(QKeyEvent *event);

  // Get and set the current state (center point and zoom, is splitting active? if yes the split line position)
  void getViewState(QPoint &offset, double &zoom, double &splitPoint, int &mode) const;
  void setViewState(const QPoint &offset, double zoom, double splitPoint, int mode);
  bool isSplitting() { return viewSplitMode != DISABLED; }

  /// The settings have changed. Reload all settings that affects this widget.
  void updateSettings() override;

  // Raw values are shown if the zoom factor is high enough or if the zoom box is shown.
  bool showRawData() { return this->zoomFactor >= SPLITVIEW_DRAW_VALUES_ZOOMFACTOR || this->drawZoomBox; }

  // Test the drawing speed with the currently selected item
  void testDrawingSpeed();

  // Add the split views menu items to the given menu. Used for the main menu bar and the context menu.
  void addMenuActions(QMenu *menu) override;

signals:
  
  // Show (or hide) the separate window
  void signalShowSeparateWindow(bool show);

public slots:

  // Accept the signal from the playlisttreewidget that signals if a new (or two) item was selected.
  // This function will restore the view/position of the items (if enabled)
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2);

  void triggerActionSeparateView() { actionSeparateView.trigger(); }

  virtual void resetView(bool checked = false) override;

private slots:

  virtual void zoomToFit(bool checked = false) override;

  void splitViewDisable(bool checked) { Q_UNUSED(checked); setViewSplitMode(DISABLED, true, true); }
  void splitViewSideBySide(bool checked) { Q_UNUSED(checked); setViewSplitMode(SIDE_BY_SIDE, true, true); }
  void splitViewComparison(bool checked) { Q_UNUSED(checked); setViewSplitMode(COMPARISON, true, true); }

  void gridDisable(bool checked) { Q_UNUSED(checked); setRegularGridSize(0, true, true); }
  void gridSet16(bool checked) { Q_UNUSED(checked); setRegularGridSize(16, true, true); }
  void gridSet32(bool checked) { Q_UNUSED(checked); setRegularGridSize(32, true, true); }
  void gridSet64(bool checked) { Q_UNUSED(checked); setRegularGridSize(64, true, true); }
  void gridSet128(bool checked) { Q_UNUSED(checked); setRegularGridSize(128, true, true); }
  void gridSetCustom(bool checked);

  void toggleZoomBox(bool checked);

  void toggleSeparateWindow(bool checked);
  void toggleSeparateWindowPlaybackBoth(bool checked) { Q_UNUSED(checked); };

protected:
  
  // Set the widget to the given view mode
  enum ViewSplitMode {DISABLED, SIDE_BY_SIDE, COMPARISON};
  // Set the view mode and update the view mode combo box. Disable the combo box events if emitSignal is false.
  void setViewSplitMode(ViewSplitMode v, bool setOtherViewIfLinked = true, bool callUpdate = false);
  // The current view mode (split view or comparison view)
  ViewSplitMode viewSplitMode {DISABLED};

  // Override some events from the widget
  virtual void paintEvent(QPaintEvent *event) override;
  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void mouseReleaseEvent(QMouseEvent *event) override;
  virtual void wheelEvent (QWheelEvent *event) override;
  virtual void keyPressEvent(QKeyEvent *event) override;

  virtual void onSwipeLeft() override;
  virtual void onSwipeRight() override;
  virtual void onSwipeUp() override;
  virtual void onSwipeDown() override;

  void createMenuActions();
  QScopedPointer<QActionGroup> actionSplitViewGroup;
  QScopedPointer<QActionGroup> actionGridGroup;
  QAction actionSplitView[3];
  QAction actionGrid[6];
  QAction actionSeparateView;
  QAction actionSeparateViewLink;
  QAction actionSeparateViewPlaybackBoth;
  QAction actionZoomBox;

  virtual bool updateMouseCursor(const QPoint &srcMousePos) override;

  // When the splitView is set as a center widget this will assert that after the adding operation the widget will have a
  // certain size (minSizeHint). The size can be set with setMinimumSizeHint().
  void showEvent(QShowEvent *event) override { Q_UNUSED(event); minSizeHint = QSize(100,100); updateGeometry(); }
  virtual QSize	minimumSizeHint() const override { return minSizeHint; }
  QSize minSizeHint;

  bool       splittingDragging {false};     //!< True if the user is currently dragging the splitter
  void       setSplittingPoint(double p, bool setOtherViewIfLinked = true);
  double     splittingPoint {0.5};          //!< A value between 0 and 1 specifying the horizontal split point (0 left, 1 right)
  enum       splitStyle {SOLID_LINE, TOP_BOTTOM_HANDLERS};
  splitStyle splittingLineStyle;            //!< The style of the splitting line. This can be set in the settings window.

  void   setMoveOffset(QPoint offset) override;
  QPoint getMoveOffsetCoordinateSystemOrigin(const QPoint zoomPoint = {}) const override;
  void   onZoomRectUpdateOffsetAndZoom(QRect zoomRect, double additionalZoomFactor) override;

  QRect   viewActiveArea;                   //!< The active area, where the picture is drawn into

  void    setZoomFactor(double zoom) override;
  QFont   zoomFactorFont;                   //!< The font to use for the zoom factor indicator
  QPoint  zoomFactorFontPos;                //!< The position where the zoom factor indication will be shown
  void    onZoomIn() { this->update(false, true); }

  // The zoom box(es)
  bool   drawZoomBox {false};               //!< If set to true, the paint event will draw the zoom box(es)
  QPoint zoomBoxMousePosition;              //!< If we are drawing the zoom box(es) we have to know where the mouse currently is.
  QColor zoomBoxBackgroundColor;            //!< The color of the zoom box background (read from settings)
  void   paintZoomBox(int view, QPainter &painter, int xSplit, const QPoint &drawArea_botR, playlistItem *item, int frame, const QPoint &pixelPos, bool pixelPosInItem, double zoomFactor, bool playing);

  //!< Using the current mouse position, calculate the position in the items under the mouse (per view)
  void   updatePixelPositions();
  QPoint zoomBoxPixelUnderCursor[2];        //!< The above function will update this. (The position of the pixel under the cursor (per item))

  // Regular grid
  unsigned int regularGridSize {0};         //!< The size of each block in the regular grid in pixels
  void setRegularGridSize(unsigned int size, bool setOtherViewIfLinked = true, bool callUpdate = false);
  QColor regularGridColor;
  void paintRegularGrid(QPainter *painter, playlistItem *item);  //!< paint the grid

  // Pointers to the playlist tree widget, the playback controller and the videoCache
  QPointer<PlaylistTreeWidget> playlist;
  QPointer<PlaybackController> playback;
  QPointer<videoCache>         cache;

  // Freezing of the view
  bool isViewFrozen {false};               //!< Is the view frozen?

  // Draw the "Loading..." message (if needed)
  void drawLoadingMessage(QPainter *painter, const QPoint &pos);
  // True if the "Loading..." message is currently being drawn for one of the two items
  bool drawingLoadingMessage[2] {false, false};

  // Draw a ruler at the top and left that indicate the x and y position of the visible pixels
  void paintPixelRulersX(QPainter &painter, playlistItem *item, int xPixMin, int xPixMax, double zoom, QPoint centerPoints, QPoint offset);
  void paintPixelRulersY(QPainter &painter, playlistItem *item, int yPixMax, int xPos,    double zoom, QPoint centerPoints, QPoint offset);

  // Class to save the current view statue (center point and zoom, splitting settings) so that we can quickly switch between them 
  // using the keyboard.
  class splitViewWidgetState
  {
  public:
    splitViewWidgetState() : valid(false) {}
    bool valid;
    QPoint centerOffset;
    double zoomFactor;
    double splittingPoint;
    ViewSplitMode viewMode;
  };
  splitViewWidgetState viewStates[8];

  // This pixmap is drawn in the lower left corner if playback is paused because we are waiting for caching.
  QPixmap waitingForCachingPixmap;

  // This is set to true by the update function so that the palette is updated in the next draw event.
  bool paletteNeedsUpdate;

  QStringPair determineItemNamesToDraw(playlistItem *item1, playlistItem *item2);
  void drawItemPathAndName(QPainter *painter, int posX, int width, QString path);
  bool drawItemPathAndNameEnabled {true};

  // Everything related to drawing tests
  QPointer<QProgressDialog> testProgressDialog;
  int testLoopCount;                            //< Set before the test starts. Count down to 0. Then the test is over.
  bool testMode {false};                                //< Set to true when the test is running
  QTimer testProgrssUpdateTimer;                //< Periodically update the progress dialog
  QElapsedTimer testDuration;                   //< Used to obtain the duration of the test
  void updateTestProgress();
  void testFinished(bool canceled);             //< Report the test results and stop the testProgrssUpdateTimer

  QPointer<splitViewWidget> getOtherWidget() const;
};

#endif // SPLITVIEWWIDGET_H
