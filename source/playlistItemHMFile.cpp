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

#include "playlistItemHMFile.h"

#include <QUrl>
#include "signalsSlots.h"

#define HM_DEBUG_OUTPUT 1
#if HM_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_HM qDebug
#else
#define DEBUG_HM(fmt,...) ((void)0)
#endif

playlistItemHMFile::playlistItemHMFile(const QString &hevcFilePath, int displayComponent)
  : playlistItemWithVideo(hevcFilePath, playlistItem_Indexed)
{
  // Set the properties of the playlistItem
  setIcon(0, convertIcon(":img_videoHEVC.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // Set the video pointer correctly
  video.reset(new videoHandlerYUV());
  videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());

  // Connect the basic signals from the video
  playlistItemWithVideo::connectVideo();

  // Nothing is currently being loaded
  isFrameLoading = false;
  isFrameLoadingDoubleBuffer = false;

  // An HEVC file can be cached if nothing goes wrong
  cachingEnabled = true;

  // Set which signal to show
  displaySignal = displayComponent;
  if (displaySignal < 0 || displaySignal > 3)
    displaySignal = 0;

  // Allocate the decoders
  loadingDecoder.reset(new hmDecoder(displaySignal));
  cachingDecoder.reset(new hmDecoder(displaySignal, true));

  // Reset display signal if this is not supported by the decoder
  if (!loadingDecoder->wrapperPredResiSupported())
    displaySignal = 0;
  yuvVideo->showPixelValuesAsDiff = (displaySignal == 2 || displaySignal == 3);

  // Open the input file.
  // TODO: This will parse the whole HEVC file twice, saving all NAL entry points twice.
  // Maybe this should be somehow avoided. Maybe by one instance that saves all the information from the NAL stream and multiple reader classes in the decoders.
  if (!loadingDecoder->openFile(hevcFilePath))
  {
    // Something went wrong. Let's find out what.
    if (loadingDecoder->errorInDecoder())
      fileState = hmFileOnlyParsing;
    if (loadingDecoder->errorParsingBitstream())
      fileState = hmFileError;

    // In any case, decoding of images is not possible.
    cachingEnabled = false;
    return;
  }

  // The bitstream looks valid and the decoder is operational.
  fileState = hmFileNoError;

  if (!cachingDecoder->openFile(hevcFilePath, loadingDecoder.data()))
  {
    // Loading the normal decoder worked, but loading another decoder for caching failed.
    // That is strange.
    cachingEnabled = false;
  }

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
  connect(yuvVideo, &videoHandlerYUV::signalRequestRawData, this, &playlistItemHMFile::loadYUVData, Qt::DirectConnection);
  connect(yuvVideo, &videoHandlerYUV::signalUpdateFrameLimits, this, &playlistItemHMFile::slotUpdateFrameLimits);
  connect(&statSource, &statisticHandler::updateItem, this, &playlistItemHMFile::updateStatSource);
  connect(&statSource, &statisticHandler::requestStatisticsLoading, this, &playlistItemHMFile::loadStatisticToCache);
}

void playlistItemHMFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the HEVC file. We save both in the playlist.
  QUrl fileURL(plItemNameOrFileName);
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(plItemNameOrFileName);

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemHMFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the HEVC file (the path to the file. Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);
  d.appendProperiteChild("displayComponent", QString::number(displaySignal));

  root.appendChild(d);
}

playlistItemHMFile *playlistItemHMFile::newplaylistItemHMFile(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemHMFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  int displaySignal = root.findChildValue("displayComponent").toInt();

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemHMFile *newFile = new playlistItemHMFile(filePath, displaySignal);

  // Load the propertied of the playlistItemIndexed
  playlistItem::loadPropertiesFromPlaylist(root, newFile);

  return newFile;
}

infoData playlistItemHMFile::getInfo() const
{
  infoData info("HEVC File Info");

  // At first append the file information part (path, date created, file size...)
  info.items.append(loadingDecoder->getFileInfoList());

  if (fileState != hmFileNoError)
    info.items.append(infoItem("Error", loadingDecoder->decoderErrorString()));
  if (fileState == hmFileOnlyParsing)
  {
    info.items.append(infoItem("Num POCs", QString::number(loadingDecoder->getNumberPOCs()), "The number of pictures in the stream."));
    info.items.append(infoItem("NAL units", "Show NAL units", "Show a detailed list of all NAL units.", true));
  }
  else if (fileState == hmFileNoError)
  {
    QSize videoSize = video->getFrameSize();
    info.items.append(infoItem("HM path", loadingDecoder->getLibraryPath(), "The path to the loaded libde265 library"));
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
    info.items.append(infoItem("Num POCs", QString::number(loadingDecoder->getNumberPOCs()), "The number of pictures in the stream."));
    info.items.append(infoItem("Internals", loadingDecoder->wrapperInternalsSupported() ? "Yes" : "No", "Is the decoder able to provide internals (statistics)?"));
    info.items.append(infoItem("Stat Parsing", loadingDecoder->statisticsEnabled() ? "Yes" : "No", "Are the statistics of the sequence currently extracted from the stream?"));
    info.items.append(infoItem("NAL units", "Show NAL units", "Show a detailed list of all NAL units.", true));
  }

  return info;
}

void playlistItemHMFile::infoListButtonPressed(int buttonID)
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

void playlistItemHMFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  if (fileState == hmFileNoError && frameIdx >= 0 && frameIdx < loadingDecoder->getNumberPOCs())
  {
    video->drawFrame(painter, frameIdx, zoomFactor, drawRawData);
    statSource.paintStatistics(painter, frameIdx, zoomFactor);
  }
}

ValuePairListSets playlistItemHMFile::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  newSet.append("YUV", video->getPixelValues(pixelPos, frameIdx));
  if (loadingDecoder->wrapperInternalsSupported() && loadingDecoder->statisticsEnabled())
    newSet.append("Stats", statSource.getValuesAt(pixelPos));

  return newSet;
}

void playlistItemHMFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  allExtensions.append("hm");
  filters.append("Annex B HEVC Bitstream (*.bin)");
}

void playlistItemHMFile::reloadItemSource()
{
  // TODO: The caching decoder must also be reloaded
  //       All items in the cache are also now invalid

  loadingDecoder->reloadItemSource();

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  // Reset the videoHandlerYUV source. With the next draw event, the videoHandlerYUV will request to decode the frame again.
  video->invalidateAllBuffers();

  // Load frame 0. This will decode the first frame in the sequence and set the
  // correct frame size/YUV format.
  loadYUVData(0, false);
}

itemLoadingState playlistItemHMFile::needsLoading(int frameIdx, bool loadRawData)
{
  auto videoState = video->needsLoading(frameIdx, loadRawData);
  if (videoState == LoadingNeeded || statSource.needsLoading(frameIdx) == LoadingNeeded)
    return LoadingNeeded;
  return videoState;
}

void playlistItemHMFile::loadFrame(int frameIdx, bool playing, bool loadRawdata)
{
  auto stateYUV = video->needsLoading(frameIdx, loadRawdata);
  auto stateStat = statSource.needsLoading(frameIdx);

  if (stateYUV == LoadingNeeded || stateStat == LoadingNeeded)
  {
    isFrameLoading = true;
    if (stateYUV == LoadingNeeded)
    {
      // Load the requested current frame
      DEBUG_HM("playlistItemRawFile::loadFrame loading frame %d %s", frameIdx, playing ? "(playing)" : "");
      video->loadFrame(frameIdx);
    }
    if (stateStat == LoadingNeeded)
    {
      DEBUG_HM("playlistItemRawFile::loadFrame loading statistics %d %s", frameIdx, playing ? "(playing)" : "");
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
      DEBUG_HM("playlistItemRawFile::loadFrame loading frame into double buffer %d %s", nextFrameIdx, playing ? "(playing)" : "");
      isFrameLoadingDoubleBuffer = true;
      video->loadFrame(nextFrameIdx, true);
      isFrameLoadingDoubleBuffer = false;
      emit signalItemDoubleBufferLoaded();
    }
  }
}

void playlistItemHMFile::cacheFrame(int idx, bool testMode)
{
  if (!cachingEnabled)
    return;

  // Cache a certain frame. This is always called in a separate thread.
  cachingMutex.lock();
  video->cacheFrame(idx, testMode);
  cachingMutex.unlock();
}

void playlistItemHMFile::loadYUVData(int frameIdx, bool caching)
{
  if (caching && !cachingEnabled)
    return;

  if (!caching && fileState != hmFileNoError)
    // We can not decode images
    return;

  DEBUG_HM("playlistItemHEVCFile::loadYUVData %d %s", frameIdx, caching ? "caching" : "");

  videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
  yuvVideo->setFrameSize(loadingDecoder->getFrameSize());
  yuvVideo->setYUVPixelFormat(loadingDecoder->getYUVPixelFormat());
  statSource.statFrameSize = loadingDecoder->getFrameSize();

  if (frameIdx > startEndFrame.second || frameIdx < 0)
  {
    DEBUG_HM("playlistItemHEVCFile::loadYUVData Invalid frame index");
    return;
  }

  // Just get the frame from the correct decoder
  QByteArray decByteArray;
  if (caching)
    decByteArray = cachingDecoder->loadYUVFrameData(frameIdx);
  else
    decByteArray = loadingDecoder->loadYUVFrameData(frameIdx);

  if (!decByteArray.isEmpty())
  {
    yuvVideo->rawYUVData = decByteArray;
    yuvVideo->rawYUVData_frameIdx = frameIdx;
  }
}

void playlistItemHMFile::loadStatisticToCache(int frameIdx, int typeIdx)
{
  Q_UNUSED(typeIdx);
  DEBUG_HM("playlistItemHEVCFile::loadStatisticToCache Request statistics type %d for frame %d", typeIdx, frameIdx);

  if (!loadingDecoder->wrapperInternalsSupported())
    return;

  statSource.statsCache[typeIdx] = loadingDecoder->getStatisticsData(frameIdx, typeIdx);
}

void playlistItemHMFile::createPropertiesWidget()
{
  // Absolutely always only call this once
  Q_ASSERT_X(!propertiesWidget, "playlistItemHEVCFile::createPropertiesWidget", "Always create the properties only once!");

  // Create a new widget and populate it with controls
  propertiesWidget.reset(new QWidget);
  ui.setupUi(propertiesWidget.data());

  QFrame *lineOne = new QFrame;
  lineOne->setObjectName(QStringLiteral("line"));
  lineOne->setFrameShape(QFrame::HLine);
  lineOne->setFrameShadow(QFrame::Sunken);
  QFrame *lineTwo = new QFrame;
  lineTwo->setObjectName(QStringLiteral("line"));
  lineTwo->setFrameShape(QFrame::HLine);
  lineTwo->setFrameShadow(QFrame::Sunken);

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
  ui.verticalLayout->insertLayout(0, createPlaylistItemControls());
  ui.verticalLayout->insertWidget(1, lineOne);
  ui.verticalLayout->insertLayout(2, yuvVideo->createYUVVideoHandlerControls(true));
  ui.verticalLayout->insertWidget(5, lineTwo);
  ui.verticalLayout->insertLayout(6, statSource.createStatisticsHandlerControls(), 1);

  // Set the components that we can display
  ui.comboBoxDisplaySignal->addItem("Reconstruction");
  if (loadingDecoder->wrapperPredResiSupported())
    ui.comboBoxDisplaySignal->addItems(QStringList() << "Prediction" << "Residual" << "Transform Coefficients");
  ui.comboBoxDisplaySignal->setCurrentIndex(displaySignal);

  // Connect signals/slots
  connect(ui.comboBoxDisplaySignal, QComboBox_currentIndexChanged_int, this, &playlistItemHMFile::displaySignalComboBoxChanged);
}

void playlistItemHMFile::fillStatisticList()
{
  if (!loadingDecoder->wrapperInternalsSupported())
    return;
}

void playlistItemHMFile::displaySignalComboBoxChanged(int idx)
{
  if (displaySignal != idx)
  {
    displaySignal = idx;
    loadingDecoder->setDecodeSignal(idx);
    cachingDecoder->setDecodeSignal(idx);

    // A different display signal was chosen. Invalidate the cache and signal that we will need a redraw.
    videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
    yuvVideo->showPixelValuesAsDiff = (idx == 2 || idx == 3);
    yuvVideo->invalidateAllBuffers();
    emit signalItemChanged(true, true);
  }
}