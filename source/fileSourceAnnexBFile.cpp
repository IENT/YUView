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

#include "fileSourceAnnexBFile.h"
#include "mainwindow.h"

#define ANNEXBFILE_DEBUG_OUTPUT 0
#if ANNEXBFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_ANNEXB qDebug
#else
#define DEBUG_ANNEXB(fmt,...) ((void)0)
#endif

fileSourceAnnexBFile::fileSourceAnnexBFile()
{
  fileBuffer.resize(BUFFER_SIZE);
  posInBuffer = 0;
  bufferStartPosInFile = 0;
  fileBufferSize = 0;
  //numZeroBytes = 0;
  
  // Set the start code to look for (0x00 0x00 0x01)
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)1);
}

fileSourceAnnexBFile::~fileSourceAnnexBFile()
{
}

// Open the file and fill the read buffer. 
bool fileSourceAnnexBFile::openFile(const QString &fileName)
{
  DEBUG_ANNEXB("fileSourceAnnexBFile::openFile fileName %s %s", fileName, saveAllUnits ? "saveAllUnits" : "");

  // Open the input file (again)
  fileSource::openFile(fileName);

  // Fill the buffer
  fileBufferSize = srcFile.read(fileBuffer.data(), BUFFER_SIZE);
  if (fileBufferSize == 0)
    // The file is empty of there was an error reading from the file.
    return false;

  // Discard all bytes until we find a start code
  quint64 pos;
  getNextNALUnit(pos);
}

QByteArray fileSourceAnnexBFile::getNextNALUnit(quint64 &posInFile)
{
  QByteArray retArray;

  int nextStartCodePos = -1;
  while (nextStartCodePos == -1)
  {
    nextStartCodePos = fileBuffer.indexOf(startCode, posInBuffer);

    if (nextStartCodePos == -1)
    {
      // No start code found ... append all data in the current buffer.
      retArray += fileBuffer.right(posInBuffer);

      if (fileBufferSize < BUFFER_SIZE)
      {
        // We are out of file and could not find a next position
        posInFile = bufferStartPosInFile + posInBuffer;
        posInBuffer = BUFFER_SIZE;
        return retArray;
      }

      // Before we load the next bytes: The start code might be located at the boundary to the next buffer
      bool lastByteMatch = fileBuffer.endsWith( startCode.left(1) );
      bool lastTwoByteMatch = fileBuffer.endsWith( startCode.left(2) );

      // We have to continue searching
      updateBuffer();

      // Now look for the special boundary case:
      if (lastByteMatch && fileBufferSize > 2 && fileBuffer.startsWith( startCode.right(2) ))
        nextStartCodePos = 2;
      if (lastTwoByteMatch && fileBufferSize > 2 && fileBuffer.startsWith( startCode.right(1) ))
        nextStartCodePos = 1;
    }
  }

  // Position found
  retArray += fileBuffer.mid(posInBuffer, nextStartCodePos - posInBuffer);
  posInBuffer = nextStartCodePos + 3; // Skip the start code
  posInFile = bufferStartPosInFile + posInBuffer;
  return QByteArray();
}

bool fileSourceAnnexBFile::updateBuffer()
{
  // Save the position of the first byte in this new buffer
  bufferStartPosInFile += fileBufferSize;

  fileBufferSize = srcFile.read(fileBuffer.data(), BUFFER_SIZE);
  posInBuffer = 0;

  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::updateBuffer fileBufferSize %d", fileBufferSize);
  return (fileBufferSize > 0);
}











//bool fileSourceAnnexBFile::seekToNextNALUnit()
//{
//  // Are we currently at the one byte of a start code?
//  if (curPosAtStartCode())
//    return gotoNextByte();
//
//  // Is there anything to read left?
//  if (fileBufferSize == 0)
//    return false;
//
//  numZeroBytes = 0;
//  
//  // Check if there is another start code in the buffer
//  int idx = fileBuffer.indexOf(startCode, posInBuffer);
//  while (idx < 0) 
//  {
//    // Start code not found in this buffer. Load next chuck of data from file.
//
//    // Before we load more data, check with how many zeroes the current buffer ends.
//    // This could be the beginning of a start code.
//    int nrZeros = 0;
//    for (int i = 1; i <= 3; i++) 
//    {
//      if (fileBuffer.at(fileBufferSize-i) == 0)
//        nrZeros++;
//    }
//    
//    // Load the next buffer
//    if (!updateBuffer()) 
//    {
//      // Out of file
//      return false;
//    }
//
//    if (nrZeros > 0)
//    {
//      // The last buffer ended with zeroes. 
//      // Now check if the beginning of this buffer is the remaining part of a start code
//      if ((nrZeros == 2 || nrZeros == 3) && fileBuffer.at(0) == 1) 
//      {
//        // Start code found
//        posInBuffer = 1;
//        return true;
//      }
//
//      if ((nrZeros == 1) && fileBuffer.at(0) == 0 && fileBuffer.at(1) == 1) 
//      {
//        // Start code found
//        posInBuffer = 2;
//        return true;
//      }
//    }
//
//    // New buffer loaded but no start code found yet. Search for it again.
//    idx = fileBuffer.indexOf(startCode, posInBuffer);
//  }
//
//  assert(idx >= 0);
//  if (quint64(idx + 3) >= fileBufferSize) 
//  {
//    // The start code is exactly at the end of the current buffer. 
//    if (!updateBuffer()) 
//      // Out of file
//      return false;
//    return true;
//  }
//
//  // Update buffer position
//  posInBuffer = idx + 3;
//  return true;
//}
//
//bool fileSourceAnnexBFile::gotoNextByte()
//{
//  // First check if the current byte is a zero byte
//  if (getCurByte() == (char)0)
//    numZeroBytes++;
//  else
//    numZeroBytes = 0;
//
//  posInBuffer++;
//
//  if (posInBuffer >= fileBufferSize) 
//  {
//    // The next byte is in the next buffer
//    if (!updateBuffer())
//    {
//      // Out of file
//      return false;
//    }
//    posInBuffer = 0;
//  }
//
//  return true;
//}
//
//QByteArray fileSourceAnnexBFile::getRemainingNALBytes(int maxBytes)
//{
//  QByteArray retArray;
//  int nrBytesRead = 0;
//
//  while (!curPosAtStartCode() && (maxBytes == -1 || nrBytesRead < maxBytes)) 
//  {
//    // Save byte and goto the next one
//    retArray.append(getCurByte());
//
//    if (!gotoNextByte())
//      // No more bytes. Return all we got.
//      return retArray;
//
//    nrBytesRead++;
//  }
//
//  // We should now be at a header byte. Remove the zeroes from the start code that we put into retArray
//  while (retArray.endsWith(char(0))) 
//    // Chop off last zero byte
//    retArray.chop(1);
//
//  return retArray;
//}
//
//bool fileSourceAnnexBFile::seekToFilePos(quint64 pos)
//{
//  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::seekToFilePos %d", pos);
//
//  if (!srcFile.seek(pos))
//    return false;
//
//  bufferStartPosInFile = pos;
//  numZeroBytes = 0;
//  fileBufferSize = 0;
//
//  return updateBuffer();
//}
//
//QByteArray fileSourceAnnexBFile::getRemainingBuffer_Update()
//{
//  QByteArray retArr = fileBuffer.mid(posInBuffer, fileBufferSize-posInBuffer); 
//  updateBuffer();
//  return retArr;
//}
//
//QByteArray fileSourceAnnexBFile::getNextNALUnit()
//{
//  if (seekToNextNALUnit())
//    return getRemainingNALBytes();
//  return QByteArray();
//}
//
//bool fileSourceAnnexBFile::addPOCToList(int poc)
//{
//  if (poc < 0)
//    return false;
//
//  if (POC_List.contains(poc))
//    // Two pictures with the same POC are not allowed
//    return false;
//  
//  POC_List.append(poc);
//  return true;
//}
//
//bool fileSourceAnnexBFile::scanFileForNalUnits(bool saveAllUnits)
//{
//  DEBUG_ANNEXB("fileSourceAnnexBFile::scanFileForNalUnits %s", saveAllUnits ? "saveAllUnits" : "");
//
//  // Show a modal QProgressDialog while this operation is running.
//  // If the user presses cancel, we will cancel and return false (opening the file failed).
//  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
//  QWidgetList l = QApplication::topLevelWidgets();
//  QWidget *mainWindow = nullptr;
//  for (QWidget *w : l)
//  {
//    MainWindow *mw = dynamic_cast<MainWindow*>(w);
//    if (mw)
//      mainWindow = mw;
//  }
//  // Create the dialog
//  qint64 maxPos = getFileSize();
//  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
//  int curPercentValue = 0;
//  QProgressDialog progress("Parsing AnnexB bitstream...", "Cancel", 0, 100, mainWindow);
//  progress.setMinimumDuration(1000);  // Show after 1s
//  progress.setAutoClose(false);
//  progress.setAutoReset(false);
//  progress.setWindowModality(Qt::WindowModal);
//
//  // Count the NALs
//  int nalID = 0;
//
//  if (saveAllUnits && nalUnitModel.rootItem.isNull())
//    // Create a new root for the nal unit tree of the QAbstractItemModel
//    nalUnitModel.rootItem.reset(new TreeItem(QStringList() << "Name" << "Value" << "Coding" << "Code" << "Meaning", nullptr));
//
//  while (seekToNextNALUnit()) 
//  {
//    try
//    {
//      // Seek successfull. The file is now pointing to the first byte after the start code.
//      parseAndAddNALUnit(nalID);
//      
//      // Update the progress dialog
//      if (progress.wasCanceled())
//      {
//        clearData();
//        return false;
//      }
//      int newPercentValue = pos() * 100 / maxPos;
//      if (newPercentValue != curPercentValue)
//      {
//        progress.setValue(newPercentValue);
//        curPercentValue = newPercentValue;
//      }
//
//      // Next NAL
//      nalID++;
//    }
//    catch (...)
//    {
//      // Reading a NAL unit failed at some point.
//      // This is not too bad. Just don't use this NAL unit and continue with the next one.
//      DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::scanFileForNalUnits Exception thrown parsing NAL %d", nalID);
//      nalID++;
//    }
//  }
//
//  // We are done.
//  progress.close();
//
//  // Sort the POC list
//  std::sort(POC_List.begin(), POC_List.end());
//    
//  return true;
//}
//
//// Look through the random access points and find the closest one before (or equal)
//// the given frameIdx where we can start decoding
//int fileSourceAnnexBFile::getClosestSeekableFrameNumber(int frameIdx) const
//{
//  // Get the POC for the frame number
//  int iPOC = POC_List[frameIdx];
//
//  // We schould always be able to seek to the beginning of the file
//  int bestSeekPOC = POC_List[0];
//
//  for (auto nal : nalUnitList)
//  {
//    if (!nal->isParameterSet() && nal->getPOC() >= 0) 
//    {
//      if (nal->getPOC() <= iPOC)
//        // We could seek here
//        bestSeekPOC = nal->getPOC();
//      else
//        break;
//    }
//  }
//
//  // Get the frame index for the given POC
//  return POC_List.indexOf(bestSeekPOC);
//}
//
//QList<QByteArray> fileSourceAnnexBFile::seekToFrameNumber(int iFrameNr)
//{
//  // Get the POC for the frame number
//  int iPOC = POC_List[iFrameNr];
//
//  // A list of all parameter sets up to the random access point with the given frame number.
//  QList<QByteArray> paramSets;
//  
//  for (auto nal : nalUnitList)
//  {
//    if (nal->isParameterSet())
//    {
//      // Append the raw parameter set to the return array
//      paramSets.append(nal->getRawNALData());
//    }
//    else if (nal->getPOC() >= 0)
//    {
//      if (nal->getPOC() == iPOC) 
//      {
//        // Seek here
//        seekToFilePos(nal->filePos);
//
//        // Return the parameter sets that we encountered so far.
//        return paramSets;
//      }
//    }
//  }
//
//  return QList<QByteArray>();
//}
//
//void fileSourceAnnexBFile::clearData()
//{
//  nalUnitList.clear();
//  POC_List.clear();
//
//  // Reset all internal values
//  fileBuffer.clear();
//  fileBuffer.resize(BUFFER_SIZE);
//  fileBufferSize = -1;
//  posInBuffer = 0;
//  bufferStartPosInFile = 0;
//  numZeroBytes = 0;
//}
//
//QVariant fileSourceAnnexBFile::NALUnitModel::headerData(int section, Qt::Orientation orientation, int role) const
//{
//  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && rootItem != nullptr)
//    return rootItem->itemData.value(section, QString());
//
//  return QVariant();
//}
//
//QVariant fileSourceAnnexBFile::NALUnitModel::data(const QModelIndex &index, int role) const
//{
//  //qDebug() << "ileSourceHEVCAnnexBFile::data " << index;
//
//  if (!index.isValid())
//    return QVariant();
//
//  if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
//    return QVariant();
//
//  TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
//
//  return QVariant(item->itemData.value(index.column()));
//}
//
//QModelIndex fileSourceAnnexBFile::NALUnitModel::index(int row, int column, const QModelIndex &parent) const
//{
//  //qDebug() << "ileSourceHEVCAnnexBFile::index " << row << column << parent;
//
//  if (!hasIndex(row, column, parent))
//    return QModelIndex();
//
//  TreeItem *parentItem;
//
//  if (!parent.isValid())
//    parentItem = rootItem.data();
//  else
//    parentItem = static_cast<TreeItem*>(parent.internalPointer());
//
//  if (parentItem == nullptr)
//    return QModelIndex();
//
//  TreeItem *childItem = parentItem->childItems.value(row, nullptr);
//  if (childItem)
//    return createIndex(row, column, childItem);
//  else
//    return QModelIndex();
//}
//
//QModelIndex fileSourceAnnexBFile::NALUnitModel::parent(const QModelIndex &index) const
//{
//  //qDebug() << "ileSourceHEVCAnnexBFile::parent " << index;
//
//  if (!index.isValid())
//    return QModelIndex();
//
//  TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
//  TreeItem *parentItem = childItem->parentItem;
//
//  if (parentItem == rootItem.data())
//    return QModelIndex();
//
//  // Get the row of the item in the list of children of the parent item
//  int row = 0;
//  if (parentItem)
//    row = parentItem->parentItem->childItems.indexOf(const_cast<TreeItem*>(parentItem));
//
//  return createIndex(row, 0, parentItem);
//
//}
//
//int fileSourceAnnexBFile::NALUnitModel::rowCount(const QModelIndex &parent) const
//{
//  //qDebug() << "ileSourceHEVCAnnexBFile::rowCount " << parent;
//
//  TreeItem *parentItem;
//  if (parent.column() > 0)
//    return 0;
//
//  if (!parent.isValid())
//    parentItem = rootItem.data();
//  else
//    parentItem = static_cast<TreeItem*>(parent.internalPointer());
//
//  return (parentItem == nullptr) ? 0 : parentItem->childItems.count();
//}
//
//
