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

#ifndef SEPARATEWINDOW_H
#define SEPARATEWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QKeyEvent>

#include "splitViewWidget.h"

class SeparateWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit SeparateWindow();
  splitViewWidget splitView;

signals:
  // Signal that the user wants to go back to single window mode
  void signalSingleWindowMode();
  
  // There was a key event in the separate window, but the separate view did not handle it.
  // The signal should be processed by the main window (maybe it is a nex/prev frame key event or something...).
  void unhandledKeyPress(QKeyEvent *event);

public slots:
  void toggleFullscreen();

protected:
  void closeEvent(QCloseEvent *event) Q_DECL_OVERRIDE;
  void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

protected slots:
  void splitViewShowSeparateWindow(bool show) { if (!show) emit signalSingleWindowMode(); }

private:
  // If the window is shown full screen, this saves if it was maximized before going to full screen
  bool showNormalMaximized;
};

#endif // SEPARATEWINDOW_H
