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

#include "playlistItemStatisticsVTMBMSFile.h"

#include <cassert>
#include <iostream>
#include <QDebug>
#include <QtConcurrent>
#include <QTime>
#include "statisticsExtensions.h"

// The internal buffer for parsing the starting positions. The buffer must not be larger than 2GB
// so that we can address all the positions in it with int (using such a large buffer is not a good
// idea anyways)
#define STAT_PARSING_BUFFER_SIZE 1048576
#define STAT_MAX_STRING_SIZE 1<<28

playlistItemStatisticsVTMBMSFile::playlistItemStatisticsVTMBMSFile(const QString &itemNameOrFileName)
  : playlistItemStatisticsFile(itemNameOrFileName)
{
  file.openFile(itemNameOrFileName);
  if (!file.isOk())
    return;

  // Read the statistics file header
  readHeaderFromFile();

  // Run the parsing of the file in the background
  cancelBackgroundParser = false;
  timer.start(1000, this);
  backgroundParserFuture = QtConcurrent::run(this, &playlistItemStatisticsVTMBMSFile::readFramePositionsFromFile);

  connect(&statSource, &statisticHandler::updateItem, [this](bool redraw){ emit signalItemChanged(redraw, RECACHE_NONE); });
  connect(&statSource, &statisticHandler::requestStatisticsLoading, this, &playlistItemStatisticsVTMBMSFile::loadStatisticToCache, Qt::DirectConnection);
}

/** The background task that parses the file and extracts the exact file positions
* where a new frame starts. If the user then later requests this POC
* we can directly jump there and parse the actual information. This way we don't have to
* scan the whole file which can get very slow for large files.
*
* This function might emit the objectInformationChanged() signal if something went wrong,
* setting the error message, or if parsing finished successfully.
*/
void playlistItemStatisticsVTMBMSFile::readFramePositionsFromFile()
{
  try
  {
    // Open the file (again). Since this is a background process, we open the file again to
    // not disturb any reading from not background code.
    fileSource inputFile;
    if (!inputFile.openFile(file.absoluteFilePath()))
      return;

    // We perform reading using an input buffer
    QByteArray inputBuffer;
    bool fileAtEnd = false;
    qint64 bufferStartPos = 0;

    QString lineBuffer;
    qint64  lineBufferStartPos = 0;
    int     lastPOC = INT_INVALID;
    bool    sortingFixed = false; 
    
    while (!fileAtEnd && !cancelBackgroundParser)
    {
      // Fill the buffer
      int bufferSize = inputFile.readBytes(inputBuffer, bufferStartPos, STAT_PARSING_BUFFER_SIZE);
      if (bufferSize < STAT_PARSING_BUFFER_SIZE)
        // Less bytes than the maximum buffer size were read. The file is at the end.
        // This is the last run of the loop.
        fileAtEnd = true;
      // a corrupted file may contain an arbitrary amount of non-\n symbols
      // prevent lineBuffer overflow by dumping it for such cases
      if (lineBuffer.size() > STAT_MAX_STRING_SIZE)
        lineBuffer.clear(); // prevent an overflow here
      for (int i = 0; i < bufferSize; i++)
      {
        // Search for '\n' newline characters
        if (inputBuffer.at(i) == 10)
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

            QRegularExpressionMatch match = pocRegex.match(lineBuffer);
            // ignore not matching lines
            if (match.hasMatch())
            {
              int poc = match.captured(1).toInt();

              if (lastPOC == -1)
              {
                // First POC
                pocStartList[poc] = lineBufferStartPos;
                if (poc == currentDrawnFrameIdx)
                  // We added a start position for the frame index that is currently drawn. We might have to redraw.
                  emit signalItemChanged(true, RECACHE_NONE);

                lastPOC = poc;

                // update number of frames
                if (poc > maxPOC)
                  maxPOC = poc;
              }
              else if (poc != lastPOC)
              {
                // this is apparently not sorted by POCs and we will not check it further
                if(!sortingFixed)
                  sortingFixed = true;

                lastPOC = poc;                
                pocStartList[poc] = lineBufferStartPos;
                if (poc == currentDrawnFrameIdx)
                  // We added a start position for the frame index that is currently drawn. We might have to redraw.
                  emit signalItemChanged(true, RECACHE_NONE);

                // update number of frames
                if (poc > maxPOC)
                  maxPOC = poc;

                // Update percent of file parsed
                backgroundParserProgress = ((double)lineBufferStartPos * 100 / (double)inputFile.getFileSize());
              }
            }
          }

          lineBuffer.clear();
          lineBufferStartPos = bufferStartPos + i + 1;
        }
        else
        {
          // No newline character found
          lineBuffer.append(inputBuffer.at(i));
        }
      }

      bufferStartPos += bufferSize;
    }

    // Parsing complete
    backgroundParserProgress = 100.0;

    setStartEndFrame(indexRange(0, maxPOC), false);
    emit signalItemChanged(false, RECACHE_NONE);

  } // try
  catch (const char *str)
  {
    std::cerr << "Error while parsing meta data: " << str << "\n";
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    emit signalItemChanged(false, RECACHE_NONE);
    return;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Error while parsing:" << ex.what() << "\n";
    parsingError = QString("Error while parsing: ") + QString(ex.what());
    emit signalItemChanged(false, RECACHE_NONE);
    return;
  }

  return;
}

void playlistItemStatisticsVTMBMSFile::readHeaderFromFile()
{
  try
  {
    if (!file.isOk())
      return;

    // Cleanup old types
    statSource.clearStatTypes();

    while (!file.atEnd())
    {
      // read one line
      QByteArray aLineByteArray = file.readLine();
      QString aLine(aLineByteArray);

      // if we found a non-header line, stop here
      if (aLine[0] != '#')
      {
        return;
      }

      // extract statistics information from header lines
      // match:
      //# Sequence size: [832x 480]
      QRegularExpression sequenceSizeRegex("# Sequence size: \\[([0-9]+)x *([0-9]+)\\]");

      // match:
      //# Block Statistic Type: MergeFlag; Flag
      QRegularExpression availableStatisticsRegex("# Block Statistic Type: *([0-9a-zA-Z_]+); *([0-9a-zA-Z]+); *(.*)");

      // get sequence size
      QRegularExpressionMatch sequenceSizeMatch = sequenceSizeRegex.match(aLine);
      if (sequenceSizeMatch.hasMatch())
      {
        statSource.setFrameSize(QSize(sequenceSizeMatch.captured(1).toInt(), sequenceSizeMatch.captured(2).toInt()));
      }

      // get available statistics
      QRegularExpressionMatch availableStatisticsMatch = availableStatisticsRegex.match(aLine);
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
        QString statType = availableStatisticsMatch.captured(2);
        if (statType.contains("AffineTFVectors"))  // "Vector" is contained in this, need to check it first
        {
          QString scaleInfo = availableStatisticsMatch.captured(3);
          QRegularExpression scaleInfoRegex("Scale: *([0-9]+)");
          QRegularExpressionMatch scaleInfoMatch = scaleInfoRegex.match(scaleInfo);
          int scale;
          if (scaleInfoMatch.hasMatch())
          {
            scale = scaleInfoMatch.captured(1).toInt();
          }
          else
          {
            scale = 1;
          }

          aType.hasAffineTFData = true;
          aType.renderVectorData = true;
          aType.vectorScale = scale;
          aType.vectorPen.setColor(QColor(255,0, 0));
        }
        else if (statType.contains("Vector"))
        {
          QString scaleInfo = availableStatisticsMatch.captured(3);
          QRegularExpression scaleInfoRegex("Scale: *([0-9]+)");
          QRegularExpressionMatch scaleInfoMatch = scaleInfoRegex.match(scaleInfo);
          int scale;
          if (scaleInfoMatch.hasMatch())
          {
            scale = scaleInfoMatch.captured(1).toInt();
          }
          else
          {
            scale = 1;
          }

          aType.hasVectorData = true;
          aType.renderVectorData = true;
          aType.vectorScale = scale;
          aType.vectorPen.setColor(QColor(255,0, 0));
        }
        else if (statType.contains("Flag"))
        {
          aType.hasValueData = true;
          aType.renderValueData = true;
          aType.colMapper = colorMapper("jet", 0, 1);
        }
        else if (statType == "Integer")  // for now do the same as for Flags
        {
          QString rangeInfo = availableStatisticsMatch.captured(3);
          QRegularExpression rangeInfoRegex("\\[([0-9\\-]+), *([0-9\\-]+)\\]");
          QRegularExpressionMatch rangeInfoMatch = rangeInfoRegex.match(rangeInfo);
          int minVal;
          int maxVal;
          if (rangeInfoMatch.hasMatch())
          {
            minVal = rangeInfoMatch.captured(1).toInt();
            maxVal = rangeInfoMatch.captured(2).toInt();
          }
          else
          {
            minVal = 0;
            maxVal = 100;
          }

          aType.hasValueData = true;
          aType.renderValueData = true;
          aType.colMapper = colorMapper("jet", minVal, maxVal);
        }
        else if (statType.contains("Line"))
        {
          aType.hasVectorData=true;
          aType.renderVectorData = true;
          aType.vectorScale = 1;
          aType.arrowHead=StatisticsType::arrowHead_t::none;
          aType.gridPen.setColor(QColor(255,255,255));
          aType.vectorPen.setColor(QColor(255,255,255));
        }

        // check whether is was a geometric partitioning statistic with polygon shape
        if (statType.contains("Polygon"))
        {
          aType.isPolygon = true;
        }

        // add the new type if it is not already in the list
        statSource.addStatType(aType); // check if in list is done by addStatsType
      }
    }
  } // try
  catch (const char *str)
  {
    std::cerr << "Error while parsing meta data: " << str << '\n';
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    return;
  }
  catch (...)
  {
    std::cerr << "Error while parsing meta data.";
    parsingError = QString("Error while parsing meta data.");
    return;
  }

  return;
}

void playlistItemStatisticsVTMBMSFile::loadStatisticToCache(int frameIdxInternal, int typeID)
{
  try
  {
    if (!file.isOk())
      return;

    QTextStream in(file.getQFile());

    if (!pocStartList.contains(frameIdxInternal))
    {
      // There are no statistics in the file for the given frame and index.
      statSource.statsCache.insert(typeID, statisticsData());
      return;
    }


    qint64 startPos = pocStartList[frameIdxInternal];

    // fast forward
    in.seek(startPos);

    QRegularExpression pocRegex("BlockStat: POC ([0-9]+)");

    // prepare regex for selected type
    StatisticsType *aType = statSource.getStatisticsType(typeID);
    Q_ASSERT_X(aType != nullptr, "StatisticsObject::readStatisticsFromFile", "Stat type not found.");
    QRegularExpression typeRegex(" " + aType->typeName + "="); // for catching lines of the type

    // for extracting scalar value statistics, need to match:
    // BlockStat: POC 1 @( 112,  88) [ 8x 8] PredMode=0
    QRegularExpression scalarRegex("POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+=([0-9\\-]+)");
    // for extracting vector value statistics, need to match:
    // BlockStat: POC 1 @( 120,  80) [ 8x 8] MVL0={ -24,  -2}
    QRegularExpression vectorRegex("POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+={ *([0-9\\-]+), *([0-9\\-]+)}");
    // for extracting affine transform value statistics, need to match:
    // BlockStat: POC 2 @( 192,  96) [64x32] AffineMVL0={-324,-116,-276,-116,-324, -92}
    QRegularExpression affineTFRegex("POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+={ *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+)}");
    // for extracting scalar polygon  statistics, need to match:
    // BlockStat: POC 2 @[(505, 384)--(511, 384)--(511, 415)--] GeoPUInterIntraFlag=0
    // BlockStat: POC 2 @[(416, 448)--(447, 448)--(447, 478)--(416, 463)--] GeoPUInterIntraFlag=0
    // will capture 3-5 points. other polygons are not supported
    QRegularExpression scalarPolygonRegex("POC ([0-9]+) @\\[((?:\\( *[0-9]+, *[0-9]+\\)--){3,5})\\] *\\w+=([0-9\\-]+)");
    // for extracting vector polygon statistics:
    QRegularExpression vectorPolygonRegex("POC ([0-9]+) @\\[((?:\\( *[0-9]+, *[0-9]+\\)--){3,5})\\] *\\w+={ *([0-9\\-]+), *([0-9\\-]+)}");
    // for extracting the partitioning line, we extract
    // BlockStat: POC 2 @( 192,  96) [64x32] Line={0,0,31,31}
    QRegularExpression lineRegex("POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+={ *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+), *([0-9\\-]+)}");

    while (!in.atEnd())
    {
      // read one line
      QString aLine = in.readLine();


      QRegularExpressionMatch pocMatch = pocRegex.match(aLine);
      // ignore not matching lines
      if (pocMatch.hasMatch())
      {
        int poc = pocMatch.captured(1).toInt();
        if (poc != frameIdxInternal)
        {
          break;
        }

        // filter lines of different types
        QRegularExpressionMatch typeMatch = typeRegex.match(aLine);
        if (typeMatch.hasMatch())
        {
          int poc, posX, posY, width, height, scalar, vecX, vecY;

          QRegularExpressionMatch statisitcMatch;
          // extract statistics info
          // try block types
          if (aType->isPolygon == false)
          {
            if (aType->hasValueData)
            {
              statisitcMatch = scalarRegex.match(aLine);
            }
            else if (aType->hasVectorData)
            {
              statisitcMatch = vectorRegex.match(aLine);
              if (!statisitcMatch.hasMatch())
              {
                statisitcMatch = lineRegex.match(aLine);
              }
            }
            else if (aType->hasAffineTFData)
            {
              statisitcMatch = affineTFRegex.match(aLine);
            }
          }
          else
          // try polygons
          {
            if (aType->hasValueData)
            {
              statisitcMatch = scalarPolygonRegex.match(aLine);
            }
            else if (aType->hasVectorData)
            {
              statisitcMatch = vectorPolygonRegex.match(aLine);
            }
          }
          if (!statisitcMatch.hasMatch())
          {
            parsingError = QString("Error while parsing statistic: ") + QString(aLine);
            continue;
          }

          // useful for debugging:
  //        QStringList all_captured = statisitcMatch.capturedTexts();

          poc = statisitcMatch.captured(1).toInt();
          width = statisitcMatch.captured(4).toInt();
          height = statisitcMatch.captured(5).toInt();
          // if there is a new POC, we are done here!
          if (poc != frameIdxInternal)
            break;

          // process block statistics
          if (aType->isPolygon == false)
          {
            posX = statisitcMatch.captured(2).toInt();
            posY = statisitcMatch.captured(3).toInt();

            // Check if block is within the image range
            if (blockOutsideOfFrame_idx == -1 && (posX + width > statSource.getFrameSize().width() || posY + height > statSource.getFrameSize().height()))
              // Block not in image. Warn about this.
              blockOutsideOfFrame_idx = frameIdxInternal;

            if (aType->hasVectorData)
            {
              vecX = statisitcMatch.captured(6).toInt();
              vecY = statisitcMatch.captured(7).toInt();
              if (statisitcMatch.lastCapturedIndex()>7)
              {
                int vecX1 = statisitcMatch.captured(8).toInt();
                int vecY1 = statisitcMatch.captured(9).toInt();
                statSource.statsCache[typeID].addLine(posX, posY, width, height, vecX, vecY,vecX1,vecY1);
              }
              else
              {
                statSource.statsCache[typeID].addBlockVector(posX, posY, width, height, vecX, vecY);
              }
            }
            else if (aType->hasAffineTFData)
            {
              int vecX0 = statisitcMatch.captured(6).toInt();
              int vecY0 = statisitcMatch.captured(7).toInt();
              int vecX1 = statisitcMatch.captured(8).toInt();
              int vecY1 = statisitcMatch.captured(9).toInt();
              int vecX2 = statisitcMatch.captured(10).toInt();
              int vecY2 = statisitcMatch.captured(11).toInt();
              statSource.statsCache[typeID].addBlockAffineTF(posX, posY, width, height, vecX0, vecY0, vecX1, vecY1, vecX2, vecY2);
            }
            else
            {
              scalar = statisitcMatch.captured(6).toInt();
              statSource.statsCache[typeID].addBlockValue(posX, posY, width, height, scalar);
            }
          }
          else
          // process polygon statistics
          {
            QString corners = statisitcMatch.captured(2);
            QStringList cornerList = corners.split("--");
            QRegularExpression cornerRegex("\\( *([0-9]+), *([0-9]+)\\)");
            QVector<QPoint> points;
            for (const auto &corner : cornerList)
            {
              QRegularExpressionMatch cornerMatch = cornerRegex.match(corner);
              if( cornerMatch.hasMatch())
              {
                int x = cornerMatch.captured(1).toInt();
                int y = cornerMatch.captured(2).toInt();
                points << QPoint(x,y);

                // Check if polygon is within the image range
                if (blockOutsideOfFrame_idx == -1 && (x + width > statSource.getFrameSize().width() || y + height > statSource.getFrameSize().height()))
                  // Block not in image. Warn about this.
                  blockOutsideOfFrame_idx = frameIdxInternal;
              }
            }

            if (aType->hasVectorData)
            {
              vecX = statisitcMatch.captured(3).toInt();
              vecY = statisitcMatch.captured(4).toInt();
              statSource.statsCache[typeID].addPolygonVector(points, vecX, vecY);
            }
            else if(aType->hasValueData)
            {
              scalar = statisitcMatch.captured(3).toInt();
              statSource.statsCache[typeID].addPolygonValue(points, scalar);
            }
          }
        }
      }
    }

    if(!statSource.statsCache.contains(typeID))
    {
      // There are no statistics in the file for the given frame and index.
      statSource.statsCache.insert(typeID, statisticsData());
      return;
    }

  } // try
  catch (const char *str)
  {
    std::cerr << "Error while parsing: " << str << '\n';
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    return;
  }
  catch (...)
  {
    std::cerr << "Error while parsing.";
    parsingError = QString("Error while parsing meta data.");
    return;
  }

  return;
}

playlistItemStatisticsVTMBMSFile *playlistItemStatisticsVTMBMSFile::newplaylistItemStatisticsVTMBMSFile(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemStatisticsFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemStatisticsVTMBMSFile *newStat = new playlistItemStatisticsVTMBMSFile(filePath);

  // Load the propertied of the playlistItem
  playlistItem::loadPropertiesFromPlaylist(root, newStat);

  // Load the status of the statistics (which are shown, transparency ...)
  newStat->statSource.loadPlaylist(root);

  return newStat;
}

void playlistItemStatisticsVTMBMSFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  allExtensions.append("vtmbmsstats");
  filters.append("Statistics File (*.vtmbmsstats)");
}

void playlistItemStatisticsVTMBMSFile::reloadItemSource()
{
  // Set default variables
  fileSortedByPOC = false;
  blockOutsideOfFrame_idx = -1;
  backgroundParserProgress = 0.0;
  parsingError.clear();
  currentDrawnFrameIdx = -1;
  maxPOC = 0;

  // Is the background parser still running? If yes, abort it.
  if (backgroundParserFuture.isRunning())
  {
    // signal to background thread that we want to cancel the processing
    cancelBackgroundParser = true;
    backgroundParserFuture.waitForFinished();
  }

  // Clear the parsed data
  pocStartList.clear();
  statSource.statsCache.clear();
  statSource.statsCacheFrameIdx = -1;

  // Reopen the file
  file.openFile(plItemNameOrFileName);
  if (!file.isOk())
    return;

  // Read the new statistics file header
  readHeaderFromFile();

  statSource.updateStatisticsHandlerControls();

  // Run the parsing of the file in the background
  cancelBackgroundParser = false;
  timer.start(1000, this);
  backgroundParserFuture = QtConcurrent::run(this, &playlistItemStatisticsVTMBMSFile::readFramePositionsFromFile);
}

