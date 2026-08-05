#include "videoHandlerYUV.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QMainWindow>
#include <QStatusBar>
#include <QSettings>

#include <ui/widgets/PlaylistTreeWidget.h>
#include <ui/Mainwindow.h>

namespace video::yuv
{

/**
 * @brief Return the HDR render window pointer (delegates to HDRRenderingManager).
 * @return HDR QWindow pointer; may be null when inactive.
 */
QWindow* videoHandlerYUV::getHDRWindow() const
{
  return m_hdrRenderingManager->getHDRWindow();
}

/**
 * @brief Query whether HDR rendering is currently active.
 * @return true if the HDR render path is in use.
 */
bool videoHandlerYUV::isHDRRenderingActive() const
{
  return m_hdrRenderingManager->isHDRRenderingActive();
}

/**
 * @brief Show a UI notice that HDR setting changes require a restart.
 * @param hdrEnabled Current HDR checkbox state.
 */
void videoHandlerYUV::showHDRRestartNotice(bool hdrEnabled)
{
  // PRD Requirements 5.1 & 5.3: Show "(requires restart)" label/notice
  
  // Update checkbox text to show restart notice (PRD Requirements 5.1 & 5.3)
  QString baseText = "Enable native 10-bit display";
  QString noticeText = baseText + " (requires restart)";
  ui.checkBoxEnable10BitDisplay->setText(noticeText);
  
  // Show the Restart button to let user apply immediately
  if (ui.buttonRestartNow)
    ui.buttonRestartNow->setVisible(true);
  
  // Also show a non-blocking status message
  QWidget* parentWidget = ui.checkBoxEnable10BitDisplay->parentWidget();
  while (parentWidget && !parentWidget->inherits("QMainWindow")) {
    parentWidget = parentWidget->parentWidget();
  }
  
  if (parentWidget) {
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(parentWidget);
    if (mainWindow && mainWindow->statusBar()) {
      QString statusMessage = hdrEnabled ?
        "HDR mode enabled - Please restart YUView to apply changes" :
        "HDR mode disabled - Please restart YUView to apply changes";
      mainWindow->statusBar()->showMessage(statusMessage, 5000); // Show for 5 seconds
    }
  }
}

/**
 * @brief Clear the HDR restart notice, restore checkbox text, and hide Restart.
 */
void videoHandlerYUV::clearHDRRestartNotice()
{
  // Clear restart notice by restoring original checkbox text
  
  QString baseText = "Enable native 10-bit display";
  ui.checkBoxEnable10BitDisplay->setText(baseText);
  
  // Hide the Restart button on fresh startup (no pending change)
  if (ui.buttonRestartNow)
    ui.buttonRestartNow->setVisible(false);
}

/**
 * @brief Restart the app to apply HDR startup settings (ControlledRestart + playlist autosave).
 */
void videoHandlerYUV::slotRestartNow()
{
  // Ensure settings are flushed before restart
  QSettings settings;
  
  // Mark that this is a controlled restart so we suppress the crash-restore dialog
  settings.setValue("ControlledRestart", true);
  
  // Proactively autosave current playlist context for seamless restore
  // Use the same key as autosave mechanism uses
  {
    // Try to serialize current playlist via MainWindow's playlist widget
    QWidget* parentWidget = QApplication::activeWindow();
    while (parentWidget && !parentWidget->inherits("QMainWindow"))
      parentWidget = parentWidget->parentWidget();
    if (parentWidget)
    {
      MainWindow* mw = qobject_cast<MainWindow*>(parentWidget);
      if (mw)
      {
        auto playlist = mw->findChild<PlaylistTreeWidget*>();
        if (playlist)
        {
          // Use widget's immediate autosave utility for consistent format
          playlist->saveAutosaveNow();
        }
      }
    }
  }
  settings.sync();

  // Restart the application in a platform-agnostic way
  const QString program = QCoreApplication::applicationFilePath();
  QStringList args = QCoreApplication::arguments();
  if (!args.isEmpty())
    args.removeFirst(); // Remove program name

  bool started = QProcess::startDetached(program, args, QDir::currentPath());
  if (!started)
  {
    QMessageBox::warning(nullptr, "Restart failed", "YUView could not be restarted automatically. Please restart it manually.");
    return;
  }

  QApplication::quit();
}

} // namespace video::yuv
