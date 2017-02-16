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

#include "typedef.h"

#ifdef Q_OS_MAC
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#elif defined(Q_OS_WIN32)
#include <windows.h>
#endif
#include <QLayout>
#include <QThread>
#include <QWidget>

static void unparentWidgets(QLayout *layout)
{
  const int n = layout->count();
  for (int i = 0; i < n; ++i) {
    QLayoutItem *item = layout->itemAt(i);
    if (item->widget()) item->widget()->setParent(0);
    else if (item->layout()) unparentWidgets(item->layout());
  }
}

// See also http://stackoverflow.com/q/40497358/1329652
void setupUi(void *ui, void(*setupUi)(void *ui, QWidget *widget))
{
  QWidget widget;
  setupUi(ui, &widget);
  QLayout *wrapperLayout = widget.layout();
  Q_ASSERT(wrapperLayout);
  QObjectList const wrapperChildren = wrapperLayout->children();
  Q_ASSERT(wrapperChildren.size() == 1);
  QLayout *topLayout = qobject_cast<QLayout *>(wrapperChildren.first());
  Q_ASSERT(topLayout);
  topLayout->setParent(0);
  delete wrapperLayout;
  unparentWidgets(topLayout);
  Q_ASSERT(widget.findChildren<QObject*>().isEmpty());
}

QImage::Format pixmapImageFormat()
{
  static auto const format = QPixmap(1,1).toImage().format();
  Q_ASSERT(format != QImage::Format_Invalid);
  return format;
}

unsigned int getOptimalThreadCount()
{
  int nrThreads = QThread::idealThreadCount() - 1;
  if (nrThreads > 0)
    return (unsigned int)nrThreads;
  else
    return 1;
}

unsigned int systemMemorySizeInMB()
{
  static unsigned int memorySizeInMB;
  if (!memorySizeInMB)
  {
    // Fetch size of main memory - assume 2 GB first.
    // Unfortunately there is no Qt API for doing this so this is platform dependent.
    memorySizeInMB = 2 << 10;
  #ifdef Q_OS_MAC
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    u_int namelen = sizeof(mib) / sizeof(mib[0]);
    uint64_t size;
    size_t len = sizeof(size);

    if (sysctl(mib, namelen, &size, &len, NULL, 0) == 0)
      memorySizeInMB = size >> 20;
  #elif defined Q_OS_UNIX
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    memorySizeInMB = (pages * page_size) >> 20;
  #elif defined Q_OS_WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    memorySizeInMB = status.ullTotalPhys >> 20;
  #endif
  }
  return memorySizeInMB;
}
