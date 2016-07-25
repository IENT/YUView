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

#include "separateWindow.h"

SeparateWindow::SeparateWindow() : QMainWindow()
{
  // Create a new splitViewWidget and set it as center widget
  splitView = new splitViewWidget(this, true);
  setCentralWidget(splitView);
  splitView->setAttribute(Qt::WA_AcceptTouchEvents);

  connect(splitView, SIGNAL(signalToggleFullScreen()), this, SLOT(toggleFullscreen()));
  connect(splitView, SIGNAL(signalShowSeparateWindow(bool)), this, SLOT(splitViewShowSeparateWindow(bool)));
};

void SeparateWindow::toggleFullscreen()
{
  QSettings settings;
  if (isFullScreen())
  {
    // Show the window normal or maximized (depending on how it was shown before)
    if (showNormalMaximized)
      showMaximized();
    else
      showNormal();
  }
  else
  {
    // Save if the window is currently maximized
    showNormalMaximized = isMaximized();

    showFullScreen();
  }
}

void SeparateWindow::closeEvent(QCloseEvent *event)
{
  // This window cannot be closed. Signal that we want to go to single window mode.
  // The main window will then hide this window.
  event->ignore();
  emit signalSingleWindowMode();
}

void SeparateWindow::keyPressEvent(QKeyEvent * event)
{
  int key = event->key();
  bool control = (event->modifiers() & Qt::ControlModifier);

  if (key == Qt::Key_Escape)
  {
    if (isFullScreen())
      toggleFullscreen();
  }
  else if (key == Qt::Key_F && control)
    toggleFullscreen();
  else if (key == Qt::Key_Space)
    emit signalPlayPauseToggle();
  else if (key == Qt::Key_Right)
    emit signalNextFrame();
  else if (key == Qt::Key_Left)
    emit signalPreviousFrame();
  else if (key == Qt::Key_Down)
    emit signalNextItem();
  else if (key == Qt::Key_Up)
    emit signalPreviousItem();
  else
  {
    // See if the split view widget handles this key press. If not, pass the event on to the QWidget.
    if (!splitView->handleKeyPress(event))
      QWidget::keyPressEvent(event);
  }
}