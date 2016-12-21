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

#include "playlistItemHEVCFile.h"

#include <QDebug>
#include <QUrl>
#include <QPainter>
#include <QtConcurrent>

#define HEVC_DEBUG_OUTPUT 1
#if HEVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_HEVC qDebug
#else
#define DEBUG_HEVC(fmt,...) ((void)0)
#endif

playlistItemHEVCFile::playlistItemHEVCFile(const QString &hevcFilePath)
  : playlistItem(hevcFilePath, playlistItem_Indexed)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_videoHEVC.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // Open the input file.
  // TODO: This will parse the whole HEVC file twice, saving all NAL entry points twice.
  // Maybe this should be somehow avoided. Maybe by one instance that saves all the information from the NAL stream and multiple reader classes in the decoders.
  if (!loadingDecoder.openFile(hevcFilePath) || !cachingDecoder.openFile(hevcFilePath))
    return;
  
  // Fill the list of statistics that we can provide
  fillStatisticList();

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  if (startEndFrame.second == -1)
    // No frames to decode
    return;

  // Load frame 0. This will decode the first frame in the sequence and set the
  // correct frame size/YUV format.
  loadYUVData(0, false);

  // If the yuvVideHandler requests raw YUV data, we provide it from the file
  connect(&yuvVideo, &videoHandlerYUV::signalRequestRawData, this, &playlistItemHEVCFile::loadYUVData, Qt::DirectConnection);
  connect(&yuvVideo, &videoHandlerYUV::signalHandlerChanged, this, &playlistItemHEVCFile::signalItemChanged);
  connect(&yuvVideo, &videoHandlerYUV::signalUpdateFrameLimits, this, &playlistItemHEVCFile::slotUpdateFrameLimits);
  connect(&statSource, &statisticHandler::updateItem, this, &playlistItemHEVCFile::updateStatSource);
  connect(&statSource, &statisticHandler::requestStatisticsLoading, this, &playlistItemHEVCFile::loadStatisticToCache);

  // An HEVC file can be cached
  cachingEnabled = true;
}

void playlistItemHEVCFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the HEVC file. We save both in the playlist.
  QUrl fileURL(plItemNameOrFileName);
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(plItemNameOrFileName);

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemHEVCFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the HEVC file (the path to the file. Relative and absolute)
  d.appendProperiteChild( "absolutePath", fileURL.toString() );
  d.appendProperiteChild( "relativePath", relativePath  );

  root.appendChild(d);
}

playlistItemHEVCFile *playlistItemHEVCFile::newplaylistItemHEVCFile(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemHEVCFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemHEVCFile *newFile = new playlistItemHEVCFile(filePath);

  // Load the propertied of the playlistItemIndexed
  playlistItem::loadPropertiesFromPlaylist(root, newFile);

  return newFile;
}

infoData playlistItemHEVCFile::getInfo() const
{
  infoData info("HEVC File Info");

  // At first append the file information part (path, date created, file size...)
  info.items.append(loadingDecoder.getFileInfoList());

  if (loadingDecoder.decoderError())
  {
    info.items.append(infoItem("Error", loadingDecoder.decoderErrorString()));
  }
  else
  {
    QSize videoSize = yuvVideo.getFrameSize();
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
    info.items.append(infoItem("Num POCs", QString::number(loadingDecoder.getNumberPOCs()), "The number of pictures in the stream."));
    info.items.append(infoItem("Frames Cached",QString::number(yuvVideo.getNrFramesCached())));
    info.items.append(infoItem("Internals", loadingDecoder.wrapperInternalsSupported() ? "Yes" : "No", "Is the decoder able to provide internals (statistics)?"));
    info.items.append(infoItem("Stat Parsing", loadingDecoder.statisticsEnabled() ? "Yes" : "No", "Are the statistics of the sequence currently extracted from the stream?"));
    info.items.append(infoItem("NAL units", "Show NAL units", "Show a detailed list of all NAL units.", true));
  }

  return info;
}

void playlistItemHEVCFile::infoListButtonPressed(int buttonID)
{
  Q_UNUSED(buttonID);

  // Parse the annex B file again and save all the values read
  fileSourceHEVCAnnexBFile file;
  if (!file.openFile(plItemNameOrFileName, true))
    // Opening the file failed.
    return;

  // The button "Show NAL units" was pressed. Create a dialog with a QTreeView and show the NAL unit list.
  QDialog newDialog;
  QTreeView *view = new QTreeView();
  view->setModel(file.getNALUnitModel());
  QVBoxLayout *verticalLayout = new QVBoxLayout(&newDialog);
  verticalLayout->addWidget(view);
  newDialog.resize(QSize(700, 700));
  view->setColumnWidth(0, 400);
  view->setColumnWidth(1, 50);
  newDialog.exec();
}

itemLoadingState playlistItemHEVCFile::needsLoading(int frameIdx)
{
  if (yuvVideo.needsLoading(frameIdx) == LoadingNeeded || statSource.needsLoading(frameIdx) == LoadingNeeded)
    return LoadingNeeded;
  return yuvVideo.needsLoading(frameIdx);
}

void playlistItemHEVCFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor)
{
  if (frameIdx != -1)
  {
    yuvVideo.drawFrame(painter, frameIdx, zoomFactor);
    statSource.paintStatistics(painter, frameIdx, zoomFactor);
  }
}

void playlistItemHEVCFile::loadYUVData(int frameIdx, bool caching)
{
  DEBUG_HEVC("playlistItemHEVCFile::loadYUVData %d %s", frameIdx, caching ? "caching" : "");

  if ((caching && cachingDecoder.decoderError()) || (!caching && loadingDecoder.decoderError()))
  {
    DEBUG_HEVC("playlistItemHEVCFile::loadYUVData decoder Error");
    return;
  }

  yuvVideo.setFrameSize(loadingDecoder.getFrameSize(), false);
  statSource.statFrameSize = loadingDecoder.getFrameSize();

  if (frameIdx > startEndFrame.second || frameIdx < 0)
  {
    DEBUG_HEVC("playlistItemHEVCFile::loadYUVData Invalid frame index");
    return;
  }

  // Just get the frame from the correct decoder
  QByteArray decByteArray;
  if (caching)
    decByteArray = cachingDecoder.loadYUVFrameData(frameIdx);
  else
    decByteArray = loadingDecoder.loadYUVFrameData(frameIdx);

  if (!decByteArray.isEmpty())
  {
    yuvVideo.rawYUVData = decByteArray;
    yuvVideo.rawYUVData_frameIdx = frameIdx;
  }
}

void playlistItemHEVCFile::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert(!propertiesWidget);

  preparePropertiesWidget(QStringLiteral("playlistItemHEVCFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *lineOne = new QFrame;
  lineOne->setObjectName(QStringLiteral("line"));
  lineOne->setFrameShape(QFrame::HLine);
  lineOne->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first index controllers (start/end...) then YUV controls (format,...)
  vAllLaout->addLayout(createPlaylistItemControls());
  vAllLaout->addWidget(lineOne);
  vAllLaout->addLayout(yuvVideo.createYUVVideoHandlerControls(true));

  if (loadingDecoder.wrapperInternalsSupported())
  {
    QFrame *line2 = new QFrame;
    line2->setObjectName(QStringLiteral("line"));
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);

    vAllLaout->addWidget(line2);
    vAllLaout->addLayout(statSource.createStatisticsHandlerControls(), 1);
  }
  else
  {
    // Insert a stretch at the bottom of the vertical global layout so that everything
    // gets 'pushed' to the top.
    vAllLaout->insertStretch(5, 1);
  }
}

void playlistItemHEVCFile::fillStatisticList()
{
  if (!loadingDecoder.wrapperInternalsSupported())
    return;

  StatisticsType sliceIdx(0, "Slice Index", 0, QColor(0, 0, 0), 10, QColor(255,0,0));
  statSource.addStatType(sliceIdx);

  StatisticsType partSize(1, "Part Size", "jet", 0, 7);
  partSize.valMap.insert(0, "PART_2Nx2N");
  partSize.valMap.insert(1, "PART_2NxN");
  partSize.valMap.insert(2, "PART_Nx2N");
  partSize.valMap.insert(3, "PART_NxN");
  partSize.valMap.insert(4, "PART_2NxnU");
  partSize.valMap.insert(5, "PART_2NxnD");
  partSize.valMap.insert(6, "PART_nLx2N");
  partSize.valMap.insert(7, "PART_nRx2N");
  statSource.addStatType(partSize);

  StatisticsType predMode(2, "Pred Mode", "jet", 0, 2);
  predMode.valMap.insert(0, "INTRA");
  predMode.valMap.insert(1, "INTER");
  predMode.valMap.insert(2, "SKIP");
  statSource.addStatType(predMode);

  StatisticsType pcmFlag(3, "PCM flag", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(pcmFlag);

  StatisticsType transQuantBypass(4, "Transquant Bypass Flag", 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.addStatType(transQuantBypass);

  StatisticsType refIdx0(5, "Ref POC 0", "col3_bblg", -16, 16);
  statSource.addStatType(refIdx0);

  StatisticsType refIdx1(6, "Ref POC 1", "col3_bblg", -16, 16);
  statSource.addStatType(refIdx1);

  StatisticsType motionVec0(7, "Motion Vector 0", 4);
  statSource.addStatType(motionVec0);

  StatisticsType motionVec1(8, "Motion Vector 1", 4);
  statSource.addStatType(motionVec1);

  StatisticsType intraDirY(9, "Intra Dir Luma", "jet", 0, 34);
  intraDirY.hasVectorData = true;
  intraDirY.renderVectorData = true;
  intraDirY.valMap.insert(0, "INTRA_PLANAR");
  intraDirY.valMap.insert(1, "INTRA_DC");
  intraDirY.valMap.insert(2, "INTRA_ANGULAR_2");
  intraDirY.valMap.insert(3, "INTRA_ANGULAR_3");
  intraDirY.valMap.insert(4, "INTRA_ANGULAR_4");
  intraDirY.valMap.insert(5, "INTRA_ANGULAR_5");
  intraDirY.valMap.insert(6, "INTRA_ANGULAR_6");
  intraDirY.valMap.insert(7, "INTRA_ANGULAR_7");
  intraDirY.valMap.insert(8, "INTRA_ANGULAR_8");
  intraDirY.valMap.insert(9, "INTRA_ANGULAR_9");
  intraDirY.valMap.insert(10, "INTRA_ANGULAR_10");
  intraDirY.valMap.insert(11, "INTRA_ANGULAR_11");
  intraDirY.valMap.insert(12, "INTRA_ANGULAR_12");
  intraDirY.valMap.insert(13, "INTRA_ANGULAR_13");
  intraDirY.valMap.insert(14, "INTRA_ANGULAR_14");
  intraDirY.valMap.insert(15, "INTRA_ANGULAR_15");
  intraDirY.valMap.insert(16, "INTRA_ANGULAR_16");
  intraDirY.valMap.insert(17, "INTRA_ANGULAR_17");
  intraDirY.valMap.insert(18, "INTRA_ANGULAR_18");
  intraDirY.valMap.insert(19, "INTRA_ANGULAR_19");
  intraDirY.valMap.insert(20, "INTRA_ANGULAR_20");
  intraDirY.valMap.insert(21, "INTRA_ANGULAR_21");
  intraDirY.valMap.insert(22, "INTRA_ANGULAR_22");
  intraDirY.valMap.insert(23, "INTRA_ANGULAR_23");
  intraDirY.valMap.insert(24, "INTRA_ANGULAR_24");
  intraDirY.valMap.insert(25, "INTRA_ANGULAR_25");
  intraDirY.valMap.insert(26, "INTRA_ANGULAR_26");
  intraDirY.valMap.insert(27, "INTRA_ANGULAR_27");
  intraDirY.valMap.insert(28, "INTRA_ANGULAR_28");
  intraDirY.valMap.insert(29, "INTRA_ANGULAR_29");
  intraDirY.valMap.insert(30, "INTRA_ANGULAR_30");
  intraDirY.valMap.insert(31, "INTRA_ANGULAR_31");
  intraDirY.valMap.insert(32, "INTRA_ANGULAR_32");
  intraDirY.valMap.insert(33, "INTRA_ANGULAR_33");
  intraDirY.valMap.insert(34, "INTRA_ANGULAR_34");
  statSource.addStatType(intraDirY);

  StatisticsType intraDirC(10, "Intra Dir Chroma", "jet", 0, 34);
  intraDirC.hasVectorData = true;
  intraDirC.renderVectorData = true;
  intraDirC.valMap.insert(0, "INTRA_PLANAR");
  intraDirC.valMap.insert(1, "INTRA_DC");
  intraDirC.valMap.insert(2, "INTRA_ANGULAR_2");
  intraDirC.valMap.insert(3, "INTRA_ANGULAR_3");
  intraDirC.valMap.insert(4, "INTRA_ANGULAR_4");
  intraDirC.valMap.insert(5, "INTRA_ANGULAR_5");
  intraDirC.valMap.insert(6, "INTRA_ANGULAR_6");
  intraDirC.valMap.insert(7, "INTRA_ANGULAR_7");
  intraDirC.valMap.insert(8, "INTRA_ANGULAR_8");
  intraDirC.valMap.insert(9, "INTRA_ANGULAR_9");
  intraDirC.valMap.insert(10, "INTRA_ANGULAR_10");
  intraDirC.valMap.insert(11, "INTRA_ANGULAR_11");
  intraDirC.valMap.insert(12, "INTRA_ANGULAR_12");
  intraDirC.valMap.insert(13, "INTRA_ANGULAR_13");
  intraDirC.valMap.insert(14, "INTRA_ANGULAR_14");
  intraDirC.valMap.insert(15, "INTRA_ANGULAR_15");
  intraDirC.valMap.insert(16, "INTRA_ANGULAR_16");
  intraDirC.valMap.insert(17, "INTRA_ANGULAR_17");
  intraDirC.valMap.insert(18, "INTRA_ANGULAR_18");
  intraDirC.valMap.insert(19, "INTRA_ANGULAR_19");
  intraDirC.valMap.insert(20, "INTRA_ANGULAR_20");
  intraDirC.valMap.insert(21, "INTRA_ANGULAR_21");
  intraDirC.valMap.insert(22, "INTRA_ANGULAR_22");
  intraDirC.valMap.insert(23, "INTRA_ANGULAR_23");
  intraDirC.valMap.insert(24, "INTRA_ANGULAR_24");
  intraDirC.valMap.insert(25, "INTRA_ANGULAR_25");
  intraDirC.valMap.insert(26, "INTRA_ANGULAR_26");
  intraDirC.valMap.insert(27, "INTRA_ANGULAR_27");
  intraDirC.valMap.insert(28, "INTRA_ANGULAR_28");
  intraDirC.valMap.insert(29, "INTRA_ANGULAR_29");
  intraDirC.valMap.insert(30, "INTRA_ANGULAR_30");
  intraDirC.valMap.insert(31, "INTRA_ANGULAR_31");
  intraDirC.valMap.insert(32, "INTRA_ANGULAR_32");
  intraDirC.valMap.insert(33, "INTRA_ANGULAR_33");
  intraDirC.valMap.insert(34, "INTRA_ANGULAR_34");
  statSource.addStatType(intraDirC);

  StatisticsType transformDepth(11, "Transform Depth", 0, QColor(0, 0, 0), 3, QColor(0,255,0));
  statSource.addStatType(transformDepth);
}

void playlistItemHEVCFile::loadStatisticToCache(int frameIdx, int typeIdx)
{
  Q_UNUSED(typeIdx);
  DEBUG_HEVC("playlistItemHEVCFile::loadStatisticToCache Request statistics type %d for frame %d", typeIdx, frameIdx);

  if (!loadingDecoder.wrapperInternalsSupported())
    return;

  statSource.statsCache[typeIdx] = loadingDecoder.getStatisticsData(frameIdx, typeIdx);
}

ValuePairListSets playlistItemHEVCFile::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  newSet.append("YUV", yuvVideo.getPixelValues(pixelPos, frameIdx));
  if (loadingDecoder.wrapperInternalsSupported() && loadingDecoder.statisticsEnabled())
    newSet.append("Stats", statSource.getValuesAt(pixelPos));

  return newSet;
}

void playlistItemHEVCFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  allExtensions.append("hevc");
  filters.append("Annex B HEVC Bitstream (*.hevc)");
}

void playlistItemHEVCFile::reloadItemSource()
{
  loadingDecoder.reloadItemSource();

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  // Reset the videoHandlerYUV source. With the next draw event, the videoHandlerYUV will request to decode the frame again.
  yuvVideo.invalidateAllBuffers();

  // Load frame 0. This will decode the first frame in the sequence and set the
  // correct frame size/YUV format.
  loadYUVData(0, false);
}

void playlistItemHEVCFile::cacheFrame(int idx)
{
  if (!cachingEnabled)
    return;

  // Cache a certain frame. This is always called in a separate thread.
  cachingMutex.lock();
  yuvVideo.cacheFrame(idx);
  cachingMutex.unlock();
}

void playlistItemHEVCFile::loadFrame(int frameIdx, bool playing)
{
  auto stateYUV = yuvVideo.needsLoading(frameIdx);
  auto stateStat = statSource.needsLoading(frameIdx);

  if (stateYUV == LoadingNeeded || stateStat == LoadingNeeded)
  {
    isFrameLoading = true;
    if (stateYUV == LoadingNeeded)
    {
      // Load the requested current frame
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading frame %d %s", frameIdx, playing ? "(playing)" : "");
      yuvVideo.loadFrame(frameIdx);
    }
    if (stateStat == LoadingNeeded)
    {
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading statistics %d %s", frameIdx, playing ? "(playing)" : "");
      statSource.loadStatistics(frameIdx);
    }
    
    isFrameLoading = false;
    emit signalItemChanged(true, false);
  }

  if (playing && (stateYUV == LoadingNeeded || stateYUV == LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdx + 1;
    if (nextFrameIdx <= startEndFrame.second)
    {
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading frame into double buffer %d %s", nextFrameIdx, playing ? "(playing)" : "");
      yuvVideo.loadFrame(nextFrameIdx, true);
    }
  }
}
