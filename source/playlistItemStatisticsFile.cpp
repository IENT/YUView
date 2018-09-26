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

#include "playlistItemStatisticsFile.h"

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

playlistItemStatisticsFile::playlistItemStatisticsFile(const QString &itemNameOrFileName)
  : playlistItem(itemNameOrFileName, playlistItem_Indexed)
{
  // Set default variables
  fileSortedByPOC = false;
  blockOutsideOfFrame_idx = -1;
  backgroundParserProgress = 0.0;
  currentDrawnFrameIdx = -1;
  maxPOC = 0;
  isStatisticsLoading = false;

  // Set statistics icon
  setIcon(0, convertIcon(":img_stats.png"));

  file.openFile(itemNameOrFileName);
  if (!file.isOk())
    return;
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

infoData playlistItemStatisticsFile::getInfo() const
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

void playlistItemStatisticsFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  // drawRawData only controls the drawing of raw pixel values
  Q_UNUSED(drawRawData);
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  // Tell the statSource to draw the statistics
  statSource.paintStatistics(painter, frameIdxInternal, zoomFactor);

  // Currently this frame is drawn.
  currentDrawnFrameIdx = frameIdxInternal;
}

// This timer event is called regularly when the background loading process is running.
// It will update
void playlistItemStatisticsFile::timerEvent(QTimerEvent *event)
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

void playlistItemStatisticsFile::createPropertiesWidget()
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

void playlistItemStatisticsFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
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

void playlistItemStatisticsFile::loadFrame(int frameIdx, bool playback, bool loadRawdata, bool emitSignals)
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

QMap<QString, QList<QList<QVariant>>>* playlistItemStatisticsFile::getData(indexRange aRange, bool aReset, QString aType)
{   
  // getting the max range
  indexRange realRange = this->getFrameIdxRange();

  int rangeSize = aRange.second - aRange.first;
  int frameSize = realRange.second - realRange.first;

  if(aReset || (rangeSize != frameSize))
    this->mStatisticData.clear();

  // running through the statisticsList
  foreach (StatisticsType statType, this->chartStatSource.getStatisticsTypeList())
  {
    if(aType == "" || aType == statType.typeName)
    {
      // creating the resultList, where we save all the datalists
      QList<QList<QVariant>> resultList;
      // getting the key
      QString key  = statType.typeName;
      // creating the data list
      QList<QVariant> dataList;

      // getting all the statistic-data by the typeId
      int typeIdx = statType.typeID;

      if (this->isRangeInside(realRange, aRange))
      {
        for(int frame = aRange.first; frame <= aRange.second; frame++)
        {
          dataList.clear();

          // first we have to load the statistic
          this->loadStatisticToCache(this->getFrameIdxInternal(frame), typeIdx);

          statisticsData statDataByType = this->chartStatSource.statsCache[typeIdx];
          // the data can be a value or a vector, converting the data into an QVariant and append it to the dataList
          if(statType.hasValueData)
          {
            foreach (statisticsItem_Value val, statDataByType.valueData)
            {
              QVariant variant = QVariant::fromValue(val);
              dataList.append(variant);
            }
          }
          else if(statType.hasVectorData)
          {
            foreach (statisticsItem_Vector val, statDataByType.vectorData)
            {
              QVariant variant = QVariant::fromValue(val);
              dataList.append(variant);
            }
          }
          // appending the data to the resultList
          resultList.append(dataList);
        }
        // adding each key with the resultList, inside of the resultList
        this->mStatisticData.insert(key, resultList);
      }
      if(aType != "")
        break;
    }
  }
  return &this->mStatisticData;
}

bool playlistItemStatisticsFile::isRangeInside(indexRange aOriginalRange, indexRange aCheckRange)
{
  bool wrongDimensionsOriginalRange   = aOriginalRange.first > aOriginalRange.second;
  bool wrongDimensionsCheckRange      = aCheckRange.first > aCheckRange.second;
  bool firstdimension                 = aOriginalRange.first <= aCheckRange.first;
  bool seconddimension                = aOriginalRange.second >= aCheckRange.second;

  return wrongDimensionsOriginalRange || wrongDimensionsCheckRange || (firstdimension && seconddimension);
}

QList<collectedData>* playlistItemStatisticsFile::sortAndCategorizeData(const QString aType, const int aFrameIndex)
{
  auto getSmallestKey = [] (QList<QString> aMapKeys) -> QString {
    int smallestFoundNumber = INT_MAX;
    QString numberString = "";
    QString resultKey = ""; // just a holder

    // getting the smallest number and the label
    foreach (QString label, aMapKeys)
    {
      if(numberString != "") // the if is necessary, otherwise it will crash on windows
        numberString.clear(); // cleaning the String

      for (int run = 0; run < label.length(); run++)
      {
        if(label[run] != 'x') // finding the number befor the 'x'
         numberString.append(label[run]); // creating the number from the chars
        else // we have found the 'x' so the number is finished
          break;
      }

      int number = numberString.toInt(); // convert to int

      // check if we have found the smallest number
      if(number < smallestFoundNumber)
      {
        // found a smaller number so hold it
        smallestFoundNumber = number;
        // we hold the label, so we dont need to "create / build" the key again
        resultKey = label;
      }
    }
    return resultKey;
  };

  //prepare the result
  QMap<QString, QMap<int, int*>*>* dataMapStatisticsItemValue = new QMap<QString, QMap<int, int*>*>;
  QMap<QString, QHash<QPoint, int*>*>* dataMapStatisticsItemVector = new QMap<QString, QHash<QPoint, int*>*>;

  // define the result list
  QList<collectedData>* resultData = new QList<collectedData>;

  indexRange range(aFrameIndex, aFrameIndex);
  this->getData(range, true, aType);

  // getting allData from the type
  QList<QList<QVariant>> allData = this->mStatisticData.value(aType);

  //check that we have a result
  if(allData.count() <= 0) // in this case, we have no result so we can return at this point
    return resultData;

  // getting the data depends on the actual selected frameIndex / POC
  QList<QVariant> data = allData.at(0);

  // now we go thru all elements of the frame
  foreach (QVariant item, data)
  {
    // item-value was defined by statisticsItem_Value @see statisticsExtensions.h
    if(item.canConvert<statisticsItem_Value>())
    {
      statisticsItem_Value value = item.value<statisticsItem_Value>();
      // creating the label: widht x height
      QString label = QString::number(value.size[0]) + "x" + QString::number(value.size[1]);

      int* chartDepthCnt;

      // hard part of the function
      // 1. check if label is in map
      // 2. if not: insert label and a new / second Map with the new values for depth
      // 3. if it was inside: check if Depth was inside the second map
      // 4. if not in second map create new Depth-data-container, fill with data and add to second map
      // 5. if it was in second map just increment the Depth-Counter
      if(!dataMapStatisticsItemValue->contains(label))
      {
        // label was not inside
        QMap<int, int*>* map = new QMap<int, int*>();

        // create Data, set to 0 and increment (or set count to the value 1, same as set to 0 and increment) and add to second map
        chartDepthCnt = new int[2];

        chartDepthCnt[0] = value.value;
        chartDepthCnt[1] = 1;

        map->insert(chartDepthCnt[0], chartDepthCnt);
        dataMapStatisticsItemValue->insert(label, map);
      }
      else
      {
        // label was inside, check if Depth-value is inside
        QMap<int, int*>* map = dataMapStatisticsItemValue->value(label);

        // Depth-Value not inside
        if(!(map->contains(value.value)))
        {
          chartDepthCnt = new int[2];                   // creating new result
          chartDepthCnt[0] = value.value;               // holding the value
          chartDepthCnt[1] = 0;                         // initialise counter to 0
          chartDepthCnt[1]++;                           // increment the counter
          map->insert(chartDepthCnt[0], chartDepthCnt); // at least add to list
        }
        else  // Depth-Value was inside
        {
          // finding the result, every item "value" is just one time in the list
          int* counter = map->value(value.value);
          counter[1]++; // increment the counter
        }
      }
    }

    // same procedure as statisticsItem_Value but at some points it is different
    // in case of statisticsItem_Vector
    if(item.canConvert<statisticsItem_Vector>())
    {
      statisticsItem_Vector vector = item.value<statisticsItem_Vector>();
      QPoint value = vector.point[0];
      short width = vector.size[0];
      short height = vector.size[1];

      QString label = QString::number(width) + "x" + QString::number(height);
      int* chartValueCnt;

      if(!dataMapStatisticsItemVector->contains(label))
      {
        // label was not inside
        QHash<QPoint, int*>* map = new QHash<QPoint, int*>();

        // create Data, set to 0 and increment (or set count to the value 1, same as set to 0 and increment) and add to second map
        chartValueCnt = new int[1];

        chartValueCnt[0] = 1;

        map->insert(value, chartValueCnt);
        dataMapStatisticsItemVector->insert(label, map);
      }
      else
      {
        // label was inside, check if Point-value is inside
        QHash<QPoint, int*>* map = dataMapStatisticsItemVector->value(label);

        // Depth-Value not inside
        if(!(map->contains(value)))
        {
          chartValueCnt = new int[1];                   // creating new result
          chartValueCnt[0] = 0;                         // initialise counter to 0
          chartValueCnt[0]++;                           // increment the counter
          map->insert(value, chartValueCnt); // at least add to list
        }
        else  // Point-Value was inside
        {
          // finding the result, every item "value" is just one time in the list
          int* counter = map->value(value);
          counter[0]++; // increment the counter
        }
      }
    }
  }

  // at least we order the data based on the width & height (from low to high) and make the data handling easier

  // first init maxElemtents with the amount of data in dataMapStatisticsItemValue
  int maxElementsToNeed = dataMapStatisticsItemValue->keys().count();

  // if we have the type statisticsItem_Value   -- no if, the brackets should improve reading the code
  {
    while(resultData->count() < maxElementsToNeed)
    {
      QString key = getSmallestKey(dataMapStatisticsItemValue->keys());

      // getting the data depends on the "smallest" key
      QMap<int, int*>* map = dataMapStatisticsItemValue->value(key);

      collectedData data;   // creating the data
      data.mStatDataType = sdtStructStatisticsItem_Value;
      data.mLabel = key;    // setting the label

      // copy each data into the list
      foreach (int value, map->keys())
      {
        int* valueAmount = map->value(value);
        data.addValue(QVariant::fromValue(valueAmount[0]), valueAmount[1]);
      }

      // appending the collectedData to the result
      resultData->append(data);

      // reset settings to find
      dataMapStatisticsItemValue->remove(key);
      key.clear();
    }

    // we can delete the dataMap, cause we dont need anymore
    foreach (QString key, dataMapStatisticsItemValue->keys())
    {
      QMap<int, int*>* valuesmap = dataMapStatisticsItemValue->value(key);
      foreach (int valuekey, valuesmap->keys())
      {
        delete valuesmap->value(valuekey);
      }
      delete valuesmap;
    }
    delete dataMapStatisticsItemValue;
  }

  // if we have the type statisticsItem_Value
  maxElementsToNeed += dataMapStatisticsItemVector->keys().count(); // calculate the maximum new, because we add the second map

  { // same as above, the brackets should improve reading the code
    // do ordering and  convert to the struct
    while(resultData->count() < maxElementsToNeed)
    {
      QString key = getSmallestKey(dataMapStatisticsItemVector->keys());

      // getting the data dataMapStatisticsItemVector on the "smallest" key
      auto map = dataMapStatisticsItemVector->value(key);

      collectedData data;   // creating the data
      data.mStatDataType = sdtStructStatisticsItem_Vector;
      data.mLabel = key;    // setting the label

      // copy each data into the list
      foreach (QPoint value, map->keys())
        data.addValue(value, *(map->value(value)));

      // appending the collectedData to the result
      resultData->append(data);

      // reset settings to find
      dataMapStatisticsItemVector->remove(key);
      key.clear();
    }

    // we can delete the dataMap, cause we dont need anymore
    foreach (auto key, dataMapStatisticsItemVector->keys())
    {
      auto valuesmap = dataMapStatisticsItemVector->value(key);
      foreach (auto valuekey, valuesmap->keys())
      {
        delete valuesmap->value(valuekey);
      }
      delete valuesmap;
    }
    delete dataMapStatisticsItemVector;

  }


//  // a debug output
//  for(int i = 0; i< resultData->count(); i++) {
//    collectedData cd = resultData->at(i);
//    foreach (int* valuePair, cd.mValueList) {
//      QString debugstring(cd.mLabel + ": " + QString::number(valuePair[0]) + " : " + QString::number(valuePair[1]));
//      qDebug() << debugstring;
//    }
//  }

  return resultData;
}

QList<collectedData>* playlistItemStatisticsFile::sortAndCategorizeDataByRange(const QString aType, const indexRange aRange)
{
  this->chartStatSource.statsCache.clear();

  //if we have the same frame --> just one frame we look at
  if(aRange.first == aRange.second) // same frame --> just one frame same as current frame
    return this->sortAndCategorizeData(aType, aRange.first);

  // we create a tempory list, to collect all data from all frames
  // and we start to sort them by the label
  QList<collectedData*>* preResult = new QList<collectedData*>();

  // next step get the data for each frame
  for (int frame = aRange.first; frame <= aRange.second; frame++)
  {
    // get the data for the actual frame
    QList<collectedData>* collectedDataByFrameList = this->sortAndCategorizeData(aType, frame);

    // now we have to integrate the new Data from one Frame to all other frames
    for(int i = 0; i< collectedDataByFrameList->count(); i++)
    {
      collectedData frameData = collectedDataByFrameList->at(i);

      bool wasnotinside = true;

      // first: check if we have the collected data-label inside of our result list
      for(int j = 0; j < preResult->count(); j++)
      {
        collectedData* resultCollectedData = preResult->at(j);

        if(*resultCollectedData == frameData)
        {
          resultCollectedData->addValues(frameData);
          wasnotinside = false;
          break;
        }
      }

      // second: the data-label was not inside, so we create and fill with data
      if(wasnotinside)
      {
        collectedData* resultCollectedData = new collectedData;
        resultCollectedData->mLabel = frameData.mLabel;
        resultCollectedData->mStatDataType =frameData.mStatDataType;
        resultCollectedData->addValues(frameData);
        preResult->append(resultCollectedData);
      }
    }
  }

  // at this point we have a tree-structure, each label has a list with all values, but the values are not summed up
  // and we have to

  // we create the data for the result
  QList<collectedData>* result = new QList<collectedData>();

  // running thru all preResult-Elements
  for (int i = 0; i < preResult->count(); i++)
  {
    // creating a list for all Data
    QList<QPair<QVariant, int>*>* tmpDataList = new QList<QPair<QVariant, int>*>();

    //get the data from preResult at an index
    collectedData* preData = preResult->at(i);

    // now we go thru all possible data-elements
    for (int j = 0; j < preData->mValues.count(); j++)
    {
      // getting the real-data (value and amount)
      QPair<QVariant, int>* preDataValuePair = preData->mValues.at(j);

      //define a auxillary-variable
      bool wasnotinside = true;

      // run thru all data, we have already in our list
      for (int k = 0; k < tmpDataList->count(); k++)
      {
        // getting data from our list
        QPair<QVariant, int>* resultData = tmpDataList->at(k);

        // and compare each value for the result with the given value
        if(resultData->first == preDataValuePair->first)
        {
          // if we found an equal pair of value, we have to sum up the amount
          resultData->second += preDataValuePair->second;
          wasnotinside = false;   // take care, that we change our bool
          break; // we can leave the loop, because every value is just one time in our list
        }
      }

      // we have the data not inside our list
      if(wasnotinside)
      {
        // we create a copy and insert it to the list
        QPair<QVariant, int>* dptcnt = new QPair<QVariant, int>();
        dptcnt->first = preDataValuePair->first;
        dptcnt->second = preDataValuePair->second;
        tmpDataList->append(dptcnt);
      }
    }

    //define the new data for the result
    collectedData data;
    data.mLabel = preData->mLabel;
    data.mStatDataType  = preData->mStatDataType;
    data.addValueList(tmpDataList);

    // at least append the new collected Data to our result-list
    result->append(data);
  }

  // we don't need the temporary created preResult anymore (remember: get memory, free memory)
  preResult->clear();
  delete preResult;

  // finally return our result
  return result;
}

bool playlistItemStatisticsFile::isDataAvaible()
{
  return backgroundParserFuture.isFinished();
}
