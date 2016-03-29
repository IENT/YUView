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

playlistItemStatisticsFile::playlistItemStatisticsFile(QString itemNameOrFileName) 
  : playlistItem(itemNameOrFileName)
{
  // Set default variables
  fileSortedByPOC = false;
  blockOutsideOfFrame_idx = -1;
  backgroundParserProgress = 0.0;
  parsingError = "";

  // Set statistics icon
  setIcon(0, QIcon(":stats.png"));

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

void playlistItemStatisticsFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor)
{
  // draw statistics (inverse order)
  for (int i = statSource.statsTypeList.count() - 1; i >= 0; i--)
  {
    if (!statSource.statsTypeList[i].render)
      continue;

    // If the statistics for this frame index were not loaded yet, do this now.
    int typeIdx = statSource.statsTypeList[i].typeID;
    if (!statSource.statsCache.contains(frameIdx) || !statSource.statsCache[frameIdx].contains(typeIdx))
    {
      loadStatisticToCache(frameIdx, typeIdx);
    }

    StatisticsItemList stat = statSource.statsCache[frameIdx][typeIdx];

    statSource.paintStatistics(painter, stat, statSource.statsTypeList[i], zoomFactor);
  }

  statSource.lastFrameIdx = frameIdx;
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
  try {
    // Open the file (again). Since this is a background process, we open the file again to 
    // not disturb any reading from not background code.
    QFile inputFile(file.absoluteFilePath());
    if (!inputFile.open(QIODevice::ReadOnly))
      return;

    int lastPOC = INT_INVALID;
    int lastType = INT_INVALID;
    int numFrames = 0;
    while (!inputFile.atEnd() && !cancelBackgroundParser)
    {
      qint64 lineStartPos = inputFile.pos();

      // read one line
      QByteArray aLineByteArray = inputFile.readLine();
      QString aLine(aLineByteArray);

      // get components of this line
      QStringList rowItemList = parseCSVLine(aLine, ';');

      // ignore empty stuff
      if (rowItemList[0].isEmpty())
        continue;

      // ignore headers
      if (rowItemList[0][0] == '%')
        continue;

      // check for POC/type information
      int poc = rowItemList[0].toInt();
      int typeID = rowItemList[5].toInt();

      if (lastType == -1 && lastPOC == -1)
      {
        // First POC/type line
        pocTypeStartList[poc][typeID] = lineStartPos;
        lastType = typeID;
        lastPOC = poc;
        numFrames++;
        nrFrames = numFrames;
        statSource.updateStartEndFrameLimit( indexRange(0, nrFrames) );
      }
      else if (typeID != lastType && poc == lastPOC)
      {
        // we found a new type but the POC stayed the same.
        // This seems to be an interleaved file
        // Check if we already collected a start position for this type
        fileSortedByPOC = true;
        lastType = typeID;
        if (pocTypeStartList[poc].contains(typeID))
          // POC/type start position already collected
          continue;
        pocTypeStartList[poc][typeID] = lineStartPos;
      }
      else if (poc != lastPOC)
      {
        // We found a new POC
        lastPOC = poc;
        lastType = typeID;
        if (fileSortedByPOC)
        {
          // There must not be a start position for any type with this POC already.
          if (pocTypeStartList.contains(poc))
            throw "The data for each POC must be continuous in an interleaved statistics file.";
        }
        else
        {
          // There must not be a start position for this POC/type already.
          if (pocTypeStartList.contains(poc) && pocTypeStartList[poc].contains(typeID))
            throw "The data for each typeID must be continuous in an non interleaved statistics file.";
        }
        pocTypeStartList[poc][typeID] = lineStartPos;

        // update number of frames
        if (poc + 1 > numFrames)
        {
          numFrames = poc + 1;
          nrFrames = numFrames;
          statSource.updateStartEndFrameLimit( indexRange(0, nrFrames) );
        }
        
        // Update percent of file parsed
        backgroundParserProgress = ((double)lineStartPos * 100 / (double)inputFile.size());
      }
      // typeID and POC stayed the same
      // do nothing
    }

    // Parsing complete
    backgroundParserProgress = 100.0;
    emit signalItemChanged(false);

    inputFile.close();

  } // try
  catch (const char * str) {
    std::cerr << "Error while parsing meta data: " << str << '\n';
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    emit signalItemChanged(false);
    return;
  }
  catch (...) {
    std::cerr << "Error while parsing meta data.";
    parsingError = QString("Error while parsing meta data.");
    emit signalItemChanged(false);
    return;
  }

  return;
}

void playlistItemStatisticsFile::readHeaderFromFile()
{
  try {
    if (!file.isOk())
      return;
    
    // cleanup old types
    statSource.statsTypeList.clear();

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
        // last type is complete
        statSource.statsTypeList.append(aType);

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

        aType.readFromRow(rowItemList); // get remaining info from row
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
        aType.colorMap[id] = QColor(r, g, b, a);
      }
      else if (rowItemList[1] == "range")
      {
        aType.colorRange = new ColorRange(rowItemList);
      }
      else if (rowItemList[1] == "defaultRange")
      {
        aType.colorRange = new DefaultColorRange(rowItemList);
      }
      else if (rowItemList[1] == "vectorColor")
      {
        unsigned char r = (unsigned char)rowItemList[2].toInt();
        unsigned char g = (unsigned char)rowItemList[3].toInt();
        unsigned char b = (unsigned char)rowItemList[4].toInt();
        unsigned char a = (unsigned char)rowItemList[5].toInt();
        aType.vectorColor = QColor(r, g, b, a);
      }
      else if (rowItemList[1] == "gridColor")
      {
        unsigned char r = (unsigned char)rowItemList[2].toInt();
        unsigned char g = (unsigned char)rowItemList[3].toInt();
        unsigned char b = (unsigned char)rowItemList[4].toInt();
        unsigned char a = 255;
        aType.gridColor = QColor(r, g, b, a);
      }
      else if (rowItemList[1] == "scaleFactor")
      {
        aType.vectorSampling = rowItemList[2].toInt();
      }
      else if (rowItemList[1] == "scaleToBlockSize")
      {
        aType.scaleToBlockSize = (rowItemList[2] == "1");
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
          statSource.frameRate = rowItemList[6].toDouble();
      }
    }

  } // try
  catch (const char * str) {
    std::cerr << "Error while parsing meta data: " << str << '\n';
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    return;
  }
  catch (...) {
    std::cerr << "Error while parsing meta data.";
    parsingError = QString("Error while parsing meta data.");
    return;
  }

  return;
}

void playlistItemStatisticsFile::loadStatisticToCache(int frameIdx, int typeID)
{
  try {
    if (!file.isOk())
      return;

    StatisticsItem anItem;
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
      int value2 = (rowItemList.count() >= 8) ? rowItemList[7].toInt() : 0;

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
      anItem.type = ((statsType->visualizationType == colorMapType) || (statsType->visualizationType == colorRangeType)) ? blockType : arrowType;

      anItem.positionRect = QRect(posX, posY, width, height);

      anItem.rawValues[0] = value1;
      anItem.rawValues[1] = value2;
      anItem.color = QColor();

      if (statsType->visualizationType == colorMapType)
      {
        ColorMap colorMap = statsType->colorMap;
        anItem.color = colorMap[value1];
      }
      else if (statsType->visualizationType == colorRangeType)
      {
        if (statsType->scaleToBlockSize)
          anItem.color = statsType->colorRange->getColor((float)value1 / (float)(anItem.positionRect.width() * anItem.positionRect.height()));
        else
          anItem.color = statsType->colorRange->getColor((float)value1);
      }
      else if (statsType->visualizationType == vectorType)
      {
        // find color
        anItem.color = statsType->vectorColor;

        // calculate the vector size
        anItem.vector[0] = (float)value1 / statsType->vectorSampling;
        anItem.vector[1] = (float)value2 / statsType->vectorSampling;
      }

      // set grid color. if unset for this type, use color of type for grid, too
      if (statsType->gridColor.isValid())
      {
        anItem.gridColor = statsType->gridColor;
      }
      else
      {
        anItem.gridColor = anItem.color;
      }

      statSource.statsCache[poc][type].append(anItem);
    }
    

  } // try
  catch (const char * str) {
    std::cerr << "Error while parsing: " << str << '\n';
    parsingError = QString("Error while parsing meta data: ") + QString(str);
    return;
  }
  catch (...) {
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

void playlistItemStatisticsFile::timerEvent(QTimerEvent * event)
{
  Q_UNUSED(event);

  emit signalItemChanged(false);
  
  // Check if the background process is still running. If not, stop the timer
  if (!backgroundParserFuture.isRunning())
    killTimer(timerId);
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
  vAllLaout->addLayout( statSource.createStatisticsHandlerControls(propertiesWidget) );
}