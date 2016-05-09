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

#ifndef SPLITVIEWWIDGET_H
#define SPLITVIEWWIDGET_H

#include <QWidget>
#include <QDockWidget>
#include <QMouseEvent>
#include "ui_splitViewWidgetControls.h"
#include "playlistItem.h"

enum ViewMode {SIDE_BY_SIDE, COMPARISON};

// The splitter can be grabbed at +-SPLITTER_MARGIN pixels
// TODO: plus minus 4 pixels for the handle might be not enough for high DPI displays. This should depend on the screens DPI.
#define SPLITVIEWWIDGET_SPLITTER_MARGIN 4
// The splitter cannot be moved closer to the border of the widget than SPLITTER_CLIPX pixels
// If the splitter is moved closer it cannot be moved back into view and is "lost"
#define SPLITVIEWWIDGET_SPLITTER_CLIPX 10
// The font and size of the text that will be drawn in the top left corner indicating the zoom factor
#define SPLITVIEWWIDGET_ZOOMFACTOR_FONT "helvetica"
#define SPLITVIEWWIDGET_ZOOMFACTOR_FONTSIZE 24
// When zooming in or out, you can only step by factors of x
#define SPLITVIEWWIDGET_ZOOM_STEP_FACTOR 2
// Set the zooming behavior. If zooming out, two approaches can be taken:
// 0: After the zoom out operation, the item point in the center of the widget will still be in the center of the widget.
// 1: After the zoom out operation, the item point under the mouse cursor will still be under the mouse.
#define SPLITVIEWWIDGET_ZOOM_OUT_MOUSE 1

class PlaylistTreeWidget;
class PlaybackController;

class splitViewWidget : public QWidget
{
  Q_OBJECT

public:
  explicit splitViewWidget(QWidget *parent = 0, bool separateView=false);

  /// Activate/Deactivate the splitting view. Only use this function!
  void setSplitEnabled(bool splitting);

  /// The common settings have changed (background color, ...)
  void updateSettings();

  // Set the widget to the given view mode
  void setViewMode(ViewMode v) { if (viewMode != v) { viewMode = v; resetViews(); } }

  //
  void setPlaylistTreeWidget( PlaylistTreeWidget *p ) { playlist = p; }
  void setPlaybackController( PlaybackController *p ) { playback = p; }

  // Setup the controls of the splitViewWidget and add them to the given dock widget. 
  // This has the advantage, that we can handle all buttonpresses and other events (which
  // are only relevant to this class) within this class and we don't have to bother the main frame.
  void setupControls(QDockWidget *dock);
  
  // Call setPrimaryWidget on the separate widget and provide the primary widget. 
  // Call setSeparateWidget on the primary widget and provide the separate widget.
  void setPrimaryWidget(splitViewWidget *primary);
  void setSeparateWidget(splitViewWidget *separate);

  // Set the minimum size hint. This will only be valid until the next showEvent. This is used when adding the widget 
  // as a new central widget. Then this size guarantees that the splitVie will have a certain size.
  // Call before adding the widget using setCenterWidget().
  void setMinimumSizeHint(QSize size) { minSizeHint = size; }

  // Update the splitView. If playback is running, call the second funtion so that the control can update conditionally.
  void update() { QWidget::update(); }
  void update(bool playback) { if (isSeparateWidget || !controls->separateViewGroupBox->isChecked() || !playback || playbackPrimary) update(); }

  // Freeze/unfreeze the view. If the view is frozen, it will take a screenshot of the current state and show that
  // in grayscale until it is unfrozen again.
  void freezeView(bool freeze);

  // Take a screenshot of this widget
  QPixmap getScreenshot();

signals:
  // If the user double clicks this widget, go to full screen.
  void signalToggleFullScreen();

  // Show (or hide) the separate window
  void signalShowSeparateWindow(bool show);

public slots:

  /// Reset everything so that the zoom factor is 1 and the display positions are centered
  void resetViews();

  /// Reset the view and set the zoom so that the current item is entirely visible.
  void zoomToFit();

  /// Zoom in/out to the given point. If no point is given, the center of the view will be 
  /// used for the zoom operation.
  void zoomIn(QPoint zoomPoint = QPoint());
  void zoomOut(QPoint zoomPoint = QPoint());

  // Update the control and emit signalShowSeparateWindow(bool).
  // This can be connected from the main window to allow keyboard shortcuts.
  void separateViewHide();
  void separateViewShow();

private slots:
  
  // Slots for the controls. They are connected when the main function sets up the controls (setuptControls).
  void on_SplitViewgroupBox_toggled(bool state) { setSplitEnabled( state ); update(); }
  void on_viewComboBox_currentIndexChanged(int index);
  void on_regularGridCheckBox_toggled(bool arg) { drawRegularGrid = arg; update(); }
  void on_gridSizeBox_valueChanged(int val) { regularGridSize = val; update(); }
  void on_zoomBoxCheckBox_toggled(bool state) { drawZoomBox = state; update(); }
  void on_separateViewGroupBox_toggled(bool state);
  void on_linkViewsCheckBox_toggled(bool state);
  void on_playbackPrimaryCheckBox_toggled(bool state);

protected:
  
  // The controls for the splitView (splitView, drawGrid ...)
  Ui::splitViewControlsWidget *controls;

  virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
  virtual void mousePressEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
  virtual void wheelEvent (QWheelEvent *e) Q_DECL_OVERRIDE;
  virtual void mouseDoubleClickEvent(QMouseEvent * event) Q_DECL_OVERRIDE { emit signalToggleFullScreen(); event->accept(); }

  // When the splitView is set as a center widget this will assert that after the adding operation the widget will have a
  // certain size (minSizeHint). The size can be set with setMinimumSizeHint().
  void showEvent(QShowEvent * event) Q_DECL_OVERRIDE { Q_UNUSED(event); minSizeHint = QSize(100,100); updateGeometry(); }
  virtual QSize	minimumSizeHint() const Q_DECL_OVERRIDE { return minSizeHint; }
  QSize minSizeHint;

  bool       splitting;          //!< If true the view will be split into 2 parts
  bool       splittingDragging;  //!< True if the user is currently dragging the splitter
  double     splittingPoint;     //!< A value between 0 and 1 specifying the horizontal split point (0 left, 1 right)
  enum       splitStyle {SOLID_LINE, TOP_BOTTOM_HANDLERS};
  splitStyle splittingLineStyle; //!< The style of the splitting line. This can be set in the settings window.

  QPoint  centerOffset;     //!< The offset of the view to the center (0,0)
  bool    viewDragging;     //!< True if the user is currently moving the view
  QPoint  viewDraggingMousePosStart;
  QPoint  viewDraggingStartOffset;
  
  double  zoomFactor;        //!< The current zoom factor
  QFont   zoomFactorFont;    //!< The font to use for the zoom factor indicator
  QPoint  zoomFactorFontPos; //!< The position where the zoom factor indication will be shown

  // The zoom box(es)
  bool   drawZoomBox;            //!< If set to true, the paint event will draw the zoom box(es)
  QPoint zoomBoxMousePosition;   //!< If we are drawing the zoom box(es) we have to know where the mouse currently is.
  QColor zoomBoxBackgroundColor; //!< The color of the zoom box background (read from settings)
  void   paintZoomBox(int view, QPainter *painter, int xSplit, QPoint drawArea_botR, QPointF itemZoomBoxTranslation, playlistItem *item, int frame, QPoint pixelPos, bool pixelPosInItem, double zoomFactor);

  // Regular grid
  bool drawRegularGrid;
  int  regularGridSize;      //!< The size of each block in the regular grid in pixels
  void paintRegularGrid(QPainter *painter, playlistItem *item);  //!< paint the grid

  // The current view mode (split view or compariosn view)
  ViewMode viewMode;

  // Pointers to the playlist tree widget and to the playback controller
  PlaylistTreeWidget *playlist;
  PlaybackController *playback;

  // Primary/Separate widget handeling
  bool isSeparateWidget;          //!< Is this the primary widget in the main windows or the one in the separate window
  splitViewWidget *otherWidget;   //!< Pointer to the other (primary or separate) widget
  bool linkViews;                 //!< Link the two widgets (link zoom factor, position and split position)
  bool playbackPrimary;           //!< When playback is running and this is the primary view and the secondary view is shown, don't run playback for this view.

  // Freezing of the view
  bool isViewFrozen;              //!< Is the view frozen?
  QPixmap frozenViewImage;        //!< The image that is shown when the view is frozen
};

#endif // SPLITVIEWWIDGET_H