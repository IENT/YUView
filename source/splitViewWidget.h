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
#include "ui_splitViewWidgetControls.h"

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

#define SPLITVIEWWIDGET_ZOOM_STEP_FACTOR 2

class PlaylistTreeWidget;
class PlaybackController;

class splitViewWidget : public QWidget
{
  Q_OBJECT

public:
  explicit splitViewWidget(QWidget *parent = 0);

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
  void setuptControls(QDockWidget *dock);

public slots:

  /// Reset everything so that the zoom factor is 1 and the display positions are centered
  void resetViews();

  /// TODO: 
  void zoomToFit() {};

  /// Zoom in/out to the given point. If no point is given, the center of the view will be 
  /// used for the zoom operation.
  void zoomIn(QPoint zoomPoint = QPoint());
  void zoomOut(QPoint zoomPoint = QPoint());

private slots:
  
  // Slots for the controls. They are connected when the main function sets up the controls (setuptControls).
  void on_SplitViewgroupBox_toggled(bool state) { setSplitEnabled( state ); update(); }
  void on_viewComboBox_currentIndexChanged(int index);
  void on_regularGridCheckBox_toggled(bool arg) { drawRegularGrid = arg; update(); }
  void on_gridSizeBox_valueChanged(int val) { regularGridSize = val; update(); }
  void on_zoomBoxCheckBox_toggled(bool state) { drawZoomBox = state; update(); }

private:
  
  // The controls for the splitView (splitView, drawGrid ...)
  Ui::splitViewControlsWidget *controls;

  virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
  virtual void mousePressEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent * event) Q_DECL_OVERRIDE;
  virtual void wheelEvent (QWheelEvent *e) Q_DECL_OVERRIDE;

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

  // Other render features
  bool drawRegularGrid;      //!< If set to true, the paint event will draw a regular grid 
  int  regularGridSize;      //!< The size of each block in the regular grid in pixels

  // The current view mode (split view or compariosn view)
  ViewMode viewMode;

  // Pointers to the playlist tree widget and to the playback controller
  PlaylistTreeWidget *playlist;
  PlaybackController *playback;
};

#endif // SPLITVIEWWIDGET_H