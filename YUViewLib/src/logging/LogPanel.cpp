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

#include "LogPanel.h"

#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QUrl>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

LogPanel::LogPanel(QWidget *parent) : QDialog(parent)
{
  setWindowTitle(tr("Log Viewer"));
  setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
  resize(900, 550);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(6);

  // ---- Log view ----
  logView = new QTextEdit(this);
  logView->setReadOnly(true);
  logView->document()->setMaximumBlockCount(MAX_DISPLAY_LINES);
  logView->setLineWrapMode(QTextEdit::NoWrap);
  QFont mono("Courier New", 9);
  mono.setStyleHint(QFont::Monospace);
  logView->setFont(mono);
  mainLayout->addWidget(logView, /*stretch=*/1);

  // ---- Minimum level selector ----
  auto *levelLayout = new QHBoxLayout;
  levelLayout->setContentsMargins(0, 0, 0, 0);
  auto *levelLabel = new QLabel(tr("Minimum level:"), this);
  levelLayout->addWidget(levelLabel);

  levelCombo = new QComboBox(this);
  levelCombo->addItem(tr("Debug"), static_cast<int>(LogLevel::Debug));
  levelCombo->addItem(tr("Info"), static_cast<int>(LogLevel::Info));
  levelCombo->addItem(tr("Warning"), static_cast<int>(LogLevel::Warning));
  levelCombo->addItem(tr("Critical"), static_cast<int>(LogLevel::Critical));
  levelCombo->addItem(tr("Fatal"), static_cast<int>(LogLevel::Fatal));
  levelCombo->setToolTip(tr("Messages below this level are discarded entirely "
                            "(neither written to file nor shown here)."));
  // Sync combo to current Logger level.
  {
    const auto cur = Logger::instance().minLevel();
    for (int i = 0; i < levelCombo->count(); ++i)
    {
      if (levelCombo->itemData(i).toInt() == static_cast<int>(cur))
      {
        levelCombo->setCurrentIndex(i);
        break;
      }
    }
  }
  connect(levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &LogPanel::onMinLevelChanged);
  levelLayout->addWidget(levelCombo);
  levelLayout->addStretch();
  mainLayout->addLayout(levelLayout);

  // ---- "Write to file" checkboxes ----
  auto *fileGroup  = new QGroupBox(tr("Write to file"), this);
  auto *fileLayout = new QVBoxLayout(fileGroup);
  fileLayout->setContentsMargins(8, 4, 8, 4);

  // Master switch
  fileWriteMasterCheck = new QCheckBox(tr("Enable file logging"), fileGroup);
  fileWriteMasterCheck->setChecked(Logger::instance().isFileWriteEnabled());
  fileWriteMasterCheck->setToolTip(tr("Master switch for writing log messages to disk. "
                                      "When off, no messages are written regardless of "
                                      "the per-category checkboxes below."));
  connect(fileWriteMasterCheck, &QCheckBox::toggled, this, &LogPanel::onFileWriteToggle);
  fileLayout->addWidget(fileWriteMasterCheck);

  // Per-category checkboxes (on a sub-row)
  auto *catRow = new QHBoxLayout;
  catRow->setContentsMargins(20, 0, 0, 0); // indent under master switch

  struct CatInfo
  {
    LogCategory cat;
    const char *label;
    const char *tooltip;
  };
  const CatInfo cats[] = {
      {LogCategory::App,
       "Application",
       "General qDebug / qWarning / qCritical messages from application code"},
      {LogCategory::FFmpeg,
       "FFmpeg",
       "FFmpeg library log messages (errors, warnings, codec info)"},
  };

  for (const auto &info : cats)
  {
    const int idx    = static_cast<int>(info.cat);
    auto     *cb     = new QCheckBox(tr(info.label), fileGroup);
    cb->setChecked(Logger::instance().isCategoryFileEnabled(info.cat));
    cb->setToolTip(tr(info.tooltip));
    cb->setEnabled(Logger::instance().isFileWriteEnabled());
    checkboxes[idx] = cb;

    connect(cb, &QCheckBox::stateChanged, this,
            [this, cat = info.cat](int state)
            { onCategoryCheckChanged(cat, state == Qt::Checked); });

    catRow->addWidget(cb);
  }
  catRow->addStretch();
  fileLayout->addLayout(catRow);
  mainLayout->addWidget(fileGroup);

  // ---- Buttons ----
  auto *btnLayout = new QHBoxLayout;
  btnLayout->setContentsMargins(0, 0, 0, 0);

  auto *clearBtn = new QPushButton(tr("Clear"), this);
  clearBtn->setToolTip(tr("Clear the log view (does not affect the log file)"));
  connect(clearBtn, &QPushButton::clicked, this, &LogPanel::onClearClicked);
  btnLayout->addWidget(clearBtn);

  auto *cleanBtn = new QPushButton(tr("Clean old logs"), this);
  cleanBtn->setToolTip(tr("Delete old log files, keeping only the most recent few."));
  connect(cleanBtn, &QPushButton::clicked, this, &LogPanel::onCleanOldLogsClicked);
  btnLayout->addWidget(cleanBtn);

  btnLayout->addStretch();

  auto *folderBtn = new QPushButton(tr("Open Log Folder"), this);
  folderBtn->setToolTip(tr("Open the directory containing log files"));
  connect(folderBtn, &QPushButton::clicked, this, &LogPanel::onOpenLogFolderClicked);
  btnLayout->addWidget(folderBtn);

  mainLayout->addLayout(btnLayout);

  // ---- Register UI callback with Logger ----
  // The callback is invoked from arbitrary threads, so we post back to the
  // UI thread via Qt::QueuedConnection.
  //
  // Lifetime safety: `this` is passed as the context/receiver argument to
  // invokeMethod (first positional argument in the Qt6 functor overload).
  // Qt6 stores this internally and, when the LogPanel is destroyed,
  // QObject::~QObject() calls QCoreApplication::removePostedEvents(this),
  // which discards all pending MetaCall events for this object before the
  // memory is freed.  The destructor also calls clearUiCallback() under the
  // Logger mutex, preventing new callbacks from being queued after that point.
  Logger::instance().setUiCallback(
      [this](LogCategory /*cat*/, const QString &line)
      {
        QMetaObject::invokeMethod(
            this, [this, line]() { appendLine(line); }, Qt::QueuedConnection);
      });
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

LogPanel::~LogPanel()
{
  Logger::instance().clearUiCallback();
}

// ---------------------------------------------------------------------------
// appendLine
// ---------------------------------------------------------------------------

void LogPanel::appendLine(const QString &line)
{
  logView->append(line);
  // Auto-scroll to bottom.
  auto *sb = logView->verticalScrollBar();
  sb->setValue(sb->maximum());
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void LogPanel::onCategoryCheckChanged(LogCategory cat, bool checked)
{
  Logger::instance().setCategoryFileEnabled(cat, checked);
}

void LogPanel::onMinLevelChanged(int index)
{
  const auto level = static_cast<LogLevel>(levelCombo->itemData(index).toInt());
  Logger::instance().setMinLevel(level);
}

void LogPanel::onFileWriteToggle(bool checked)
{
  Logger::instance().setFileWriteEnabled(checked);
  // Enable/disable the per-category checkboxes to reflect the master state.
  for (auto *cb : checkboxes)
  {
    if (cb)
      cb->setEnabled(checked);
  }
}

void LogPanel::onClearClicked()
{
  logView->clear();
}

void LogPanel::onCleanOldLogsClicked()
{
  Logger::instance().cleanOldLogs();
  QMessageBox::information(this, tr("Clean old logs"),
                           tr("Old log files have been cleaned up. "
                              "Only the most recent log files are kept."));
}

void LogPanel::onOpenLogFolderClicked()
{
  QDesktopServices::openUrl(QUrl::fromLocalFile(Logger::instance().logDirectory()));
}
