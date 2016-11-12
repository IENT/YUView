/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "playlistItemStatisticsFile.h"
#include <assert.h>
#include <QTime>
#include <QDebug>
#include <QtConcurrent>
#include <iostream>

#include "statisticsExtensions.h"

// The internal buffer for parsing the starting positions. The buffer must not be larger than 2GB
// so that we can adress all the positions in it with int (using such a large buffer is not a good
// idea anyways)
#define STAT_PARSING_BUFFER_SIZE 1048576

playlistItemStatisticsFile::playlistItemStatisticsFile(QString itemNameOrFileName)
  : playlistItem(itemNameOrFileName, playlistItem_Indexed)
{
  // Set default variables
  fileSortedByPOC = false;
  blockOutsideOfFrame_idx = -1;
  backgroundParserProgress = 0.0;
  parsingError = "";
  currentDrawnFrameIdx = -1;
  maxPOC = 0;

  // Set statistics icon
  setIcon(0, QIcon(":img_stats.png"));

  file.openFile(itemNameOrFileName);
  if (!file.isOk())
    return;

  // Read the statistics file header
  readHeaderFromFile();

  // Run the parsing of the file in the backfround
  cancelBackgroundParser = false;
  timerId = startTimer(1000);
  backgroundParserFuture = QtConcurrent::run(this, &playlistItemStatisticsFile::readFrameAndTypePositionsFromFile);

  connect(&statSource, SIGNAL(updateItem(bool)), this, SLOT(updateStatSource(bool)));
  connect(&statSource, SIGNAL(requestStatisticsLoading(int,int)), this, SLOT(loadStatisticToCache(int,int)));
}

playlistItemStatisticsFile::~playlistItemStatisticsFile()
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

QList<infoItem> playlistItemStatisticsFile::getInfoList()
{
  QList<infoItem> infoList;

  // Append the file information (path, date created, file size...)
  infoList.append( file.getFileInfoList() );

  // Is the file sorted by POC?
  infoList.append(infoItem("Sorted by POC", fileSortedByPOC ? "Yes" : "No"));

  // Show the progress of the background parsing (if running)
  if (backgroundParserFuture.isRunning())
    infoList.append(infoItem("Parsing:", QString("%1%...").arg(backgroundParserProgress, 0, 'f', 2) ));

  // Print a warning if one of the blocks in the statistics file is outside of the defined "frame size"
  if (blockOutsideOfFrame_idx != -1)
    infoList.append(infoItem("Warning", QString("A block in frame %1 is outside of the given size of the statistics.").arg(blockOutsideOfFrame_idx)));

  // Show any errors that occured during parsing
  if (!parsingError.isEmpty())
    infoList.append(infoItem("Parsing Error:", parsingError));

  return infoList;
}

void playlistItemStatisticsFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback)
{
  Q_UNUSED(playback);

  // Tell the statSource to draw the statistics
  statSource.paintStatistics(painter, frameIdx, zoomFactor);

  // Currently this frame is drawn.
  currentDrawnFrameIdx = frameIdx;
}

/** The background task that parsese the file and extracts the exact file positions
* where a new frame or a new type starts. If the user then later requests this type/POC
* we can directly jump there and parse the actual information. This way we don't have to
* scan the whole file which can get very slow for large files.
*
* This function might emit the objectInformationChanged() signal if something went wrong,
* setting the error message, or if parsing finished successfully.
*/
void playlistItemStatisticsFile::readFrameAndTypePositionsFromFile()
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
        // Search for '\n' newline charachters
        if (inputBuffer.at(i) == 10)
        {
          // We found a newline character
          if (lineBuffer.size() > 0)
          {
            // Parse the previous line
            // get components of this line
            QStringList rowItemList = parseCSVLine(lineBuffer, ';');

            // ignore empty entries and headers
            if (!rowItemList[0].isEmpty() && rowItemList[0][0] != '%')
            {
              // check for POC/type information
              int poc = rowItemList[0].toInt();
              int typeID = rowItemList[5].toInt();

              if (lastType == -1 && lastPOC == -1)
              {
                // First POC/type line
                pocTypeStartList[poc][typeID] = lineBufferStartPos;
                if (poc == currentDrawnFrameIdx)
                  // We added a start pos for the frame index that is currently drawn. We might have to redraw.
                  emit signalItemChanged(true, false);

                lastType = typeID;
                lastPOC = poc;

                // update number of frames
                if (poc > maxPOC)
                  maxPOC = poc;
              }
              else if (typeID != lastType && poc == lastPOC)
              {
                // we found a new type but the POC stayed the same.
                // This seems to be an interleaved file
                // Check if we already collected a start position for this type
                fileSortedByPOC = true;
                lastType = typeID;
                if (!pocTypeStartList[poc].contains(typeID))
                {
                  pocTypeStartList[poc][typeID] = lineBufferStartPos;
                  if (poc == currentDrawnFrameIdx)
                    // We added a start pos for the frame index that is currently drawn. We might have to redraw.
                    emit signalItemChanged(true, false);
                }
              }
              else if (poc != lastPOC)
              {
                // We found a new POC
                if (fileSortedByPOC)
                {
                  // There must not be a start position for any type with this POC already.
                  if (pocTypeStartList.contains(poc))
                    throw "The data for each POC must be continuous in an interleaved statistics file->";
                }
                else
                {
                  // There must not be a start position for this POC/type already.
                  if (pocTypeStartList.contains(poc) && pocTypeStartList[poc].contains(typeID))
                    throw "The data for each typeID must be continuous in an non interleaved statistics file->";
                }

                lastPOC = poc;
                lastType = typeID;

                pocTypeStartList[poc][typeID] = lineBufferStartPos;
                if (poc == currentDrawnFrameIdx)
                  // We added a start pos for the frame index that is currently drawn. We might have to redraw.
                  emit signalItemChanged(true, false);

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
          // No newline character found
          lineBuffer.append( inputBuffer.at(i) );
      }

      bufferStartPos += bufferSize;
    }

    // Parsing complete
    backgroundParserProgress = 100.0;

    setStartEndFrame( indexRange(0, maxPOC), false );
    emit signalItemChanged(false, false);

  } // try
  catch (const char * str)
  {
    std::cerr << "Error while parsing meta data: " << str << '\n';
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    emit signalItemChanged(false, false);
    return;
  }
  catch (...)
  {
    std::cerr << "Error while parsing meta data.";
    parsingError = QString("Error while parsing meta data.");
    emit signalItemChanged(false, false);
    return;
  }

  return;
}

void playlistItemStatisticsFile::readHeaderFromFile()
{
  try
  {
    if (!file.isOk())
      return;
    
    // Cleanup old types
    statSource.clearStatTypes();

    // scan headerlines first
    // also count the lines per Frame for more efficient memory allocation
    // if an ID is used twice, the data of the first gets overwritten
    bool typeParsingActive = false;
    StatisticsType aType;

    while (!file.atEnd())
    {
      // read one line
      QByteArray aLineByteArray = file.readLine();
      QString aLine(aLineByteArray);

      // get components of this line
      QStringList rowItemList = parseCSVLine(aLine, ';');

      if (rowItemList[0].isEmpty())
        continue;

      // either a new type or a line which is not header finishes the last type
      if (((rowItemList[1] == "type") || (rowItemList[0][0] != '%')) && typeParsingActive)
      {
        // Last type is complete. Store this initial state.
        aType.setInitialState();
        statSource.addStatType(aType);

        // start from scratch for next item
        aType = StatisticsType();
        typeParsingActive = false;

        // if we found a non-header line, stop here
        if (rowItemList[0][0] != '%')
          return;
      }

      if (rowItemList[1] == "type")   // new type
      {
        aType.typeID = rowItemList[2].toInt();
        aType.typeName = rowItemList[3];

        // The next entry (4) is "map", "range", or "vector"
        if( rowItemList.count() >= 5 )
          if (rowItemList[4] == "map" || rowItemList[4] == "range")
          {
            aType.hasValueData = true;
            aType.renderValueData = true;
          }
          else if (rowItemList[4] == "vector") 
          {
            aType.hasVectorData = true;
            aType.renderVectorData = true;
          }

        typeParsingActive = true;
      }
      else if (rowItemList[1] == "mapColor")
      {
        int id = rowItemList[2].toInt();

        // assign color
        unsigned char r = (unsigned char)rowItemList[3].toInt();
        unsigned char g = (unsigned char)rowItemList[4].toInt();
        unsigned char b = (unsigned char)rowItemList[5].toInt();
        unsigned char a = (unsigned char)rowItemList[6].toInt();
        
        aType.colMapper.type = colorMapper::mappingType::map;
        aType.colMapper.colorMap.insert(id, QColor(r, g, b, a));
      }
      else if (rowItemList[1] == "range")
      {
        // This is a range with min/max
        int min = rowItemList[2].toInt();
        unsigned char r = (unsigned char)rowItemList[4].toInt();
        unsigned char g = (unsigned char)rowItemList[6].toInt();
        unsigned char b = (unsigned char)rowItemList[8].toInt();
        unsigned char a = (unsigned char)rowItemList[10].toInt();
        QColor minColor = QColor(r, g, b, a);
        
        int max = rowItemList[3].toInt();
        r = rowItemList[5].toInt();
        g = rowItemList[7].toInt();
        b = rowItemList[9].toInt();
        a = rowItemList[11].toInt();
        QColor maxColor = QColor(r, g, b, a);
        
        aType.colMapper = colorMapper(min, minColor, max, maxColor);
      }
      else if (rowItemList[1] == "defaultRange")
      {
        // This is a color gradient function
        int min = rowItemList[2].toInt();
        int max = rowItemList[3].toInt();
        QString rangeName = rowItemList[4];
        
        aType.colMapper = colorMapper(rangeName, min, max);
      }
      else if (rowItemList[1] == "vectorColor")
      {
        unsigned char r = (unsigned char)rowItemList[2].toInt();
        unsigned char g = (unsigned char)rowItemList[3].toInt();
        unsigned char b = (unsigned char)rowItemList[4].toInt();
        unsigned char a = (unsigned char)rowItemList[5].toInt();
        aType.vectorPen.setColor(QColor(r, g, b, a));
      }
      else if (rowItemList[1] == "gridColor")
      {
        unsigned char r = (unsigned char)rowItemList[2].toInt();
        unsigned char g = (unsigned char)rowItemList[3].toInt();
        unsigned char b = (unsigned char)rowItemList[4].toInt();
        unsigned char a = 255;
        aType.gridPen.setColor(QColor(r, g, b, a));
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
        QString seqName = rowItemList[2];
        QString layerId = rowItemList[3];
        // For now do nothing with this information.
        // Show the file name for this item instead.
        int width = rowItemList[4].toInt();
        int height = rowItemList[5].toInt();
        if (width > 0 && height > 0)
          statSource.statFrameSize = QSize(width, height);
        if (rowItemList[6].toDouble() > 0.0)
          frameRate = rowItemList[6].toDouble();
      }
    }

  } // try
  catch (const char * str)
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

void playlistItemStatisticsFile::loadStatisticToCache(int frameIdx, int typeID)
{
  try
  {
    if (!file.isOk())
      return;

    QTextStream in( file.getQFile() );

    if (!pocTypeStartList.contains(frameIdx) || !pocTypeStartList[frameIdx].contains(typeID))
      return;

    qint64 startPos = pocTypeStartList[frameIdx][typeID];
    if (fileSortedByPOC)
    {
      // If the statistics file is sorted by POC we have to start at the first entry of this POC and parse the
      // file until another POC is encountered. If this is not done, some information from a different typeID
      // could be ignored during parsing.

      // Get the position of the first line with the given frameIdx
      startPos = std::numeric_limits<qint64>::max();
      QMap<int, qint64>::iterator it;
      for (it = pocTypeStartList[frameIdx].begin(); it != pocTypeStartList[frameIdx].end(); ++it)
        if (it.value() < startPos)
          startPos = it.value();
    }

    // fast forward
    in.seek(startPos);

    while (!in.atEnd())
    {
      // read one line
      QString aLine = in.readLine();

      // get components of this line
      QStringList rowItemList = parseCSVLine(aLine, ';');

      if (rowItemList[0].isEmpty())
        continue;

      int poc = rowItemList[0].toInt();
      int type = rowItemList[5].toInt();

      // if there is a new poc, we are done here!
      if (poc != frameIdx)
        break;
      // if there is a new type and this is a non interleaved file, we are done here.
      if (!fileSortedByPOC && type != typeID)
        break;

      int value1 = rowItemList[6].toInt();
      int value2 = 0;
      bool vectorData = false;
      if (rowItemList.count() >= 8)
      {
        value2 = rowItemList[7].toInt();
        vectorData = true;
      }

      int posX = rowItemList[1].toInt();
      int posY = rowItemList[2].toInt();
      int width = rowItemList[3].toUInt();
      int height = rowItemList[4].toUInt();

      // Check if block is within the image range
      if (blockOutsideOfFrame_idx == -1 && (posX + width > statSource.statFrameSize.width() || posY + height > statSource.statFrameSize.height()))
        // Block not in image. Warn about this.
        blockOutsideOfFrame_idx = frameIdx;

      StatisticsType *statsType = statSource.getStatisticsType(type);
      Q_ASSERT_X(statsType != NULL, "StatisticsObject::readStatisticsFromFile", "Stat type not found.");

      if (vectorData && statsType->hasVectorData)
        statSource.statsCache[type].addBlockVector(posX, posY, width, height, value1, value2);
      else
        statSource.statsCache[type].addBlockValue(posX, posY, width, height, value1);
    }

  } // try
  catch (const char * str)
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

QStringList playlistItemStatisticsFile::parseCSVLine(QString line, char delimiter)
{
  // first, trim newline and whitespaces from both ends of line
  line = line.trimmed().replace(" ", "");

  // now split string with delimiter
  return line.split(delimiter);
}

// This timer event is called regularly when the background loading process is running.
// It will update
void playlistItemStatisticsFile::timerEvent(QTimerEvent * event)
{
  Q_UNUSED(event);

  // Check if the background process is still running. If it is not, no signal are required anymore.
  // The final update signal was emitted by the background process.
  if (!backgroundParserFuture.isRunning())
    killTimer(timerId);
  else
  {
    setStartEndFrame( indexRange(0, maxPOC), false );
    emit signalItemChanged(false, false);
  }
}

void playlistItemStatisticsFile::createPropertiesWidget()
{
  // Absolutely always only call this once//
  assert (propertiesWidget == NULL);

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemStatisticsFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);

  QFrame *line = new QFrame(propertiesWidget);
  line->setObjectName(QStringLiteral("lineOne"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  vAllLaout->addLayout( createPlaylistControls() );
  vAllLaout->addWidget( line );
  vAllLaout->addLayout( statSource.createStatisticsHandlerControls() );

  // Do not add any stretchers at the bottom because the statistics handler controls will
  // expand to take up as much space as there is available

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

void playlistItemStatisticsFile::savePlaylist(QDomElement &root, QDir playlistDir)
{
  // Determine the relative path to the yuv file-> We save both in the playlist.
  QUrl fileURL( file.getAbsoluteFilePath() );
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath( file.getAbsoluteFilePath() );

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemStatisticsFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);
  
  // Apppend all the properties of the yuv file (the path to the file-> Relative and absolute)
  d.appendProperiteChild( "absolutePath", fileURL.toString() );
  d.appendProperiteChild( "relativePath", relativePath  );

  // Save the status of the statistics (which are shown, transparency ...)
  statSource.savePlaylist(d);

  root.appendChild(d);
}

playlistItemStatisticsFile *playlistItemStatisticsFile::newplaylistItemStatisticsFile(QDomElementYUView root, QString playlistFilePath)
{
  // Parse the dom element. It should have all values of a playlistItemStatisticsFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return NULL;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemStatisticsFile *newStat = new playlistItemStatisticsFile(filePath);

  // Load the propertied of the playlistItem
  playlistItem::loadPropertiesFromPlaylist(root, newStat);

  // Load the status of the statistics (which are shown, transparency ...)
  newStat->statSource.loadPlaylist(root);

  return newStat;
}

void playlistItemStatisticsFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  allExtensions.append("csv");
  filters.append("Statistics File (*.csv)");
}

void playlistItemStatisticsFile::reloadItemSource()
{
  // Set default variables
  fileSortedByPOC = false;
  blockOutsideOfFrame_idx = -1;
  backgroundParserProgress = 0.0;
  parsingError = "";
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
  pocTypeStartList.clear();
  statSource.statsCache.clear();
  statSource.statsCacheFrameIdx = -1;

  // Reopen the file
  file.openFile(plItemNameOrFileName);
  if (!file.isOk())
    return;

  // Read the new statistics file header
  readHeaderFromFile();

  statSource.updateStatisticsHandlerControls();

  // Run the parsing of the file in the backfround
  cancelBackgroundParser = false;
  timerId = startTimer(1000);
  backgroundParserFuture = QtConcurrent::run(this, &playlistItemStatisticsFile::readFrameAndTypePositionsFromFile);
}