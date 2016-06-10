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

#include "yuviewapp.h"

YUViewApp::YUViewApp( int & argc, char **argv ) : QApplication(argc, argv)
{
  //printf("Build Version: %s \n",YUVIEW_HASH);
  // check the YUVIEW_HASH against the JSON output from here:
  // https://api.github.com/repos/IENT/YUView/commits
  // if comparison mismatch, there is a new version available!

  QString versionString = QString::fromUtf8(YUVIEW_VERSION);

  QApplication::setApplicationName("YUView");
  QApplication::setApplicationVersion(versionString);
  QApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents,false);
  QApplication::setAttribute(Qt::AA_SynthesizeTouchForUnhandledMouseEvents,false);
  QApplication::setOrganizationName("Institut für Nachrichtentechnik, RWTH Aachen University");
  QApplication::setOrganizationDomain("ient.rwth-aachen.de");

  w = new MainWindow();

  QStringList fileList;
  for ( int i = 1; i < argc; ++i )
    fileList.append( QString(argv[i]) );
  if (!fileList.empty())
    w->loadFiles(fileList);

  w->show();
}

bool YUViewApp::event(QEvent *event)
{
  QStringList fileList;

  switch (event->type())
  {
  case QEvent::FileOpen:
    fileList.append((static_cast<QFileOpenEvent *>(event))->file());
    w->loadFiles( fileList );
    return true;
  default:
    return QApplication::event(event);
  }
}

int main(int argc, char *argv[])
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // DPI support
#endif

  YUViewApp a(argc, argv);
  return a.exec();
}
