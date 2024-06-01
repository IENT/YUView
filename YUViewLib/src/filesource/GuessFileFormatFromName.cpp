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

#include "GuessFileFormatFromName.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

GuessResult guessFormatFromFilename(const QString &fullFilePath)
{
  GuessResult format;

  // We are going to check two strings (one after the other) for indicators on the frames size, fps
  // and bit depth. 1: The file name, 2: The folder name that the file is contained in.

  QFileInfo fileInfo(fullFilePath);
  auto      dirName  = fileInfo.absoluteDir().dirName();
  auto      fileName = fileInfo.fileName();
  if (fileName.isEmpty())
    return format;

  for (auto const &name : {fileName, dirName})
  {
    // First, we will try to get a frame size from the name
    if (!format.frameSize.isValid())
    {
      // The regular expressions to match. They are sorted from most detailed to least so that the
      // most detailed ones are tested first.
      auto regExprList =
          QStringList()
          << "([0-9]+)(?:x|X|\\*)([0-9]+)_([0-9]+)(?:Hz)?_([0-9]+)b?[\\._]" // Something_2160x1440_60_8_more.yuv
                                                                            // or
                                                                            // Something_2160x1440_60_8b.yuv
                                                                            // or
                                                                            // Something_2160x1440_60Hz_8_more.yuv
          << "([0-9]+)(?:x|X|\\*)([0-9]+)_([0-9]+)(?:Hz)?[\\._]" // Something_2160x1440_60_more.yuv
                                                                 // or Something_2160x1440_60.yuv
          << "([0-9]+)(?:x|X|\\*)([0-9]+)[\\._]";                // Something_2160x1440_more.yuv or
                                                                 // Something_2160x1440.yuv

      for (auto regExpStr : regExprList)
      {
        QRegularExpression exp(regExpStr);
        auto               match = exp.match(name);
        if (match.hasMatch())
        {
          auto widthString  = match.captured(1);
          auto heightString = match.captured(2);
          format.frameSize  = Size(widthString.toInt(), heightString.toInt());

          auto rateString = match.captured(3);
          if (!rateString.isEmpty())
            format.frameRate = rateString.toDouble();

          auto bitDepthString = match.captured(4);
          if (!bitDepthString.isEmpty())
            format.bitDepth = bitDepthString.toUInt();

          break; // Don't check the following expressions
        }
      }
    }

    // try resolution indicators with framerate: "1080p50", "720p24" ...
    if (!format.frameSize.isValid())
    {
      QRegularExpression rx1080p("1080p([0-9]+)");
      auto               matchIt = rx1080p.globalMatch(name);
      if (matchIt.hasNext())
      {
        auto match           = matchIt.next();
        format.frameSize     = Size(1920, 1080);
        auto frameRateString = match.captured(1);
        format.frameRate     = frameRateString.toInt();
      }
    }
    if (!format.frameSize.isValid())
    {
      QRegularExpression rx720p("720p([0-9]+)");
      auto               matchIt = rx720p.globalMatch(name);
      if (matchIt.hasNext())
      {
        auto match           = matchIt.next();
        format.frameSize     = Size(1280, 720);
        auto frameRateString = match.captured(1);
        format.frameRate     = frameRateString.toInt();
      }
    }

    if (!format.frameSize.isValid())
    {
      // try to find resolution indicators (e.g. 'cif', 'hd') in file name
      if (name.contains("_cif", Qt::CaseInsensitive))
        format.frameSize = Size(352, 288);
      else if (name.contains("_qcif", Qt::CaseInsensitive))
        format.frameSize = Size(176, 144);
      else if (name.contains("_4cif", Qt::CaseInsensitive))
        format.frameSize = Size(704, 576);
      else if (name.contains("UHD", Qt::CaseSensitive))
        format.frameSize = Size(3840, 2160);
      else if (name.contains("HD", Qt::CaseSensitive))
        format.frameSize = Size(1920, 1080);
      else if (name.contains("1080p", Qt::CaseSensitive))
        format.frameSize = Size(1920, 1080);
      else if (name.contains("720p", Qt::CaseSensitive))
        format.frameSize = Size(1280, 720);
    }

    // Second, if we were able to get a frame size but no frame rate, we will try to get a frame
    // rate.
    if (format.frameSize.isValid() && format.frameRate == -1)
    {
      // Look for: 24fps, 50fps, 24FPS, 50FPS
      QRegularExpression rxFPS("([0-9]+)fps", QRegularExpression::CaseInsensitiveOption);
      auto               match = rxFPS.match(name);
      if (match.hasMatch())
      {
        auto frameRateString = match.captured(1);
        format.frameRate     = frameRateString.toInt();
      }
    }
    if (format.frameSize.isValid() && format.frameRate == -1)
    {
      // Look for: 24Hz, 50Hz, 24HZ, 50HZ
      QRegularExpression rxHZ("([0-9]+)HZ", QRegularExpression::CaseInsensitiveOption);
      auto               match = rxHZ.match(name);
      if (match.hasMatch())
      {
        QString frameRateString = match.captured(1);
        format.frameRate        = frameRateString.toInt();
      }
    }

    // Third, if we were able to get a frame size but no bit depth, we try to get a bit depth.
    if (format.frameSize.isValid() && format.bitDepth == 0)
    {
      for (unsigned bd : {8, 9, 10, 12, 16})
      {
        // Look for: 10bit, 10BIT, 10-bit, 10-BIT
        if (name.contains(QString("%1bit").arg(bd), Qt::CaseInsensitive) ||
            name.contains(QString("%1-bit").arg(bd), Qt::CaseInsensitive))
        {
          format.bitDepth = bd;
          break;
        }
        // Look for bit depths like: _16b_ .8b. -12b-
        QRegularExpression exp(QString("(?:_|\\.|-)%1b(?:_|\\.|-)").arg(bd));
        auto               match = exp.match(name);
        if (match.hasMatch())
        {
          format.bitDepth = bd;
          break;
        }
      }
    }

    // If we were able to get a frame size, try to get an indicator for packed formats
    if (format.frameSize.isValid())
    {
      QRegularExpression exp("(?:_|\\.|-)packed(?:_|\\.|-)");
      auto               match = exp.match(name);
      if (match.hasMatch())
        format.packed = true;
    }
  }

  return format;
}
