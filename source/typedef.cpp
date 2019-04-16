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
#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QLayout>
#include <QSettings>
#include <QThread>
#include <QWidget>

QString QDomElementYUView::findChildValue(const QString &tagName, QStringPairList &attributeList) const
{
  for (QDomNode n = firstChild(); !n.isNull(); n = n.nextSibling())
    if (n.isElement() && n.toElement().tagName() == tagName)
    {
      QDomNamedNodeMap attributes = n.toElement().attributes();
      for (int i = 0; i < attributes.length(); i++)
      {
        QString name = attributes.item(i).nodeName();
        QString val  = attributes.item(i).nodeValue();
        attributeList.append(QStringPair(name, val));
      }
      return n.toElement().text();
    }
  return QString();
}

QString getInputFormatName(inputFormat i)
{
  if (i == inputInvalid || i == input_NUM)
    return "";
  QStringList l = QStringList() << "annexBHEVC" << "annexBAVC" << "FFmpeg";
  return l.at((int)i);
}

inputFormat getInputFormatFromName(QString name)
{
  QStringList l = QStringList() << "annexBHEVC" << "annexBAVC" << "FFmpeg";
  int idx = l.indexOf(name);
  return (idx < 0 || idx >= input_NUM) ? inputInvalid : (inputFormat)idx;
}

QString getDecoderEngineName(decoderEngine e)
{
  if (e <= decoderEngineInvalid || e >= decoderEngineNum)
    return "";
  QStringList l = QStringList() << "libDe265" << "HM" << "Dav1d" << "FFmpeg";
  return l.at((int)e);
}

decoderEngine getDecoderEngineFromName(QString name)
{
  QStringList l = QStringList() << "libDe265" << "HM" << "Dav1d" << "FFmpeg";
  int idx = l.indexOf(name);
  return (idx < 0 || idx >= decoderEngineNum) ? decoderEngineInvalid : (decoderEngine)idx;
}

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

QIcon convertIcon(QString iconPath)
{
  QSettings settings;
  QString themeName = settings.value("Theme", "Default").toString();
  
  // Get the active and inactive colors
  QStringList colors = getThemeColors(themeName);
  QRgb activeColor, inActiveColor;
  if (colors.count() == 4)
  {
    QColor active(colors[1]);
    QColor inactive(colors[2]);
    activeColor = active.rgb();
    inActiveColor = inactive.rgb();
  }
  else
  {
    activeColor = qRgb(0, 0, 0);
    inActiveColor = qRgb(128, 128, 128);
  }

  // Color the icon in the active/inactive colors
  QImage input(iconPath);

  QImage active(input.size(), input.format());
  QImage inActive(input.size(), input.format());
  for (int y = 0; y < input.height(); y++)
  {
    for (int x = 0; x < input.width(); x++)
    {
      QRgb in = input.pixel(x, y);
      if (qAlpha(in) != 0)
      {
        active.setPixel(x, y, activeColor);
        inActive.setPixel(x, y, inActiveColor);
      }
      else
      {
        active.setPixel(x, y, in);
        inActive.setPixel(x, y, in);
      }
    }
  }
  
  QIcon outIcon;
  outIcon.addPixmap(QPixmap::fromImage(active), QIcon::Normal);
  outIcon.addPixmap(QPixmap::fromImage(inActive), QIcon::Disabled);

  return outIcon;
}

QPixmap convertPixmap(QString pixmapPath)
{
  QSettings settings;
  QString themeName = settings.value("Theme", "Default").toString();

  // Get the active and inactive colors
  QStringList colors = getThemeColors(themeName);
  QRgb activeColor;
  if (colors.count() == 4)
  {
    QColor active(colors[1]);
    activeColor = active.rgb();
  }
  else
    activeColor = qRgb(0, 0, 0);

  QImage input(pixmapPath);

  QImage active(input.size(), input.format());
  for (int y = 0; y < input.height(); y++)
  {
    for (int x = 0; x < input.width(); x++)
    {
      QRgb in = input.pixel(x, y);
      if (qAlpha(in) != 0)
        active.setPixel(x, y, activeColor);
      else
        active.setPixel(x, y, in);
    }
  }

  return QPixmap::fromImage(active);
}

QStringList getThemeNameList()
{
  QStringList ret;
  ret.append("Default");
  ret.append("Simple Dark/Blue");
  ret.append("Simple Dark/Orange");
  return ret;
}

QString getThemeFileName(QString themeName)
{
  if (themeName == "Simple Dark/Blue" || themeName == "Simple Dark/Orange")
    return ":YUViewSimple.qss";
  return "";
}

QStringList getThemeColors(QString themeName)
{
  if (themeName == "Simple Dark/Blue")
    return QStringList() << "#262626" << "#E0E0E0" << "#808080" << "#3daee9";
  if (themeName == "Simple Dark/Orange")
    return QStringList() << "#262626" << "#E0E0E0" << "#808080" << "#FFC300 ";
  return QStringList();
}

QString pixelFormatToString(QImage::Format f)
{
  if (f == QImage::Format_Invalid)
    return "Format_Invalid";
  if (f == QImage::Format_Mono)
    return "Format_Mono";
  if (f == QImage::Format_MonoLSB)
    return "Format_MonoLSB";
  if (f == QImage::Format_Indexed8)
    return "Format_Indexed8";
  if (f == QImage::Format_RGB32)
    return "Format_RGB32";
  if (f == QImage::Format_ARGB32)
    return "Format_ARGB32";
  if (f == QImage::Format_ARGB32_Premultiplied)
    return "Format_ARGB32_Premultiplied";
  if (f == QImage::Format_RGB16)
    return "Format_RGB16";
  if (f == QImage::Format_ARGB8565_Premultiplied)
    return "Format_ARGB8565_Premultiplied";
  if (f == QImage::Format_RGB666)
    return "Format_RGB666";
  if (f == QImage::Format_ARGB6666_Premultiplied)
    return "Format_ARGB6666_Premultiplied";
  if (f == QImage::Format_RGB555)
    return "Format_RGB555";
  if (f == QImage::Format_ARGB8555_Premultiplied)
    return "Format_ARGB8555_Premultiplied";
  if (f == QImage::Format_RGB888)
    return "Format_RGB888";
  if (f == QImage::Format_RGB444)
    return "Format_RGB444";
  if (f == QImage::Format_ARGB4444_Premultiplied)
    return "Format_ARGB4444_Premultiplied";
  if (f == QImage::Format_RGBX8888)
    return "Format_RGBX8888";
  if (f == QImage::Format_RGBA8888)
    return "Format_RGBA8888";
  if (f == QImage::Format_RGBA8888_Premultiplied)
    return "Format_RGBA8888_Premultiplied";
  if (f == QImage::Format_BGR30)
    return "Format_BGR30";
  if (f == QImage::Format_A2BGR30_Premultiplied)
    return "Format_A2BGR30_Premultiplied";
  if (f == QImage::Format_RGB30)
    return "Format_RGB30";
  if (f == QImage::Format_A2RGB30_Premultiplied)
    return "Format_A2RGB30_Premultiplied";
  if (f == QImage::Format_Alpha8)
    return "Format_Alpha8";
  if (f == QImage::Format_Grayscale8)
    return "Format_Grayscale8";
  return "Unknown";
}