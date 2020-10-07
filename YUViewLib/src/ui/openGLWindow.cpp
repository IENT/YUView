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

#include "openGLWindow.h"

#include <QSettings>

OpenGLWindow::OpenGLWindow() :
  openGLView(this)
{


    // https://doc.qt.io/qt-5/qopenglwidget.html
    //As described above, it is simpler and more robust to set the requested format globally so
    //  that it applies to all windows and contexts during the lifetime of the application.
      QSurfaceFormat format;
      // asks for a OpenGL 3.2 debug context using the Core profile
      format.setMajorVersion(3);
      format.setMinorVersion(3);
      format.setDepthBufferSize(24);
      format.setStencilBufferSize(8);
      // Setting an interval value of 0 will turn the vertical refresh syncing off, any value
      // higher than 0 will turn the vertical syncing on. Setting interval to a higher value,
      // for example 10, results in having 10 vertical retraces between every buffer swap.
      format.setSwapInterval(0); // no effect in windows
      format.setProfile(QSurfaceFormat::CoreProfile);
      format.setOption(QSurfaceFormat::DebugContext);
      QSurfaceFormat::setDefaultFormat(format);


    setCentralWidget(&openGLView);
//  splitView.setAttribute(Qt::WA_AcceptTouchEvents);



//  connect(&splitView, &splitViewWidget::signalToggleFullScreen, this, &OpenGLWindow::toggleFullscreen);
//  connect(&splitView, &splitViewWidget::signalShowOpenGLWindow, this, &OpenGLWindow::splitViewShowOpenGLWindow);
}

void OpenGLWindow::toggleFullscreen()
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

void OpenGLWindow::handleOepnGLLoggerMessages( QOpenGLDebugMessage message )
{
    qDebug() << message;
}

void OpenGLWindow::keyPressEvent(QKeyEvent *event)
{
  int key = event->key();
  bool controlOnly = (event->modifiers() == Qt::ControlModifier);

  if (key == Qt::Key_Escape)
  {
    if (isFullScreen())
      toggleFullscreen();
  }
  else if (key == Qt::Key_F && controlOnly)
    toggleFullscreen();
  else if (key == Qt::Key_G && controlOnly)
    close();
  else
  {
    // pass the event on to the QWidget.
    QWidget::keyPressEvent(event);
  }
}
