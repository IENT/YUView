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

#include "YUViewApplication.h"
#include <logging/Macros.h>

#include <common/Typedef.h>
#include <crash/CrashHandler.h>
#include <handler/SingleInstanceHandler.h>
#include <logging/Logger.h>
#include <ui/Mainwindow.h>

#include <QApplication>
#include <QSettings>
#include <QStandardPaths>

// Debug output routes through the logApp category, toggleable
// at runtime via QT_LOGGING_RULES.
#define DEBUG_APP(msg) LOG_DEBUG(logApp) << msg

YUViewApplication::YUViewApplication(int argc, char *argv[]) : QApplication(argc, argv)
{
  QString versionString = QString::fromUtf8(YUVIEW_VERSION);
  setApplicationName("YUView");
  setApplicationVersion(versionString);

  // -------------------------------------------------------------------------
  // Logging + crash handler setup.
  // Must be done after setApplicationName / setOrganizationName so that
  // QStandardPaths resolves to the correct directory.
  // -------------------------------------------------------------------------
  setOrganizationName("Institut für Nachrichtentechnik, RWTH Aachen University");
  setOrganizationDomain("ient.rwth-aachen.de");

  Logger::instance().init();
  CrashHandler::instance().init(Logger::instance().logDirectory());
#ifdef Q_OS_LINUX
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
  QGuiApplication::setDesktopFileName("YUView");
#endif
#endif

  QStringList args = arguments();
  DEBUG_APP("YUViewApplication args" << args);

  std::unique_ptr<singleInstanceHandler> instance;
  if (WIN_LINUX_SINGLE_INSTANCE && (is_Q_OS_WIN || is_Q_OS_LINUX))
  {
    // On mac, we can use the singleInstanceHandler. However, these don't work on windows and linux.
    instance.reset(new singleInstanceHandler);
    QString appName = "YUView.ient.rwth-aachen.de";
    if (instance->isRunning(appName, args.mid(1)))
    {
      // An instance is already running and we passed our command line arguments to it.
      DEBUG_APP("YUViewApplication sent command line to other instance");
      return;
    }

    // This is the first instance of the program
    instance->listen(appName);
  }

  // For Qt 5.8 there is a Bug in Qt that crashes the application if a certain type of proxy server
  // is used. With the -noUpdate parameter, we can disable automatic updates so that YUView can be
  // used normally.
  if (args.size() == 2 && args.last() == "-noUpdate")
  {
    QSettings settings;
    settings.beginGroup("updates");
    settings.setValue("checkForUpdates", false);
    settings.endGroup();
    DEBUG_APP("YUViewApplication automatic updates disabled");
  }

  bool alternativeUpdateSource = false;
  if (is_Q_OS_WIN && args.size() == 2 &&
      (args.last() == "-debugUpdateFromTestDeploy" || args.last() == "updateElevatedAltSource"))
  {
    // Do an update from the alternative URL. This way we can test upcoming updates from
    // an alternative source before deploying it to everybody.
    alternativeUpdateSource = true;
    DEBUG_APP("YUViewApplication update from alternate URL");
  }

  MainWindow w(alternativeUpdateSource);
  installEventFilter(&w);

  // If another application is opened, we will just add the given file to the playlist.
  if (WIN_LINUX_SINGLE_INSTANCE && (is_Q_OS_WIN || is_Q_OS_LINUX))
    w.connect(instance.get(), &singleInstanceHandler::newAppStarted, &w, &MainWindow::loadFiles);

  if (UPDATE_FEATURE_ENABLE && is_Q_OS_WIN && args.size() == 2 &&
      (args.last() == "updateElevated" || args.last() == "updateElevatedAltSource"))
  {
    // The process should now be elevated and we will force an update
    w.forceUpdateElevated();
    args.removeLast();
    DEBUG_APP("YUViewApplication update being elevated");
  }
  else
    w.autoUpdateCheck();

  QStringList fileList = args.mid(1);
  if (!fileList.empty())
  {
    DEBUG_APP("YUViewApplication loading file list " << fileList);
    w.loadFiles(fileList);
  }

  w.show();
  returnCode = exec();

  // Explicitly shut down the Logger before the QApplication is destroyed.
  // This avoids relying on the Logger singleton's destructor, which runs
  // during static destruction when Qt may already be torn down.
  Logger::instance().shutdown();
}

bool YUViewApplication::notify(QObject *receiver, QEvent *event)
{
  // Wrap every Qt event dispatch in a try/catch so that an unhandled C++
  // exception thrown inside an event handler is caught here rather than
  // propagating out to std::terminate (which would skip our crash handler
  // on some platforms / compilers).
  try
  {
    return QApplication::notify(receiver, event);
  }
  catch (const std::exception &e)
  {
    qCritical("Unhandled exception in event handler: %s", e.what());
    throw; // Re-throw so std::set_terminate (in CrashHandler) can log it.
  }
  catch (...)
  {
    qCritical("Unhandled unknown exception in event handler");
    throw;
  }
}