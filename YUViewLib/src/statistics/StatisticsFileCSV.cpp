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

#include <statistics/StatisticsTypeBuilder.h>

#include <QTextStream>
#include <iostream>

namespace stats
{

using FileSorting = StatisticsFileBase::ParsingInfo::FileSorting;

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

std::optional<int> toInteger(const QString &text)
{
  bool       ok    = true;
  const auto value = text.toInt(&ok);
  if (ok)
    return value;
  return {};
}

std::optional<Color> toColorWithClipping(const QString &textR,
                                         const QString &textG,
                                         const QString &textB,
                                         const QString &textA)
{
  const auto r = toInteger(textR);
  const auto g = toInteger(textG);
  const auto b = toInteger(textB);
  const auto a = toInteger(textA);

  if (r && g && b && a)
    return Color(functions::clip(*r, 0, 255),
                 functions::clip(*g, 0, 255),
                 functions::clip(*b, 0, 255),
                 functions::clip(*a, 0, 255));
  return {};
}

enum class SpecifiedType
{
  map,
  range,
  vector,
  line
};

struct ParsedType
{
  int                          typeID{};
  std::string                  typeName{};
  std::optional<SpecifiedType> specifiedType{};

  std::optional<StatisticsType::ValueDataOptions>  valueDataOptions;
  std::optional<StatisticsType::VectorDataOptions> vectorDataOptions;
  StatisticsType::GridOptions                      gridOptions;
};

void checkAndAddTypeToStatisticsData(StatisticsData                  &statisticsData,
                                     const std::optional<ParsedType> &type)
{
  if (!type)
    return;

  const bool isValid = (type->valueDataOptions || type->vectorDataOptions);
  if (!isValid)
    return;

  statisticsData.addStatType(StatisticsTypeBuilder(type->typeID, type->typeName)
                               .withOptionalValueDataOptions(type->valueDataOptions)
                               .withOptionalVectorDataOptions(type->vectorDataOptions)
                               .withGridOptions(type->gridOptions)
                               .build());
}

std::optional<ParsedType> parseHeaderLine(const QStringList &lineItems)
{
  if (lineItems.count() < 5)
    return {};

  ParsedType newType;

  if (const auto typeID = toInteger(lineItems[2]))
    newType.typeID = *typeID;
  else
    return {};

  newType.typeName = lineItems[3].toStdString();

  const auto typeEntry = lineItems[4];
  if (typeEntry == "map")
    newType.specifiedType = SpecifiedType::map;
  else if (typeEntry == "range")
    newType.specifiedType = SpecifiedType::range;
  else if (typeEntry == "vector")
    newType.specifiedType = SpecifiedType::vector;
  else if (typeEntry == "line")
    newType.specifiedType = SpecifiedType::line;
  else
    return {};

  // The vector/line type is valid without any additional options. We can just draw a vector.
  // The map and range types must have additional options. They are invalid by default.
  if (newType.specifiedType == SpecifiedType::vector ||
      newType.specifiedType == SpecifiedType::line)
    newType.vectorDataOptions.emplace();

  return newType;
}

std::optional<color::ColorMapper> parseColorMapperFromRange(const QStringList &lineItems)
{
  if (lineItems.count() < 12)
    return {};

  const auto minValue = toInteger(lineItems[2]);
  const auto minColor =
    toColorWithClipping(lineItems[4], lineItems[6], lineItems[8], lineItems[10]);

  const auto maxValue = toInteger(lineItems[3]);
  const auto maxColor =
    toColorWithClipping(lineItems[5], lineItems[7], lineItems[9], lineItems[11]);

  if (!minValue || !minColor || !maxValue || !maxColor)
    return {};

  return color::ColorMapper({*minValue, *maxValue}, *minColor, *maxColor);
}

} // namespace

StatisticsFileCSV::StatisticsFileCSV(const std::string &filename, StatisticsData &statisticsData)
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

    this->parsingInfo.parsingProgress = 0.0;

    while (!fileAtEnd && !breakFunction.load())
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

                if (poc > this->parsingInfo.maxPocEncountered)
                  this->parsingInfo.maxPocEncountered = poc;
              }
              else if (typeID != lastType && poc == lastPOC)
              {
                // we found a new type but the POC stayed the same.
                // This seems to be an interleaved file
                // Check if we already collected a start position for this type
                if (this->parsingInfo.fileSorting == FileSorting::Unknown)
                  this->parsingInfo.fileSorting = FileSorting::SortedByPOC;
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
                if (this->parsingInfo.fileSorting == FileSorting::Unknown)
                  this->parsingInfo.fileSorting = FileSorting::SortedByType;

                // We found a new POC
                if (this->parsingInfo.fileSorting == FileSorting::SortedByPOC)
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

                if (poc > this->parsingInfo.maxPocEncountered)
                  this->parsingInfo.maxPocEncountered = poc;

                // Update percent of file parsed
                if (const auto fileSize = inputFile.getFileSize())
                  this->parsingInfo.parsingProgress = (static_cast<double>(lineBufferStartPos) *
                                                       100 / static_cast<double>(*fileSize));
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

    this->parsingInfo.parsingProgress = 100.0;
  }
  catch (const char *str)
  {
    this->parsingInfo.errorMessage = "Error while parsing meta data: " + std::string(str);
  }
  catch (const std::exception &ex)
  {
    this->parsingInfo.errorMessage = "Error while parsing meta data: " + std::string(ex.what());
  }
}

void StatisticsFileCSV::loadStatisticData(StatisticsData &statisticsData, int poc, int typeID)
{
  if (!this->file.isOk())
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
    if (this->parsingInfo.fileSorting == FileSorting::SortedByPOC)
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
      if (this->parsingInfo.fileSorting == FileSorting::SortedByType && type != typeID)
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
      if (!this->parsingInfo.pocWithDataOutsideOfFrame &&
          (posX + int(width) > int(statisticsData.getFrameSize().width) ||
           posY + int(height) > int(statisticsData.getFrameSize().height)))
        this->parsingInfo.pocWithDataOutsideOfFrame = poc;

      auto &statTypes = statisticsData.getStatisticsTypes();
      auto  statIt    = std::find_if(statTypes.begin(),
                                 statTypes.end(),
                                 [type](StatisticsType &t) { return t.getTypeID() == type; });
      Q_ASSERT_X(statIt != statTypes.end(), Q_FUNC_INFO, "Stat type not found.");

      if (vectorData && statIt->vectorDataOptions)
        statisticsData[type].addBlockVector(posX, posY, width, height, values[0], values[1]);
      else if (lineData && statIt->vectorDataOptions)
        statisticsData[type].addLine(
          posX, posY, width, height, values[0], values[1], values[2], values[3]);
      else
        statisticsData[type].addBlockValue(posX, posY, width, height, values[0]);
    }
  }
  catch (const char *str)
  {
    this->parsingInfo.errorMessage = "Error while parsing meta data: " + std::string(str);
  }
  catch (...)
  {
    this->parsingInfo.errorMessage = "Error while parsing meta data.";
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
    std::optional<ParsedType> currentType;

    while (!this->file.atEnd())
    {
      auto    aLineByteArray = this->file.readLine();
      QString aLine(aLineByteArray);

      const auto rowItemList = parseCSVLine(aLine, ';');

      if (rowItemList[0].isEmpty())
        continue;

      const bool isNonHeaderLine = (rowItemList[0][0] != '%');
      if (isNonHeaderLine)
      {
        checkAndAddTypeToStatisticsData(statisticsData, currentType);
        return;
      }

      if (rowItemList[1] == "type")
      {
        checkAndAddTypeToStatisticsData(statisticsData, currentType);
        currentType.reset();

        currentType = parseHeaderLine(rowItemList);
      }
      else if (rowItemList[1] == "mapColor")
      {
        if (currentType->specifiedType == SpecifiedType::map)
        {
          const auto id = toInteger(rowItemList[2]);
          const auto color =
            toColorWithClipping(rowItemList[3], rowItemList[4], rowItemList[5], rowItemList[6]);

          if (id && color)
          {
            if (!currentType->valueDataOptions)
              currentType->valueDataOptions.emplace();
            currentType->valueDataOptions->colorMapper->mappingType   = color::MappingType::Map;
            currentType->valueDataOptions->colorMapper->colorMap[*id] = *color;
          }
        }
      }
      else if (rowItemList[1] == "range")
      {
        if (currentType->specifiedType == SpecifiedType::range)
          if (const auto colorMapper = parseColorMapperFromRange(rowItemList))
          {
            if (!currentType->valueDataOptions)
              currentType->valueDataOptions.emplace();
            currentType->valueDataOptions->colorMapper = *colorMapper;
          }
      }
      else if (rowItemList[1] == "defaultRange")
      {
        if (currentType->specifiedType == SpecifiedType::range)
        {
          int  min       = rowItemList[2].toInt();
          int  max       = rowItemList[3].toInt();
          auto rangeName = rowItemList[4].toStdString();

          if (!currentType->valueDataOptions)
            currentType->valueDataOptions.emplace();
          currentType->valueDataOptions = StatisticsType::ValueDataOptions(
            {.colorMapper = color::ColorMapper({min, max}, rangeName)});
        }
      }
      else if (rowItemList[1] == "vectorColor")
      {
        auto r                                       = (unsigned char)rowItemList[2].toInt();
        auto g                                       = (unsigned char)rowItemList[3].toInt();
        auto b                                       = (unsigned char)rowItemList[4].toInt();
        auto a                                       = (unsigned char)rowItemList[5].toInt();
        currentType->vectorDataOptions->style->color = Color(r, g, b, a);
      }
      else if (rowItemList[1] == "gridColor")
      {
        auto r                                = (unsigned char)rowItemList[2].toInt();
        auto g                                = (unsigned char)rowItemList[3].toInt();
        auto b                                = (unsigned char)rowItemList[4].toInt();
        auto a                                = 255;
        currentType->gridOptions.style->color = Color(r, g, b, a);
      }
      else if (rowItemList[1] == "scaleFactor")
      {
        if (currentType->vectorDataOptions)
          currentType->vectorDataOptions->scale = rowItemList[2].toInt();
      }
      else if (rowItemList[1] == "scaleToBlockSize")
      {
        if (currentType->valueDataOptions)
          currentType->valueDataOptions->scaleToBlockSize = (rowItemList[2] == "1");
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

    checkAndAddTypeToStatisticsData(statisticsData, currentType);
  }
  catch (const char *str)
  {
    this->parsingInfo.errorMessage = "Error while parsing header: " + std::string(str);
  }
  catch (...)
  {
    this->parsingInfo.errorMessage = "Error while parsing header.";
  }
}

} // namespace stats
