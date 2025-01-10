/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "StatisticsFileVTMBMS.h"

#include <QRegularExpression>
#include <QTextStream>

#include <iostream>

namespace stats
{

// The internal buffer for parsing the starting positions. The buffer must not be larger than 2GB
// so that we can address all the positions in it with int (using such a large buffer is not a good
// idea anyways)
constexpr unsigned STAT_PARSING_BUFFER_SIZE = 1048576u;
constexpr unsigned STAT_MAX_STRING_SIZE     = 1u << 28;

StatisticsFileVTMBMS::StatisticsFileVTMBMS(const QString &filename, StatisticsData &statisticsData)
    : StatisticsFileBase(filename)
{
  this->readHeaderFromFile(statisticsData);
}

/** The background task that parses the file and extracts the exact file positions
 * where a new frame starts. If the user then later requests this POC
 * we can directly jump there and parse the actual information. This way we don't have to
 * scan the whole file which can get very slow for large files.
 *
 * This function might emit the objectInformationChanged() signal if something went wrong,
 * setting the error message, or if parsing finished successfully.
 */
void StatisticsFileVTMBMS::readFrameAndTypePositionsFromFile(std::atomic_bool &breakFunction)
{
  try
  {
    // Open the file (again). Since this is a background process, we open the file again to
    // not disturb any reading from not background code.
    FileSource inputFile;
    if (!inputFile.openFile(this->file.getAbsoluteFilePath()))
      return;

    // We perform reading using an input buffer
    QByteArray inputBuffer;
    bool       fileAtEnd      = false;
    uint64_t   bufferStartPos = 0;

    QString  lineBuffer;
    uint64_t lineBufferStartPos = 0;
    int      lastPOC            = INT_INVALID;
    bool     sortingFixed       = false;

    while (!fileAtEnd && !breakFunction.load() && !this->abortParsingDestroy)
    {
      // Fill the buffer
      auto bufferSize = inputFile.readBytes(inputBuffer, bufferStartPos, STAT_PARSING_BUFFER_SIZE);
      if (bufferSize < 0)
        throw "Error reading bytes";
      if (bufferSize < STAT_PARSING_BUFFER_SIZE)
        // Less bytes than the maximum buffer size were read. The file is at the end.
        // This is the last run of the loop.
        fileAtEnd = true;
      // a corrupted file may contain an arbitrary amount of non-\n symbols
      // prevent lineBuffer overflow by dumping it for such cases
      if (unsigned(lineBuffer.size()) > STAT_MAX_STRING_SIZE)
        lineBuffer.clear(); // prevent an overflow here
      for (size_t i = 0; i < size_t(bufferSize); i++)
      {
        // Search for '\n' newline characters
        if (inputBuffer.at(int(i)) == 10)
        {
          // We found a newline character
          if (lineBuffer.size() > 0)
          {
            // Parse the previous line
            // get components of this line
            // get poc using regular expression
            // need to match this:
            // BlockStat: POC 1 @( 120,  80) [ 8x 8] MVL0={ -24,  -2}
            // BlockStat: POC 1 @( 112,  88) [ 8x 8] PredMode=0
            QRegularExpression pocRegex("BlockStat: POC ([0-9]+)");
            auto               match = pocRegex.match(lineBuffer);
            // ignore not matching lines
            if (match.hasMatch())
            {
              auto poc = match.captured(1).toInt();

              if (lastPOC == -1)
              {
                // First POC
                this->pocStartList[poc] = lineBufferStartPos;
                emit readPOC(poc);

                lastPOC = poc;

                // update number of frames
                if (poc > this->maxPOC)
                  this->maxPOC = poc;
              }
              else if (poc != lastPOC)
              {
                // this is apparently not sorted by POCs and we will not check it further
                if (!sortingFixed)
                  sortingFixed = true;

                lastPOC                 = poc;
                this->pocStartList[poc] = lineBufferStartPos;
                emit readPOC(poc);

                // update number of frames
                if (poc > this->maxPOC)
                  this->maxPOC = poc;

                // Update percent of file parsed
                if (const auto fileSize = inputFile.getFileSize())
                  this->parsingProgress = (static_cast<double>(lineBufferStartPos) * 100 /
                                           static_cast<double>(*fileSize));
              }
            }
          }

          lineBuffer.clear();
          lineBufferStartPos = bufferStartPos + i + 1;
        }
        else
        {
          // No newline character found
          lineBuffer.append(inputBuffer.at(int(i)));
        }
      }

      bufferStartPos += bufferSize;
    }

    // Parsing complete
    this->parsingProgress = 100.0;
  }
  catch (const char *str)
  {
    std::cerr << "Error while parsing meta data: " << str << "\n";
    this->errorMessage = QString("Error while parsing meta data: ") + QString(str);
    this->error        = true;
    return;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Error while parsing:" << ex.what() << "\n";
    this->errorMessage = QString("Error while parsing: ") + QString(ex.what());
    this->error        = true;
    return;
  }

  return;
}

void StatisticsFileVTMBMS::loadStatisticData(StatisticsData &statisticsData, int poc, int typeID)
{
}

void StatisticsFileVTMBMS::readHeaderFromFile(StatisticsData &statisticsData)
{
}

} // namespace stats
