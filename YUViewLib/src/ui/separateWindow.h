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

#ifndef SEPARATEWINDOW_H
#define SEPARATEWINDOW_H

#include <QMainWindow>

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
