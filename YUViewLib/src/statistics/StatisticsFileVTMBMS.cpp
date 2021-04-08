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
    if (!inputFile.openFile(this->file.absoluteFilePath()))
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
                this->parsingProgress =
                    ((double)lineBufferStartPos * 100 / (double)inputFile.getFileSize());
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
  if (!this->file.isOk())
    return;

  try
  {
    statisticsData.setFrameIndex(poc);
    
    if (this->pocStartList.count(poc) == 0)
    {
      // There are no statistics in the file for the given frame and index.
      statisticsData[typeID] = {};
      return;
    }

    auto startPos = this->pocStartList[poc];

    QTextStream in(this->file.getQFile());
    in.seek(startPos);

    QRegularExpression pocRegex("BlockStat: POC ([0-9]+)");

    // prepare regex for selected type
    auto &statTypes = statisticsData.getStatisticsTypes();
    auto statIt = std::find_if(statTypes.begin(), statTypes.end(), [typeID](StatisticsType &t){ return t.typeID == typeID; });
    Q_ASSERT_X(statIt != statTypes.end(), Q_FUNC_INFO, "Stat type not found.");
    QRegularExpression typeRegex(" " + statIt->typeName + "="); // for catching lines of the type

    // for extracting scalar value statistics, need to match:
    // BlockStat: POC 1 @( 112,  88) [ 8x 8] PredMode=0
    QRegularExpression scalarRegex(
        "POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+=([0-9\\-]+)");
    // for extracting vector value statistics, need to match:
    // BlockStat: POC 1 @( 120,  80) [ 8x 8] MVL0={ -24,  -2}
    QRegularExpression vectorRegex("POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x "
                                   "*([0-9]+)\\] *\\w+={ *([0-9\\-]+), *([0-9\\-]+)}");
    // for extracting affine transform value statistics, need to match:
    // BlockStat: POC 2 @( 192,  96) [64x32] AffineMVL0={-324,-116,-276,-116,-324, -92}
    QRegularExpression affineTFRegex(
        "POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+={ "
        "*([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+)}");
    // for extracting scalar polygon  statistics, need to match:
    // BlockStat: POC 2 @[(505, 384)--(511, 384)--(511, 415)--] GeoPUInterIntraFlag=0
    // BlockStat: POC 2 @[(416, 448)--(447, 448)--(447, 478)--(416, 463)--] GeoPUInterIntraFlag=0
    // will capture 3-5 points. other polygons are not supported
    QRegularExpression scalarPolygonRegex(
        "POC ([0-9]+) @\\[((?:\\( *[0-9]+, *[0-9]+\\)--){3,5})\\] *\\w+=([0-9\\-]+)");
    // for extracting vector polygon statistics:
    QRegularExpression vectorPolygonRegex(
        "POC ([0-9]+) @\\[((?:\\( *[0-9]+, *[0-9]+\\)--){3,5})\\] *\\w+={ *([0-9\\-]+), "
        "*([0-9\\-]+)}");
    // for extracting the partitioning line, we extract
    // BlockStat: POC 2 @( 192,  96) [64x32] Line={0,0,31,31}
    QRegularExpression lineRegex(
        "POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+={ "
        "*([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+)}");

    while (!in.atEnd())
    {
      // read one line
      auto aLine    = in.readLine();
      auto pocMatch = pocRegex.match(aLine);
      // ignore not matching lines
      if (pocMatch.hasMatch())
      {
        auto pocRow = pocMatch.captured(1).toInt();
        if (poc != pocRow)
          break;

        // filter lines of different types
        auto typeMatch = typeRegex.match(aLine);
        if (typeMatch.hasMatch())
        {
          int posX, posY, width, height, scalar, vecX, vecY;

          QRegularExpressionMatch statisitcMatch;
          // extract statistics info
          // try block types
          if (statIt->isPolygon == false)
          {
            if (statIt->hasValueData)
              statisitcMatch = scalarRegex.match(aLine);
            else if (statIt->hasVectorData)
            {
              statisitcMatch = vectorRegex.match(aLine);
              if (!statisitcMatch.hasMatch())
                statisitcMatch = lineRegex.match(aLine);
            }
            else if (statIt->hasAffineTFData)
              statisitcMatch = affineTFRegex.match(aLine);
          }
          else
          // try polygons
          {
            if (statIt->hasValueData)
              statisitcMatch = scalarPolygonRegex.match(aLine);
            else if (statIt->hasVectorData)
              statisitcMatch = vectorPolygonRegex.match(aLine);
          }
          if (!statisitcMatch.hasMatch())
          {
            this->errorMessage = QString("Error while parsing statistic: ") + QString(aLine);
            continue;
          }

          // useful for debugging:
          //        QStringList all_captured = statisitcMatch.capturedTexts();

          pocRow = statisitcMatch.captured(1).toInt();
          width  = statisitcMatch.captured(4).toInt();
          height = statisitcMatch.captured(5).toInt();
          // if there is a new POC, we are done here!
          if (poc != pocRow)
            break;

          // process block statistics
          if (statIt->isPolygon == false)
          {
            posX = statisitcMatch.captured(2).toInt();
            posY = statisitcMatch.captured(3).toInt();

            // Check if block is within the image range
            if (blockOutsideOfFramePOC == -1 && (posX + width > statisticsData.getFrameSize().width ||
                                                 posY + height > statisticsData.getFrameSize().height))
              // Block not in image. Warn about this.
              blockOutsideOfFramePOC = poc;

            if (statIt->hasVectorData)
            {
              vecX = statisitcMatch.captured(6).toInt();
              vecY = statisitcMatch.captured(7).toInt();
              if (statisitcMatch.lastCapturedIndex() > 7)
              {
                auto vecX1 = statisitcMatch.captured(8).toInt();
                auto vecY1 = statisitcMatch.captured(9).toInt();
                statisticsData[typeID].addLine(
                    posX, posY, width, height, vecX, vecY, vecX1, vecY1);
              }
              else
              {
                statisticsData[typeID].addBlockVector(posX, posY, width, height, vecX, vecY);
              }
            }
            else if (statIt->hasAffineTFData)
            {
              auto vecX0 = statisitcMatch.captured(6).toInt();
              auto vecY0 = statisitcMatch.captured(7).toInt();
              auto vecX1 = statisitcMatch.captured(8).toInt();
              auto vecY1 = statisitcMatch.captured(9).toInt();
              auto vecX2 = statisitcMatch.captured(10).toInt();
              auto vecY2 = statisitcMatch.captured(11).toInt();
              statisticsData[typeID].addBlockAffineTF(
                  posX, posY, width, height, vecX0, vecY0, vecX1, vecY1, vecX2, vecY2);
            }
            else
            {
              scalar = statisitcMatch.captured(6).toInt();
              statisticsData[typeID].addBlockValue(posX, posY, width, height, scalar);
            }
          }
          else
          // process polygon statistics
          {
            auto               corners    = statisitcMatch.captured(2);
            auto               cornerList = corners.split("--");
            QRegularExpression cornerRegex("\\( *([0-9]+), *([0-9]+)\\)");
            stats::Polygon     points;
            for (const auto &corner : cornerList)
            {
              auto cornerMatch = cornerRegex.match(corner);
              if (cornerMatch.hasMatch())
              {
                auto x = cornerMatch.captured(1).toInt();
                auto y = cornerMatch.captured(2).toInt();
                points.push_back({x, y});

                // Check if polygon is within the image range
                if (this->blockOutsideOfFramePOC == -1 &&
                    (x + width > statisticsData.getFrameSize().width ||
                     y + height > statisticsData.getFrameSize().height))
                  // Block not in image. Warn about this.
                  this->blockOutsideOfFramePOC = poc;
              }
            }

            if (statIt->hasVectorData)
            {
              vecX = statisitcMatch.captured(3).toInt();
              vecY = statisitcMatch.captured(4).toInt();
              statisticsData[typeID].addPolygonVector(points, vecX, vecY);
            }
            else if (statIt->hasValueData)
            {
              scalar = statisitcMatch.captured(3).toInt();
              statisticsData[typeID].addPolygonValue(points, scalar);
            }
          }
        }
      }
    }

    if (!statisticsData.hasDataForTypeID(typeID))
    {
      // There are no statistics in the file for the given frame and index.
      statisticsData[typeID] = {};
      return;
    }

  } // try
  catch (const char *str)
  {
    std::cerr << "Error while parsing: " << str << '\n';
    this->errorMessage = QString("Error while parsing meta data: ") + QString(str);
    return;
  }
  catch (...)
  {
    std::cerr << "Error while parsing.";
    this->errorMessage = QString("Error while parsing meta data.");
    return;
  }

  return;
}

void StatisticsFileVTMBMS::readHeaderFromFile(StatisticsData &statisticsData)
{
  try
  {
    if (!this->file.isOk())
      return;

    statisticsData.clear();

    while (!this->file.atEnd())
    {
      // read one line
      auto    aLineByteArray = this->file.readLine();
      QString aLine(aLineByteArray);

      // if we found a non-header line, stop here
      if (aLine[0] != '#')
        return;

      // extract statistics information from header lines
      // match:
      //# Sequence size: [832x 480]
      QRegularExpression sequenceSizeRegex("# Sequence size: \\[([0-9]+)x *([0-9]+)\\]");

      // match:
      //# Block Statistic Type: MergeFlag; Flag
      QRegularExpression availableStatisticsRegex(
          "# Block Statistic Type: *([0-9a-zA-Z_]+); *([0-9a-zA-Z]+); *(.*)");

      // get sequence size
      auto sequenceSizeMatch = sequenceSizeRegex.match(aLine);
      if (sequenceSizeMatch.hasMatch())
      {
        statisticsData.setFrameSize(
            Size(sequenceSizeMatch.captured(1).toInt(), sequenceSizeMatch.captured(2).toInt()));
      }

      // get available statistics
      auto availableStatisticsMatch = availableStatisticsRegex.match(aLine);
      if (availableStatisticsMatch.hasMatch())
      {
        StatisticsType aType;
        // Store initial state.
        aType.setInitialState();

        // set name
        aType.typeName = availableStatisticsMatch.captured(1);

        // with -1, an id will be automatically assigned
        aType.typeID = -1;

        // check if scalar or vector
        auto statType = availableStatisticsMatch.captured(2);
        if (statType.contains(
                "AffineTFVectors")) // "Vector" is contained in this, need to check it first
        {
          auto               scaleInfo = availableStatisticsMatch.captured(3);
          QRegularExpression scaleInfoRegex("Scale: *([0-9]+)");
          auto               scaleInfoMatch = scaleInfoRegex.match(scaleInfo);
          int                scale;
          if (scaleInfoMatch.hasMatch())
            scale = scaleInfoMatch.captured(1).toInt();
          else
            scale = 1;

          aType.hasAffineTFData  = true;
          aType.renderVectorData = true;
          aType.vectorScale      = scale;
          aType.vectorStyle.color = Color(255, 0, 0);
        }
        else if (statType.contains("Vector"))
        {
          auto               scaleInfo = availableStatisticsMatch.captured(3);
          QRegularExpression scaleInfoRegex("Scale: *([0-9]+)");
          auto               scaleInfoMatch = scaleInfoRegex.match(scaleInfo);
          int                scale;
          if (scaleInfoMatch.hasMatch())
            scale = scaleInfoMatch.captured(1).toInt();
          else
            scale = 1;

          aType.hasVectorData    = true;
          aType.renderVectorData = true;
          aType.vectorScale      = scale;
          aType.vectorStyle.color = Color(255, 0, 0);
        }
        else if (statType.contains("Flag"))
        {
          aType.hasValueData    = true;
          aType.renderValueData = true;
          aType.colorMapper     = ColorMapper("jet", 0, 1);
        }
        else if (statType.contains("Integer")) // for now do the same as for Flags
        {
          auto               rangeInfo = availableStatisticsMatch.captured(3);
          QRegularExpression rangeInfoRegex("\\[([0-9\\-]+), *([0-9\\-]+)\\]");
          auto               rangeInfoMatch = rangeInfoRegex.match(rangeInfo);
          int                minVal         = 0;
          int                maxVal         = 100;
          if (rangeInfoMatch.hasMatch())
          {
            minVal = rangeInfoMatch.captured(1).toInt();
            maxVal = rangeInfoMatch.captured(2).toInt();
          }

          aType.hasValueData    = true;
          aType.renderValueData = true;
          aType.colorMapper     = ColorMapper("jet", minVal, maxVal);
        }
        else if (statType.contains("Line"))
        {
          aType.hasVectorData    = true;
          aType.renderVectorData = true;
          aType.vectorScale      = 1;
          aType.arrowHead        = StatisticsType::ArrowHead::none;
          aType.gridStyle.color = Color(255, 255, 255);
          aType.vectorStyle.color = Color(255, 255, 255);
        }

        // check whether is was a geometric partitioning statistic with polygon shape
        if (statType.contains("Polygon"))
          aType.isPolygon = true;

        // add the new type if it is not already in the list
        statisticsData.addStatType(aType); // check if in list is done by addStatsType
      }
    }
  } // try
  catch (const char *str)
  {
    std::cerr << "Error while parsing meta data: " << str << '\n';
    this->errorMessage = QString("Error while parsing meta data: ") + QString(str);
  }
  catch (...)
  {
    std::cerr << "Error while parsing meta data.";
    this->errorMessage = QString("Error while parsing meta data.");
  }
}

} // namespace stats
