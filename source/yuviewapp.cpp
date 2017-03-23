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

#include "mainwindow.h"
#include "typedef.h"

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling); // DPI support
#endif
  QApplication app(argc, argv);

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

  MainWindow w;
  app.installEventFilter(&w);

  QStringList args = app.arguments();

#if UPDATE_FEATURE_ENABLE && _WIN32
  if (args.size() == 2 && args[1] == "updateElevated")
  {
    // The process should now be elevated and we will force an update
    w.forceUpdateElevated();
    args.removeLast();
  }
  else
  {
    if (w.autoUpdateCheck())
      return 0;
  }
#else
  w.autoUpdateCheck();
#endif

  QStringList fileList = args.mid(1);
  if (!fileList.empty())
    w.loadFiles(fileList);

  w.show();
  return app.exec();
}
