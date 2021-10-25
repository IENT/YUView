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

#include "FunctionsGui.h"

#include <common/Functions.h>

#include <QSettings>

QColor functionsGui::toQColor(const Color &color)
{
  if (color.alpha() == -1)
    return QColor(color.R(), color.G(), color.B());
  return QColor(color.R(), color.G(), color.B(), color.A());
}

Color functionsGui::toColor(const QColor &color)
{
  if (color.alpha() == 255)
    return Color(color.red(), color.green(), color.blue());
  return Color(color.red(), color.green(), color.blue(), color.alpha());
}

QImage::Format functionsGui::pixmapImageFormat()
{
  static auto const format = QPixmap(1, 1).toImage().format();
  Q_ASSERT(format != QImage::Format_Invalid);
  return format;
}

QIcon functionsGui::convertIcon(QString iconPath)
{
  QSettings settings;
  QString   themeName = settings.value("Theme", "Default").toString();

  // Get the active and inactive colors
  QStringList colors = functions::getThemeColors(themeName);
  QRgb        activeColor, inActiveColor;
  if (colors.count() == 4)
  {
    QColor active(colors[1]);
    QColor inactive(colors[2]);
    activeColor   = active.rgb();
    inActiveColor = inactive.rgb();
  }
  else
  {
    activeColor   = qRgb(0, 0, 0);
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

QPixmap functionsGui::convertPixmap(QString pixmapPath)
{
  QSettings settings;
  QString   themeName = settings.value("Theme", "Default").toString();

  // Get the active and inactive colors
  QStringList colors = functions::getThemeColors(themeName);
  QRgb        activeColor;
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

QString functionsGui::pixelFormatToString(QImage::Format f)
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
