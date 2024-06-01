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

#include "StatisticsFileCSV.h"

#include <QTextStream>
#include <iostream>

namespace stats
{

namespace
{

// The internal buffer for parsing the starting positions. The buffer must not be larger than 2GB
// so that we can address all the positions in it with int (using such a large buffer is not a good
// idea anyways)
constexpr unsigned STAT_PARSING_BUFFER_SIZE = 1048576u;
constexpr unsigned STAT_MAX_STRING_SIZE     = 1u << 28;

QStringList parseCSVLine(const QString &srcLine, char delimiter)
{
  // first, trim newline and white spaces from both ends of line
  QString line = srcLine.trimmed().remove(' ');
  // now split string with delimiter
  return line.split(delimiter);
}

} // namespace

StatisticsFileCSV::StatisticsFileCSV(const QString &filename, StatisticsData &statisticsData)
    : StatisticsFileBase(filename)
{
  this->readHeaderFromFile(statisticsData);
}

/** The background task that parses the file and extracts the exact file positions
 * where a new frame or a new type starts. If the user then later requests this type/POC
 * we can directly jump there and parse the actual information. This way we don't have to
 * scan the whole file which can get very slow for large files.
 *
 * This function might emit the objectInformationChanged() signal if something went wrong,
 * setting the error message, or if parsing finished successfully.
 */
void StatisticsFileCSV::readFrameAndTypePositionsFromFile(std::atomic_bool &breakFunction)
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
    int      lastType           = INT_INVALID;
    bool     sortingFixed       = false;

    this->parsingProgress = 0;

    while (!fileAtEnd && !breakFunction.load() && !this->abortParsingDestroy)
    {
      // Fill the buffer
      auto bufferSize = inputFile.readBytes(inputBuffer, bufferStartPos, STAT_PARSING_BUFFER_SIZE);
      if (bufferSize < 0)
        return; // Error reading bytes from file
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
            auto rowItemList = parseCSVLine(lineBuffer, ';');

            // ignore empty entries and headers
            if (!rowItemList[0].isEmpty() && rowItemList[0][0] != '%')
            {
              // check for POC/type information
              auto poc    = rowItemList[0].toInt();
              auto typeID = rowItemList[5].toInt();

              if (lastType == -1 && lastPOC == -1)
              {
                // First POC/type line
                this->pocTypeFileposMap[poc][typeID] = lineBufferStartPos;
                emit readPOCType(poc, typeID);

                lastType = typeID;
                lastPOC  = poc;

                // update number of frames
                if (poc > this->maxPOC)
                  this->maxPOC = poc;
              }
              else if (typeID != lastType && poc == lastPOC)
              {
                // we found a new type but the POC stayed the same.
                // This seems to be an interleaved file
                // Check if we already collected a start position for this type
                if (!sortingFixed)
                {
                  // we only check the first occurence of this, in a non-interleaved file
                  // the above condition can be met and will reset fileSortedByPOC

                  this->fileSortedByPOC = true;
                  sortingFixed          = true;
                }
                lastType = typeID;
                if (this->pocTypeFileposMap[poc].count(typeID) == 0)
                {
                  this->pocTypeFileposMap[poc][typeID] = lineBufferStartPos;
                  emit readPOCType(poc, typeID);
                }
              }
              else if (poc != lastPOC)
              {
                // this is apparently not sorted by POCs and we will not check it further
                if (!sortingFixed)
                  sortingFixed = true;

                // We found a new POC
                if (this->fileSortedByPOC)
                {
                  // There must not be a start position for any type with this POC already.
                  if (this->pocTypeFileposMap.count(poc) > 0)
                    throw "The data for each POC must be continuous in an interleaved statistics "
                          "file";
                }
                else
                {
                  // There must not be a start position for this POC/type already.
                  if (this->pocTypeFileposMap.count(poc) > 0 &&
                      this->pocTypeFileposMap[poc].count(typeID) > 0)
                    throw "The data for each typeID must be continuous in an non interleaved "
                          "statistics file";
                }

                lastPOC  = poc;
                lastType = typeID;

                this->pocTypeFileposMap[poc][typeID] = lineBufferStartPos;
                emit readPOCType(poc, typeID);

                // update number of frames
                if (poc > this->maxPOC)
                  this->maxPOC = poc;

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

    this->parsingProgress = 100.0;
  }
  catch (const char *str)
  {
    std::cerr << "Error while parsing meta data: " << str << "\n";
    this->errorMessage = QString("Error while parsing meta data: ") + QString(str);
    this->error        = true;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "Error while parsing:" << ex.what() << "\n";
    this->errorMessage = QString("Error while parsing: ") + QString(ex.what());
    this->error        = true;
  }
}

void StatisticsFileCSV::loadStatisticData(StatisticsData &statisticsData, int poc, int typeID)
{
  if (!this->file.isOpened())
    return;

  try
  {
    statisticsData.setFrameIndex(poc);

    if (this->pocTypeFileposMap.count(poc) == 0 || this->pocTypeFileposMap[poc].count(typeID) == 0)
    {
      // There are no statistics in the file for the given frame and index.
      statisticsData[typeID] = {};
      return;
    }

    auto startPos = this->pocTypeFileposMap[poc][typeID];
    if (this->fileSortedByPOC)
    {
      // If the statistics file is sorted by POC we have to start at the first entry of this POC and
      // parse the file until another POC is encountered. If this is not done, some information from
      // a different typeID could be ignored during parsing.

      // Get the position of the first line with the given frameIdx
      startPos = std::numeric_limits<qint64>::max();
      for (const auto &typeEntry : this->pocTypeFileposMap[poc])
        if (typeEntry.second < startPos)
          startPos = typeEntry.second;
    }

    QTextStream in(this->file.getQFile());
    in.seek(startPos);

    while (!in.atEnd())
    {
      // read one line
      auto aLine       = in.readLine();
      auto rowItemList = parseCSVLine(aLine, ';');

      if (rowItemList[0].isEmpty())
        continue;

      auto pocRow = rowItemList[0].toInt();
      auto type   = rowItemList[5].toInt();

      // if there is a new POC, we are done here!
      if (pocRow != poc)
        break;
      // if there is a new type and this is a non interleaved file, we are done here.
      if (!this->fileSortedByPOC && type != typeID)
        break;

      int values[4] = {0};

      values[0] = rowItemList[6].toInt();

      bool vectorData = false;
      bool lineData   = false; // or a vector specified by 2 points

      if (rowItemList.count() > 7)
      {
        values[1]  = rowItemList[7].toInt();
        vectorData = true;
      }
      if (rowItemList.count() > 8)
      {
        values[2]  = rowItemList[8].toInt();
        values[3]  = rowItemList[9].toInt();
        lineData   = true;
        vectorData = false;
      }

      auto posX   = rowItemList[1].toInt();
      auto posY   = rowItemList[2].toInt();
      auto width  = rowItemList[3].toUInt();
      auto height = rowItemList[4].toUInt();

      // Check if block is within the image range
      if (this->blockOutsideOfFramePOC == -1 &&
          (posX + int(width) > int(statisticsData.getFrameSize().width) ||
           posY + int(height) > int(statisticsData.getFrameSize().height)))
        // Block not in image. Warn about this.
        this->blockOutsideOfFramePOC = poc;

      auto &statTypes = statisticsData.getStatisticsTypes();
      auto  statIt    = std::find_if(statTypes.begin(), statTypes.end(), [type](StatisticsType &t) {
        return t.typeID == type;
      });
      Q_ASSERT_X(statIt != statTypes.end(), Q_FUNC_INFO, "Stat type not found.");

      if (vectorData && statIt->hasVectorData)
        statisticsData[type].addBlockVector(posX, posY, width, height, values[0], values[1]);
      else if (lineData && statIt->hasVectorData)
        statisticsData[type].addLine(
            posX, posY, width, height, values[0], values[1], values[2], values[3]);
      else
        statisticsData[type].addBlockValue(posX, posY, width, height, values[0]);
    }
  }
  catch (const char *str)
  {
    std::cerr << "Error while parsing: " << str << '\n';
    this->errorMessage = QString("Error while parsing meta data: ") + QString(str);
    this->error        = true;
  }
  catch (...)
  {
    std::cerr << "Error while parsing.";
    this->errorMessage = QString("Error while parsing meta data.");
    this->error        = true;
  }
}

void StatisticsFileCSV::readHeaderFromFile(StatisticsData &statisticsData)
{
  // TODO: Why is there a try block here? I see no throwing of anything ...
  //       We should get rid of this and just set an error and return on failure.
  try
  {
    if (!this->file.isOk())
      return;

    statisticsData.clear();

    // scan header lines first
    // also count the lines per Frame for more efficient memory allocation
    // if an ID is used twice, the data of the first gets overwritten
    bool           typeParsingActive = false;
    StatisticsType aType;

    while (!this->file.atEnd())
    {
      // read one line
      auto    aLineByteArray = this->file.readLine();
      QString aLine(aLineByteArray);

      // get components of this line
      auto rowItemList = parseCSVLine(aLine, ';');

      if (rowItemList[0].isEmpty())
        continue;

      // either a new type or a line which is not header finishes the last type
      if (((rowItemList[1] == "type") || (rowItemList[0][0] != '%')) && typeParsingActive)
      {
        // Last type is complete. Store this initial state.
        aType.setInitialState();
        statisticsData.addStatType(aType);

        // start from scratch for next item
        aType             = StatisticsType();
        typeParsingActive = false;

        // if we found a non-header line, stop here
        if (rowItemList[0][0] != '%')
          return;
      }

      if (rowItemList[1] == "type") // new type
      {
        aType.typeID   = rowItemList[2].toInt();
        aType.typeName = rowItemList[3];

        // The next entry (4) is "map", "range", or "vector"
        if (rowItemList.count() >= 5)
        {
          if (rowItemList[4] == "map" || rowItemList[4] == "range")
          {
            aType.hasValueData    = true;
            aType.renderValueData = true;
          }
          else if (rowItemList[4] == "vector" || rowItemList[4] == "line")
          {
            aType.hasVectorData    = true;
            aType.renderVectorData = true;
            if (rowItemList[4] == "line")
              aType.arrowHead = StatisticsType::ArrowHead::none;
          }
        }

        typeParsingActive = true;
      }
      else if (rowItemList[1] == "mapColor")
      {
        int id = rowItemList[2].toInt();

        // assign color
        auto r = (unsigned char)rowItemList[3].toInt();
        auto g = (unsigned char)rowItemList[4].toInt();
        auto b = (unsigned char)rowItemList[5].toInt();
        auto a = (unsigned char)rowItemList[6].toInt();

        aType.colorMapper.mappingType  = color::MappingType::Map;
        aType.colorMapper.colorMap[id] = Color(r, g, b, a);
      }
      else if (rowItemList[1] == "range")
      {
        // This is a range with min/max
        auto min      = rowItemList[2].toInt();
        auto r        = (unsigned char)rowItemList[4].toInt();
        auto g        = (unsigned char)rowItemList[6].toInt();
        auto b        = (unsigned char)rowItemList[8].toInt();
        auto a        = (unsigned char)rowItemList[10].toInt();
        auto minColor = Color(r, g, b, a);

        auto max      = rowItemList[3].toInt();
        r             = rowItemList[5].toInt();
        g             = rowItemList[7].toInt();
        b             = rowItemList[9].toInt();
        a             = rowItemList[11].toInt();
        auto maxColor = Color(r, g, b, a);

        aType.colorMapper = color::ColorMapper({min, max}, minColor, maxColor);
      }
      else if (rowItemList[1] == "defaultRange")
      {
        // This is a color gradient function
        int  min       = rowItemList[2].toInt();
        int  max       = rowItemList[3].toInt();
        auto rangeName = rowItemList[4].toStdString();

        aType.colorMapper = color::ColorMapper({min, max}, rangeName);
      }
      else if (rowItemList[1] == "vectorColor")
      {
        auto r                  = (unsigned char)rowItemList[2].toInt();
        auto g                  = (unsigned char)rowItemList[3].toInt();
        auto b                  = (unsigned char)rowItemList[4].toInt();
        auto a                  = (unsigned char)rowItemList[5].toInt();
        aType.vectorStyle.color = Color(r, g, b, a);
      }
      else if (rowItemList[1] == "gridColor")
      {
        auto r                = (unsigned char)rowItemList[2].toInt();
        auto g                = (unsigned char)rowItemList[3].toInt();
        auto b                = (unsigned char)rowItemList[4].toInt();
        auto a                = 255;
        aType.gridStyle.color = Color(r, g, b, a);
      }
      else if (rowItemList[1] == "scaleFactor")
      {
        aType.vectorScale = rowItemList[2].toInt();
      }
      else if (rowItemList[1] == "scaleToBlockSize")
      {
        aType.scaleValueToBlockSize = (rowItemList[2] == "1");
      }
      else if (rowItemList[1] == "seq-specs")
      {
        auto seqName = rowItemList[2];
        auto layerId = rowItemList[3];
        // For now do nothing with this information.
        // Show the file name for this item instead.
        auto width  = rowItemList[4].toInt();
        auto height = rowItemList[5].toInt();
        if (width > 0 && height > 0)
          statisticsData.setFrameSize(Size(width, height));
        if (rowItemList[6].toDouble() > 0.0)
          this->framerate = rowItemList[6].toDouble();
      }
    }
  }
  catch (const char *str)
  {
    std::cerr << "Error while parsing meta data: " << str << '\n';
    this->errorMessage = QString("Error while parsing meta data: ") + QString(str);
    this->error        = true;
  }
  catch (...)
  {
    std::cerr << "Error while parsing meta data.";
    this->errorMessage = QString("Error while parsing meta data.");
    this->error        = true;
  }
}

} // namespace stats