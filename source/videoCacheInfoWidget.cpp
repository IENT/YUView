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

#include "videoCacheInfoWidget.h"

#include <QGroupBox>
#include <QPainter>
#include <QSettings>

#define VIDEOCACHEINFOWIDGET_DEBUG_OUTPUT 0
#if VIDEOCACHEINFOWIDGET_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_CACHINGINFO qDebug
#else
#define DEBUG_CACHINGINFO(fmt,...) ((void)0)
#endif

/// ----------------------- videoCacheStatusWidget -------------------------

using namespace videoCacheStatusWidgetNamespace;

void videoCacheStatusWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  
  const int width = size().width();
  const int height = size().height();

  // The list of colors that we choose the item colors from.
  static QList<QColor> colors = QList<QColor>()
    << QColor(33, 150, 243) // Blue
    << QColor(0, 150, 136)  // Teal
    << QColor(139, 195, 74) // Light Green
    << QColor(96, 125, 139) // Blue Grey
    << QColor(255, 193, 7)  // Amber
    << QColor(103, 58, 183) // Deep Purple
    << QColor(0, 188, 212)  // Cyan
    << QColor(156, 39, 176) // Purple
    << QColor(255, 87, 34)  // Deep Orange
    << QColor(3, 169, 244); // Light blue

  int xStart = 0;
  for (int i = 0; i < relativeValsEnd.count(); i++)
  {
    QColor c = colors.at(i % colors.count());
    float endVal = relativeValsEnd.at(i);
    int xEnd = int(endVal * width);
    painter.fillRect(xStart, 0, xEnd - xStart, height, c);

    // The old end value is the start value of the next rect
    xStart = xEnd + 1;
  }

  // Draw the fill status as text
  //painter.setBrush(palette().windowText());
  QString pTxt = QString("%1 MB / %2 MB / %3 KB/s").arg(cacheLevelMB).arg(cacheLevelMaxMB).arg(cacheRateInBytesPerMs);
  painter.drawText(0, 0, width, height, Qt::AlignCenter, pTxt);

  // Only draw the border
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(0, 0, width-1, height-1);
}

void videoCacheStatusWidget::updateStatus(PlaylistTreeWidget *playlist, unsigned int cacheRate)
{
  // Get all items from the playlist
  QList<playlistItem*> allItems = playlist->getAllPlaylistItems();

  // Get if caching is enabled and how much memory we can use for the cache
  QSettings settings;
  settings.beginGroup("VideoCache");
  cacheLevelMaxMB = settings.value("ThresholdValueMB", 49).toUInt();
  const int64_t cacheLevelMax = cacheLevelMaxMB * 1000 * 1000;
  settings.endGroup();

  // Clear the old percent values
  relativeValsEnd.clear();

  // Let's find out how much space in the cache is used.
  // In combination with cacheLevelMax we also know how much space is free.
  int64_t cacheLevel = 0;
  for (int i = 0; i < allItems.count(); i++)
  {
    playlistItem *item = allItems.at(i);
    int nrFrames = item->getNumberCachedFrames();
    unsigned int frameSize = item->getCachingFrameSize();
    int64_t itemCacheSize = nrFrames * frameSize;
    DEBUG_CACHINGINFO("videoCacheStatusWidget::updateStatus Item %d frames %d * size %d = %d", i, nrFrames, frameSize, (int)itemCacheSize);

    float endVal = (float)(cacheLevel + itemCacheSize) / cacheLevelMax;
    relativeValsEnd.append(endVal);
    cacheLevel += itemCacheSize;
  }

  // Save the values that will be shown as text
  cacheLevelMB = cacheLevel / 1000000;
  cacheRateInBytesPerMs = cacheRate;

  // Also redraw if the values were updated
  update();
}

/// ------------------------- VideoCacheInfoWidget -----------------------

VideoCacheInfoWidget::VideoCacheInfoWidget(QWidget *parent) : QWidget(parent)
{
  // Create the video cache status widget
  statusWidget = new videoCacheStatusWidget(this);
  statusWidget->setMinimumHeight(20);

  // Create a QGroupBox with text label inside
  QGroupBox *groupBox = new QGroupBox("Details", this);
  groupBox->setCheckable(true);
  QVBoxLayout *vbox = new QVBoxLayout;
  cachingInfoLabel = new QLabel("", this);
  cachingInfoLabel->setAlignment(Qt::AlignTop);
  vbox->addWidget(cachingInfoLabel);
  groupBox->setLayout(vbox);

  // Add everything to a vertical layout
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->addWidget(statusWidget);
  mainLayout->addWidget(groupBox, 1);

  setLayout(mainLayout);

  connect(groupBox, &QGroupBox::toggled, this, &VideoCacheInfoWidget::onGroupBoxToggled);
}

void VideoCacheInfoWidget::onGroupBoxToggled(bool on)
{
  if (cachingInfoLabel == nullptr)
    return;

  cachingInfoLabel->setVisible(on);
}

void VideoCacheInfoWidget::onUpdateCacheStatus()
{
  if (playlist == nullptr || cache == nullptr)
    return;
  if (!isVisible())
    return;

  playlist->updateCachingStatus();

  DEBUG_CACHINGINFO("VideoCacheInfoWidget::updateCacheStatus");
  statusWidget->updateStatus(playlist, cacheRateInBytesPerMs);

  QStringList statusText = cache->getCacheStatusText();
  cachingInfoLabel->setText(statusText.join("\n"));
}