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

#include "playlistItemVTMBMSStatisticsFile.h"

#include <cassert>
#include <iostream>
#include <functional> //for std::hash
#include <string>
#include <QDebug>
#include <QtConcurrent>
#include <QTime>
#include "statisticsExtensions.h"

// The internal buffer for parsing the starting positions. The buffer must not be larger than 2GB
// so that we can address all the positions in it with int (using such a large buffer is not a good
// idea anyways)
#define STAT_PARSING_BUFFER_SIZE 1048576

playlistItemVTMBMSStatisticsFile::playlistItemVTMBMSStatisticsFile(const QString &itemNameOrFileName)
  : playlistItem(itemNameOrFileName, playlistItem_Indexed)
{
  // Set default variables
  fileSortedByPOC = false;
  blockOutsideOfFrame_idx = -1;
  backgroundParserProgress = 0.0;
  currentDrawnFrameIdx = -1;
  maxPOC = 0;
  isStatisticsLoading = false;

  // todo: change. this is a hack for testing
  statSource.statFrameSize = QSize(832, 480);

  // get poc and type using regular expression
  // need to match this:
  // BlockStat: POC 1 @( 120,  80) [ 8x 8] MVL0={ -24,  -2}
  // BlockStat: POC 1 @( 112,  88) [ 8x 8] PredMode=0
  pocAndTypeRegex.setPattern("POC ([0-9]+).*\] (\\w+)=(.*)");

  // Set statistics icon
  setIcon(0, convertIcon(":img_stats.png"));

  file.openFile(itemNameOrFileName);
  if (!file.isOk())
    return;

  // Read the statistics file header
//  readHeaderFromFile();

  readFramePositionsFromFile();
  // Run the parsing of the file in the background
  cancelBackgroundParser = false;
//  timer.start(1000, this);
//  backgroundParserFuture = QtConcurrent::run(this, &playlistItemVTMBMSStatisticsFile::readFramePositionsFromFile);

  connect(&statSource, &statisticHandler::updateItem, [this](bool redraw){ emit signalItemChanged(redraw, RECACHE_NONE); });
  connect(&statSource, &statisticHandler::requestStatisticsLoading, this, &playlistItemVTMBMSStatisticsFile::loadStatisticToCache, Qt::DirectConnection);
}

playlistItemVTMBMSStatisticsFile::~playlistItemVTMBMSStatisticsFile()
{
  // The playlistItemStatisticsFile object is being deleted.
  // Check if the background thread is still running.
  if (backgroundParserFuture.isRunning())
  {
    // signal to background thread that we want to cancel the processing
    cancelBackgroundParser = true;
    backgroundParserFuture.waitForFinished();
  }
}

infoData playlistItemVTMBMSStatisticsFile::getInfo() const
{
  infoData info("Statistics File info");

  // Append the file information (path, date created, file size...)
  info.items.append(file.getFileInfoList());

  // Is the file sorted by POC?
  info.items.append(infoItem("Sorted by POC", fileSortedByPOC ? "Yes" : "No"));

  // Show the progress of the background parsing (if running)
  if (backgroundParserFuture.isRunning())
    info.items.append(infoItem("Parsing:", QString("%1%...").arg(backgroundParserProgress, 0, 'f', 2)));

  // Print a warning if one of the blocks in the statistics file is outside of the defined "frame size"
  if (blockOutsideOfFrame_idx != -1)
    info.items.append(infoItem("Warning", QString("A block in frame %1 is outside of the given size of the statistics.").arg(blockOutsideOfFrame_idx)));

  // Show any errors that occurred during parsing
  if (!parsingError.isEmpty())
    info.items.append(infoItem("Parsing Error:", parsingError));

  return info;
}

void playlistItemVTMBMSStatisticsFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  // drawRawData only controls the drawing of raw pixel values
  Q_UNUSED(drawRawData);
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  // Tell the statSource to draw the statistics
  statSource.paintStatistics(painter, frameIdxInternal, zoomFactor);

  // Currently this frame is drawn.
  currentDrawnFrameIdx = frameIdxInternal;
}

/** The background task that parses the file and extracts the exact file positions
* where a new frame or a new type starts. If the user then later requests this type/POC
* we can directly jump there and parse the actual information. This way we don't have to
* scan the whole file which can get very slow for large files.
*
* This function might emit the objectInformationChanged() signal if something went wrong,
* setting the error message, or if parsing finished successfully.
*/
void playlistItemVTMBMSStatisticsFile::readFramePositionsFromFile()
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
    int     lastType = INT_INVALID;
    bool    sortingFixed = false; 
    
    while (!fileAtEnd && !cancelBackgroundParser)
    {
      // Fill the buffer
      int bufferSize = inputFile.readBytes(inputBuffer, bufferStartPos, STAT_PARSING_BUFFER_SIZE);
      if (bufferSize < STAT_PARSING_BUFFER_SIZE)
        // Less bytes than the maximum buffer size were read. The file is at the end.
        // This is the last run of the loop.
        fileAtEnd = true;

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
            QRegularExpressionMatch match = pocAndTypeRegex.match(lineBuffer);
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

              StatisticsType aType;
              // Last type is complete. Store this initial state.
              aType.setInitialState();

              // set name
              aType.typeName = match.captured(2);

//              // set id
//              std::string str = "Hello World";
//              std::hash<std::string> hasher;
////              auto hashed = hasher(str); //returns std::size_t
//              int id_from_name = hasher(aType.typeName.toStdString());
//              aType.typeID = id_from_name;
              // with -1, an id will be automatically assigned
              aType.typeID = -1;

              // check if scalar or vector
              QString statVal = match.captured(3);
              if (statVal.contains(','))
              { // is a vector
                aType.hasVectorData = true;
                aType.renderVectorData = true;
              }
              else
              { // is a scalar
                aType.hasValueData = true;
                aType.renderValueData = true;
              }

//              // have fixed colors for now. todo: change
//              aType.colMapper = colorMapper("jet", 0, 5);
//              aType.vectorPen.setColor(QColor(100, 0, 0));
//              aType.gridPen.setColor(QColor(0, 0, 100));



              // add the new type if it is not already in the list
//              if (!statSource.getStatisticsTypeList().contains(aType))
//              {
              statSource.addStatType(aType); // check if in list is done by addStatsType
//              }
            }
          }

          lineBuffer.clear();
          lineBufferStartPos = bufferStartPos + i + 1;
        }
        else
          // No newline character found
          lineBuffer.append(inputBuffer.at(i));
      }

      bufferStartPos += bufferSize;
    }

    // Parsing complete
    backgroundParserProgress = 100.0;

    setStartEndFrame(indexRange(0, maxPOC), false);
//    emit signalItemChanged(false, RECACHE_NONE);
    emit signalItemChanged(true, RECACHE_NONE); // todo. remove hack

  } // try
  catch (const char *str)
  {
    std::cerr << "Error while parsing meta data: " << str << '\n';
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    emit signalItemChanged(false, RECACHE_NONE);
    return;
  }
  catch (...)
  {
    std::cerr << "Error while parsing meta data.";
    parsingError = QString("Error while parsing meta data.");
    emit signalItemChanged(false, RECACHE_NONE);
    return;
  }

  return;
}

//void playlistItemVTMBMSStatisticsFile::readHeaderFromFile()
//{
//  try
//  {
//    if (!file.isOk())
//      return;

//    // Cleanup old types
//    statSource.clearStatTypes();

//    // scan header lines first
//    // also count the lines per Frame for more efficient memory allocation
//    // if an ID is used twice, the data of the first gets overwritten
//    bool typeParsingActive = false;
//    StatisticsType aType;

//    while (!file.atEnd())
//    {
//      // read one line
//      QByteArray aLineByteArray = file.readLine();
//      QString aLine(aLineByteArray);

//      // get components of this line
//      QStringList rowItemList = parseCSVLine(aLine, ';');

//      if (rowItemList[0].isEmpty())
//        continue;

//      // either a new type or a line which is not header finishes the last type
//      if (((rowItemList[1] == "type") || (rowItemList[0][0] != '%')) && typeParsingActive)
//      {
//        // Last type is complete. Store this initial state.
//        aType.setInitialState();
//        statSource.addStatType(aType);

//        // start from scratch for next item
//        aType = StatisticsType();
//        typeParsingActive = false;

//        // if we found a non-header line, stop here
//        if (rowItemList[0][0] != '%')
//          return;
//      }

//      if (rowItemList[1] == "type")   // new type
//      {
//        aType.typeID = rowItemList[2].toInt();
//        aType.typeName = rowItemList[3];

//        // The next entry (4) is "map", "range", or "vector"
//        if(rowItemList.count() >= 5)
//        {
//          if (rowItemList[4] == "map" || rowItemList[4] == "range")
//          {
//            aType.hasValueData = true;
//            aType.renderValueData = true;
//          }
//          else if (rowItemList[4] == "vector" || rowItemList[4] == "line")
//          {
//            aType.hasVectorData = true;
//            aType.renderVectorData = true;
//            if (rowItemList[4] == "line")
//              aType.arrowHead=StatisticsType::arrowHead_t::none;
//          }
//        }

//        typeParsingActive = true;
//      }
//      else if (rowItemList[1] == "mapColor")
//      {
//        int id = rowItemList[2].toInt();

//        // assign color
//        unsigned char r = (unsigned char)rowItemList[3].toInt();
//        unsigned char g = (unsigned char)rowItemList[4].toInt();
//        unsigned char b = (unsigned char)rowItemList[5].toInt();
//        unsigned char a = (unsigned char)rowItemList[6].toInt();

//        aType.colMapper.type = colorMapper::mappingType::map;
//        aType.colMapper.colorMap.insert(id, QColor(r, g, b, a));
//      }
//      else if (rowItemList[1] == "range")
//      {
//        // This is a range with min/max
//        int min = rowItemList[2].toInt();
//        unsigned char r = (unsigned char)rowItemList[4].toInt();
//        unsigned char g = (unsigned char)rowItemList[6].toInt();
//        unsigned char b = (unsigned char)rowItemList[8].toInt();
//        unsigned char a = (unsigned char)rowItemList[10].toInt();
//        QColor minColor = QColor(r, g, b, a);

//        int max = rowItemList[3].toInt();
//        r = rowItemList[5].toInt();
//        g = rowItemList[7].toInt();
//        b = rowItemList[9].toInt();
//        a = rowItemList[11].toInt();
//        QColor maxColor = QColor(r, g, b, a);

//        aType.colMapper = colorMapper(min, minColor, max, maxColor);
//      }
//      else if (rowItemList[1] == "defaultRange")
//      {
//        // This is a color gradient function
//        int min = rowItemList[2].toInt();
//        int max = rowItemList[3].toInt();
//        QString rangeName = rowItemList[4];

//        aType.colMapper = colorMapper(rangeName, min, max);
//      }
//      else if (rowItemList[1] == "vectorColor")
//      {
//        unsigned char r = (unsigned char)rowItemList[2].toInt();
//        unsigned char g = (unsigned char)rowItemList[3].toInt();
//        unsigned char b = (unsigned char)rowItemList[4].toInt();
//        unsigned char a = (unsigned char)rowItemList[5].toInt();
//        aType.vectorPen.setColor(QColor(r, g, b, a));
//      }
//      else if (rowItemList[1] == "gridColor")
//      {
//        unsigned char r = (unsigned char)rowItemList[2].toInt();
//        unsigned char g = (unsigned char)rowItemList[3].toInt();
//        unsigned char b = (unsigned char)rowItemList[4].toInt();
//        unsigned char a = 255;
//        aType.gridPen.setColor(QColor(r, g, b, a));
//      }
//      else if (rowItemList[1] == "scaleFactor")
//      {
//        aType.vectorScale = rowItemList[2].toInt();
//      }
//      else if (rowItemList[1] == "scaleToBlockSize")
//      {
//        aType.scaleValueToBlockSize = (rowItemList[2] == "1");
//      }
//      else if (rowItemList[1] == "seq-specs")
//      {
//        QString seqName = rowItemList[2];
//        QString layerId = rowItemList[3];
//        // For now do nothing with this information.
//        // Show the file name for this item instead.
//        int width = rowItemList[4].toInt();
//        int height = rowItemList[5].toInt();
//        if (width > 0 && height > 0)
//          statSource.statFrameSize = QSize(width, height);
//        if (rowItemList[6].toDouble() > 0.0)
//          frameRate = rowItemList[6].toDouble();
//      }
//    }

//  } // try
//  catch (const char *str)
//  {
//    std::cerr << "Error while parsing meta data: " << str << '\n';
//    parsingError = QString("Error while parsing meta data: ") + QString(str);
//    return;
//  }
//  catch (...)
//  {
//    std::cerr << "Error while parsing meta data.";
//    parsingError = QString("Error while parsing meta data.");
//    return;
//  }

//  return;
//}

void playlistItemVTMBMSStatisticsFile::loadStatisticToCache(int frameIdxInternal, int typeID)
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
//    if (fileSortedByPOC)
//    {
//      // If the statistics file is sorted by POC we have to start at the first entry of this POC and parse the
//      // file until another POC is encountered. If this is not done, some information from a different typeID
//      // could be ignored during parsing.

//      // Get the position of the first line with the given frameIdxInternal
//      startPos = std::numeric_limits<qint64>::max();
//      for (const qint64 &value : pocTypeStartList[frameIdxInternal])
//        if (value < startPos)
//          startPos = value;
//    }

    // fast forward
    in.seek(startPos);

    // prepare regex
    StatisticsType *aType = statSource.getStatisticsType(typeID);
    Q_ASSERT_X(aType != nullptr, "StatisticsObject::readStatisticsFromFile", "Stat type not found.");
    QRegularExpression typeRegex(aType->typeName); // for catching lines of the type

    // for extracting scalar value statistics, need to match:
    // BlockStat: POC 1 @( 112,  88) [ 8x 8] PredMode=0
    QRegularExpression scalarRegex("POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+=([0-9\\-]+)");
    // for extracting vector value statistics, need to match:
    // BlockStat: POC 1 @( 120,  80) [ 8x 8] MVL0={ -24,  -2}
    QRegularExpression vectorRegex("POC ([0-9]+) @\\( *([0-9]+), *([0-9]+)\\) *\\[ *([0-9]+)x *([0-9]+)\\] *\\w+={ *([0-9\\-]+), *([0-9\\-]+)}");

    while (!in.atEnd())
    {
      // read one line
      QString aLine = in.readLine();

      // filter lines of different types
      QRegularExpressionMatch typeMatch = typeRegex.match(aLine);
      if (typeMatch.hasMatch())
      {

        int poc, posX, posY, width, height, scalar, vecX, vecY;

        QRegularExpressionMatch statisitcMatch;
        // extract statistics info
        if (aType->hasValueData)
        {
          statisitcMatch = scalarRegex.match(aLine);
        }
        else if (aType->hasVectorData)
        {
          statisitcMatch = vectorRegex.match(aLine);
        }

        if (!statisitcMatch.hasMatch())
        {
          parsingError = QString("Error while parsing statistic: ") + QString(aLine);
          continue;
        }

        poc = statisitcMatch.captured(1).toInt();
        posX = statisitcMatch.captured(2).toInt();
        posY = statisitcMatch.captured(3).toInt();
        width = statisitcMatch.captured(4).toInt();
        height = statisitcMatch.captured(5).toInt();

        // if there is a new POC, we are done here!
        if (poc != frameIdxInternal)
          break;


        // Check if block is within the image range
        if (blockOutsideOfFrame_idx == -1 && (posX + width > statSource.statFrameSize.width() || posY + height > statSource.statFrameSize.height()))
          // Block not in image. Warn about this.
          blockOutsideOfFrame_idx = frameIdxInternal;

//        const StatisticsType *statsType = statSource.getStatisticsType(type);
//        Q_ASSERT_X(statsType != nullptr, "StatisticsObject::readStatisticsFromFile", "Stat type not found.");

        if (aType->hasVectorData)
        {
          vecX = statisitcMatch.captured(6).toInt();
          vecY = statisitcMatch.captured(7).toInt();
          statSource.statsCache[typeID].addBlockVector(posX, posY, width, height, vecX, vecY);
        }
//        else if (lineData && statsType->hasVectorData)
//          statSource.statsCache[type].addLine(posX, posY, width, height, values[0], values[1], values[2], values[3]);
        else
        {
          scalar = statisitcMatch.captured(6).toInt();
          statSource.statsCache[typeID].addBlockValue(posX, posY, width, height, scalar);
        }

      }
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

// This timer event is called regularly when the background loading process is running.
// It will update
void playlistItemVTMBMSStatisticsFile::timerEvent(QTimerEvent *event)
{
  if (event->timerId() != timer.timerId())
    return playlistItem::timerEvent(event);

  // Check if the background process is still running. If it is not, no signal are required anymore.
  // The final update signal was emitted by the background process.
  if (!backgroundParserFuture.isRunning())
    timer.stop();
  else
  {
    setStartEndFrame(indexRange(0, maxPOC), false);
    emit signalItemChanged(false, RECACHE_NONE);
  }
}

void playlistItemVTMBMSStatisticsFile::createPropertiesWidget()
{
  // Absolutely always only call this once//
  assert(!propertiesWidget);

  // Create a new widget and populate it with controls
  preparePropertiesWidget(QStringLiteral("playlistItemStatisticsFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame;
  line->setObjectName(QStringLiteral("lineOne"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  vAllLaout->addLayout(createPlaylistItemControls());
  vAllLaout->addWidget(line);
  vAllLaout->addLayout(statSource.createStatisticsHandlerControls());

  // Do not add any stretchers at the bottom because the statistics handler controls will
  // expand to take up as much space as there is available
}

void playlistItemVTMBMSStatisticsFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the YUV file-> We save both in the playlist.
  QUrl fileURL(file.getAbsoluteFilePath());
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(file.getAbsoluteFilePath());

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemStatisticsFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the YUV file (the path to the file-> Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);

  // Save the status of the statistics (which are shown, transparency ...)
  statSource.savePlaylist(d);

  root.appendChild(d);
}

playlistItemVTMBMSStatisticsFile *playlistItemVTMBMSStatisticsFile::newplaylistItemVTMBMSStatisticsFile(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemStatisticsFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemVTMBMSStatisticsFile *newStat = new playlistItemVTMBMSStatisticsFile(filePath);

  // Load the propertied of the playlistItem
  playlistItem::loadPropertiesFromPlaylist(root, newStat);

  // Load the status of the statistics (which are shown, transparency ...)
  newStat->statSource.loadPlaylist(root);

  return newStat;
}

void playlistItemVTMBMSStatisticsFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  allExtensions.append("vtmbmsstats");
  filters.append("Statistics File (*.vtmbmsstats)");
}

void playlistItemVTMBMSStatisticsFile::reloadItemSource()
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
//  readHeaderFromFile();
  readFramePositionsFromFile();
  statSource.updateStatisticsHandlerControls();

  // Run the parsing of the file in the background
  cancelBackgroundParser = false;
//  timer.start(1000, this);
//  backgroundParserFuture = QtConcurrent::run(this, &playlistItemVTMBMSStatisticsFile::readFramePositionsFromFile);
}

void playlistItemVTMBMSStatisticsFile::loadFrame(int frameIdx, bool playback, bool loadRawdata, bool emitSignals)
{
  Q_UNUSED(playback);
  Q_UNUSED(loadRawdata);
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  if (statSource.needsLoading(frameIdxInternal) == LoadingNeeded)
  {
    isStatisticsLoading = true;
    statSource.loadStatistics(frameIdxInternal);
    isStatisticsLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }
}
