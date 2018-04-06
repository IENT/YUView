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

#include "playlistItemCompressedVideo.h"

#include "parserAnnexBAVC.h"
#include "parserAnnexBHEVC.h"
#include "parserAnnexBJEM.h"
#include "hevcDecoderHM.h"
#include "hevcDecoderLibde265.h"
#include "hevcNextGenDecoderJEM.h"

#include <QThread>
#include "mainwindow.h"

#define HEVC_DEBUG_OUTPUT 0
#if HEVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_HEVC qDebug
#else
#define DEBUG_HEVC(fmt,...) ((void)0)
#endif

// Initialize the static names list of the decoder engines
QStringList playlistItemCompressedVideo::inputFormatNames = QStringList() << "annexBHEVC" << "annexBAVC" << "annexBJEM" << "FFMpeg";
QStringList playlistItemCompressedVideo::decoderEngineNames = QStringList() << "libDe265" << "HM" << "JEM" << "FFMpeg";

playlistItemCompressedVideo::playlistItemCompressedVideo(const QString &compressedFilePath, int displayComponent, inputFormat input, decoderEngine decoder)
  : playlistItemWithVideo(compressedFilePath, playlistItem_Indexed)
{
  // Set the properties of the playlistItem
  // TODO: should this change with the type of video?
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

  // An compressed file can be cached if nothing goes wrong
  cachingEnabled = true;

  // Set which signal to show
  displaySignal = displayComponent;
  if (displaySignal < 0)
    displaySignal = 0;

  // Open the input file.
  inputFormatType = input;
  if (isAnnexBFileSource())
  {
    // Open file
    inputFileAnnexB.reset(new fileSourceAnnexBFile(compressedFilePath));
    // inputFormatType a parser
    if (input == inputAnnexBHEVC)
      inputFileAnnexBParser.reset(new parserAnnexBHEVC());
    else if (inputFormatType == inputAnnexBAVC)
      inputFileAnnexBParser.reset(new parserAnnexBAVC());
    else if (inputFormatType == inputAnnexBJEM)
      inputFileAnnexBParser.reset(new parserAnnexBJEM());
    // Parse the file
    parseAnnexBFile(inputFileAnnexB, inputFileAnnexBParser);
    inputFileAnnexBParser->sortPOCList();

    fileState = onlyParsing;
  }
  else
  {
    // Try ffmpeg to open the file
    inputFileFFMpeg.reset(new fileSourceFFMpegFile());
    if (!inputFileFFMpeg->openFile(compressedFilePath))
    {
      fileState = error;
      return;
    }
    
    fileState = onlyParsing;
  }

  // Allocate the decoders
  decoderEngineType = decoder;
  if (decoder == decoderLibde265)
  {
    loadingDecoder.reset(new hevcDecoderLibde265(displaySignal));
    cachingDecoder.reset(new hevcDecoderLibde265(displaySignal, true));
  }
  else if (decoder == decoderHM)
  {
    loadingDecoder.reset(new hevcDecoderHM(displaySignal));
    cachingDecoder.reset(new hevcDecoderHM(displaySignal, true));
  }
  else if (decoder == decoderJEM)
  {
    loadingDecoder.reset(new hevcNextGenDecoderJEM(displaySignal));
    cachingDecoder.reset(new hevcNextGenDecoderJEM(displaySignal, true));
  }
  else
    return;

  // Reset display signal if this is not supported by the decoder
  if (displaySignal > loadingDecoder->wrapperNrSignalsSupported())
    displaySignal = 0;
  yuvVideo->showPixelValuesAsDiff = (displaySignal == 2 || displaySignal == 3);

  
  //if (!loadingDecoder->openFile(hevcFilePath))
  //{
  //  // Something went wrong. Let's find out what.
  //  if (loadingDecoder->errorInDecoder())
  //    fileState = onlyParsing;
  //  if (loadingDecoder->errorParsingBitstream())
  //    fileState = error;

  //  // In any case, decoding of images is not possible.
  //  cachingEnabled = false;
  //  return;
  //}

  //// The bitstream looks valid and the decoder is operational.
  //fileState = noError;

  //if (cachingDecoder && !cachingDecoder->openFile(hevcFilePath, loadingDecoder.data()))
  //{
  //  // Loading the normal decoder worked, but loading another decoder for caching failed.
  //  // That is strange.
  //  cachingEnabled = false;
  //}

  //// Fill the list of statistics that we can provide
  //fillStatisticList();

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  //if (startEndFrame.second == -1)
  //  // No frames to decode
  //  return;

  //// Load frame 0. This will decode the first frame in the sequence and set the
  //// correct frame size/YUV format.
  //loadYUVData(0, false);

  //// If the yuvVideHandler requests raw YUV data, we provide it from the file
  //connect(yuvVideo, &videoHandlerYUV::signalRequestRawData, this, &playlistItemRawCodedVideo::loadYUVData, Qt::DirectConnection);
  //connect(yuvVideo, &videoHandlerYUV::signalUpdateFrameLimits, this, &playlistItemRawCodedVideo::slotUpdateFrameLimits);
  //connect(&statSource, &statisticHandler::updateItem, this, &playlistItemRawCodedVideo::updateStatSource);
  //connect(&statSource, &statisticHandler::requestStatisticsLoading, this, &playlistItemRawCodedVideo::loadStatisticToCache, Qt::DirectConnection);
}

void playlistItemCompressedVideo::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the HEVC file. We save both in the playlist.
  QUrl fileURL(plItemNameOrFileName);
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(plItemNameOrFileName);

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemRawCodedVideo");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the HEVC file (the path to the file. Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);
  d.appendProperiteChild("displayComponent", QString::number(displaySignal));

  QString readerEngine = inputFormatNames.at(inputFormatType);
  d.appendProperiteChild("inputFormat", readerEngine);
  QString decoderTypeName = decoderEngineNames.at(decoderEngineType);
  d.appendProperiteChild("decoder", decoderTypeName);
  
  root.appendChild(d);
}

playlistItemCompressedVideo *playlistItemCompressedVideo::newPlaylistItemCompressedVideo(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemRawCodedVideo
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  int displaySignal = root.findChildValue("displayComponent").toInt();

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  inputFormat input = inputAnnexBHEVC;
  QString inputName = root.findChildValue("inputFormat");
  int idx = inputFormatNames.indexOf(inputName);
  if (idx >= 0 && idx < input_NUM)
    input = inputFormat(idx);

  decoderEngine decoder = decoderLibde265;
  QString decoderName = root.findChildValue("decoder");
  idx = decoderEngineNames.indexOf(decoderName);
  if (idx >= 0 && idx < decoder_NUM)
    decoder = decoderEngine(idx);

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemCompressedVideo *newFile = new playlistItemCompressedVideo(filePath, displaySignal, input, decoder);

  // Load the propertied of the playlistItemIndexed
  playlistItem::loadPropertiesFromPlaylist(root, newFile);

  return newFile;
}

infoData playlistItemCompressedVideo::getInfo() const
{
  infoData info("HEVC File Info");

  // At first append the file information part (path, date created, file size...)
  // info.items.append(loadingDecoder->getFileInfoList());

  if (fileState == onlyParsing)
  {
    //info.items.append(infoItem("Num POCs", QString::number(loadingDecoder->getNumberPOCs()), "The number of pictures in the stream."));
    info.items.append(infoItem("NAL units", "Show NAL units", "Show a detailed list of all NAL units.", true));
    info.items.append(infoItem("Reader", inputFormatNames.at(inputFormatType)));
  }
  else if (fileState == noError)
  {
    QSize videoSize = video->getFrameSize();
    info.items.append(infoItem("Reader", inputFormatNames.at(inputFormatType)));
    info.items.append(infoItem("Decoder", loadingDecoder->getDecoderName()));
    info.items.append(infoItem("library path", loadingDecoder->getLibraryPath(), "The path to the loaded libde265 library"));
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
    info.items.append(infoItem("Num POCs", QString::number(startEndFrame.second), "The number of pictures in the stream."));
    info.items.append(infoItem("Internals", loadingDecoder->wrapperInternalsSupported() ? "Yes" : "No", "Is the decoder able to provide internals (statistics)?"));
    info.items.append(infoItem("Stat Parsing", loadingDecoder->statisticsEnabled() ? "Yes" : "No", "Are the statistics of the sequence currently extracted from the stream?"));
    info.items.append(infoItem("NAL units", "Show NAL units", "Show a detailed list of all NAL units.", true));
  }

  return info;
}

void playlistItemCompressedVideo::infoListButtonPressed(int buttonID)
{
  Q_UNUSED(buttonID);

  // The button "Show NAL units" was pressed. Create a dialog with a QTreeView and show the NAL unit list.
  QScopedPointer<parserAnnexB> parserA;
  QScopedPointer<parserAVFormat> parserB;
  if (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC || inputFormatType == inputAnnexBJEM)
  {
    // Just open and parse the file again
    QScopedPointer<fileSourceAnnexBFile> annexBFile(new fileSourceAnnexBFile(plItemNameOrFileName));
    // Create a parser
    if (inputFormatType == inputAnnexBHEVC)
      parserA.reset(new parserAnnexBHEVC());
    else if (inputFormatType == inputAnnexBAVC)
      parserA.reset(new parserAnnexBAVC());
    else if (inputFormatType == inputAnnexBJEM)
      parserA.reset(new parserAnnexBJEM());
    
    // Parse the file
    parserA->enableModel();
    parseAnnexBFile(annexBFile, parserA);
  }
  else // inputLibavformat
  {
    // Just open and parse the file again
    QScopedPointer<fileSourceFFMpegFile> ffmpegFile(new fileSourceFFMpegFile(plItemNameOrFileName));
    AVCodecID codec = inputFileFFMpeg->getCodec();
    parserB.reset(new parserAVFormat(codec));
    parserB->enableModel();
    parseFFMpegFile(ffmpegFile, parserB);
  }
  
  QDialog newDialog;
  QTreeView *view = new QTreeView();
  if (parserA)
    view->setModel(parserA->getNALUnitModel());
  else
    view->setModel(parserB->getNALUnitModel());
  QVBoxLayout *verticalLayout = new QVBoxLayout(&newDialog);
  verticalLayout->addWidget(view);
  newDialog.resize(QSize(1000, 900));
  view->setColumnWidth(0, 400);
  view->setColumnWidth(1, 50);
  newDialog.exec();
}

itemLoadingState playlistItemCompressedVideo::needsLoading(int frameIdx, bool loadRawData)
{
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  auto videoState = video->needsLoading(frameIdxInternal, loadRawData);
  if (videoState == LoadingNeeded || statSource.needsLoading(frameIdxInternal) == LoadingNeeded)
    return LoadingNeeded;
  return videoState;
}

void playlistItemCompressedVideo::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  //if (fileState == noError && frameIdxInternal >= 0 && frameIdxInternal < loadingDecoder->getNumberPOCs())
  //{
  //  video->drawFrame(painter, frameIdxInternal, zoomFactor, drawRawData);
  //  statSource.paintStatistics(painter, frameIdxInternal, zoomFactor);
  //}
  //else if (loadingDecoder->errorInDecoder())
  //{
  //  // There was an error in the deocder. 
  //  infoText = "There was an error when loading the decoder: \n";
  //  infoText += loadingDecoder->decoderErrorString();
  //  infoText += "\n";
  //  infoText += "We do not currently ship the HM and JEM decoder libraries.\n";
  //  infoText += "You can find download links in Help->Downloads";
  //  playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  //}
}

void playlistItemCompressedVideo::loadYUVData(int frameIdxInternal, bool caching)
{
  if (caching && !cachingEnabled)
    return;

  if (!caching && fileState != noError)
    // We can not decode images
    return;

  DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData %d %s", frameIdxInternal, caching ? "caching" : "");

  videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
  yuvVideo->setFrameSize(loadingDecoder->getFrameSize());
  yuvVideo->setYUVPixelFormat(loadingDecoder->getYUVPixelFormat());
  statSource.statFrameSize = loadingDecoder->getFrameSize();

  if (frameIdxInternal > startEndFrame.second || frameIdxInternal < 0)
  {
    DEBUG_HEVC("playlistItemCompressedVideo::loadYUVData Invalid frame index");
    return;
  }

  // Just get the frame from the correct decoder
  QByteArray decByteArray;
  if (caching)
    decByteArray = cachingDecoder->loadYUVFrameData(frameIdxInternal);
  else
    decByteArray = loadingDecoder->loadYUVFrameData(frameIdxInternal);

  if (!decByteArray.isEmpty())
  {
    yuvVideo->rawYUVData = decByteArray;
    yuvVideo->rawYUVData_frameIdx = frameIdxInternal;
  }
}

void playlistItemCompressedVideo::createPropertiesWidget()
{
  // Absolutely always only call this once
  Q_ASSERT_X(!propertiesWidget, "playlistItemCompressedVideo::createPropertiesWidget", "Always create the properties only once!");

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
  if (loadingDecoder)
  {
    ui.comboBoxDisplaySignal->addItems(loadingDecoder->wrapperGetSignalNames());
    ui.comboBoxDisplaySignal->setCurrentIndex(displaySignal);
  }

  // Connect signals/slots
  connect(ui.comboBoxDisplaySignal, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &playlistItemCompressedVideo::displaySignalComboBoxChanged);
}

void playlistItemCompressedVideo::fillStatisticList()
{
  if (!loadingDecoder || !loadingDecoder->wrapperInternalsSupported())
    return;

  loadingDecoder->fillStatisticList(statSource);
}

void playlistItemCompressedVideo::loadStatisticToCache(int frameIdx, int typeIdx)
{
  Q_UNUSED(typeIdx);
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  DEBUG_HEVC("playlistItemCompressedVideo::loadStatisticToCache Request statistics type %d for frame %d", typeIdx, frameIdxInternal);

  if (!loadingDecoder->wrapperInternalsSupported())
    return;

  statSource.statsCache[typeIdx] = loadingDecoder->getStatisticsData(frameIdxInternal, typeIdx);
}

indexRange playlistItemCompressedVideo::getStartEndFrameLimits() const
{
  if (fileState != error)
  {
    if (isAnnexBFileSource())
      return indexRange(0, inputFileAnnexBParser->getNumberPOCs());
    else
      // TODO: 
      return indexRange(0, 0);
  }
  
  return indexRange(0, 0);
}

ValuePairListSets playlistItemCompressedVideo::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  newSet.append("YUV", video->getPixelValues(pixelPos, frameIdxInternal));
  if (loadingDecoder->wrapperInternalsSupported() && loadingDecoder->statisticsEnabled())
    newSet.append("Stats", statSource.getValuesAt(pixelPos));

  return newSet;
}

void playlistItemCompressedVideo::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  QStringList ext;
  ext << "hevc" << "h265" << "265" << "avc" << "h264" << "264" << "avi" << "avr" << "cdxl" << "xl" << "dv" << "dif" << "flm" << "flv" << "flv" << "h261" << "h26l" << "cgi" << "ivr" << "lvf"
      << "m4v" << "mkv" << "mk3d" << "mka" << "mks" << "mjpg" << "mjpeg" << "mpo" << "j2k" << "mov" << "mp4" << "m4a" << "3gp" << "3g2" << "mj2" << "mvi" << "mxg" << "v" << "ogg" 
      << "mjpg" << "viv" << "xmv" << "ts";
  QString filtersString = "FFMpeg files (";
  for (QString e : ext)
    filtersString.append(QString("*.%1").arg(e));
  filtersString.append(")");

  allExtensions.append(ext);
  filters.append(filtersString);
}

void playlistItemCompressedVideo::reloadItemSource()
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

void playlistItemCompressedVideo::cacheFrame(int frameIdx, bool testMode)
{
  if (!cachingEnabled)
    return;

  // Cache a certain frame. This is always called in a separate thread.
  cachingMutex.lock();
  video->cacheFrame(getFrameIdxInternal(frameIdx), testMode);
  cachingMutex.unlock();
}

void playlistItemCompressedVideo::loadFrame(int frameIdx, bool playing, bool loadRawdata, bool emitSignals)
{
  // The current thread must never be the main thread but one of the interactive threads.
  Q_ASSERT(QThread::currentThread() != QApplication::instance()->thread());
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  auto stateYUV = video->needsLoading(frameIdxInternal, loadRawdata);
  auto stateStat = statSource.needsLoading(frameIdxInternal);

  if (stateYUV == LoadingNeeded || stateStat == LoadingNeeded)
  {
    isFrameLoading = true;
    if (stateYUV == LoadingNeeded)
    {
      // Load the requested current frame
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading frame %d %s", frameIdxInternal, playing ? "(playing)" : "");
      video->loadFrame(frameIdxInternal);
    }
    if (stateStat == LoadingNeeded)
    {
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading statistics %d %s", frameIdxInternal, playing ? "(playing)" : "");
      statSource.loadStatistics(frameIdxInternal);
    }

    isFrameLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }

  if (playing && (stateYUV == LoadingNeeded || stateYUV == LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdxInternal + 1;
    if (nextFrameIdx <= startEndFrame.second)
    {
      DEBUG_HEVC("playlistItemRawFile::loadFrame loading frame into double buffer %d %s", nextFrameIdx, playing ? "(playing)" : "");
      isFrameLoadingDoubleBuffer = true;
      video->loadFrame(nextFrameIdx, true);
      isFrameLoadingDoubleBuffer = false;
      if (emitSignals)
        emit signalItemDoubleBufferLoaded();
    }
  }
}

void playlistItemCompressedVideo::determineInputAndDecoder(QWidget *parent, QString fileName, inputFormat &input, decoderEngine &decoder)
{
  // TODO: Determine the combination of reader / decoder to use.
  // There should be an intelligent test to determine the type and then if in doubt we should ask the user.
  // Ask the user if multiple different combinations can be used.

  QFileInfo info(fileName);
  QString ext = info.suffix();
  if (ext == "hevc" || ext == "h265" || ext == "265")
  {
    // Let's try to open it as a raw AnnexBHEVC file
    input = inputAnnexBHEVC;
    decoder = decoderLibde265;
  }
  else
  {
    input = inputLibavformat;
    decoder = decoderFFMpeg;
  }

  /*bool ok;
  QString label = "<html><head/><body><p>There are multiple decoders that we can use in order to decode the raw coded video bitstream file:</p><p><b>libde265:</b> A very fast and open source HEVC decoder. The internals version even supports display of the prediction and residual signal.</p><p><b>libHM:</b> The library version of the HEVC reference test model software (HM). Slower than libde265.</p><p><b>JEM:</b> The library version of the the HEVC next generation decoder software JEM.</p></body></html>";
  QString item = QInputDialog::getItem(parent, "Select a decoder engine", label, decoderEngineNames, 0, false, &ok);
  if (ok && !item.isEmpty())
  {
    int idx = decoderEngineNames.indexOf(item);
    if (idx >= 0 && idx < decoder_NUM)
      return decoderEngine(idx);
  }*/
  
  
}

void playlistItemCompressedVideo::displaySignalComboBoxChanged(int idx)
{
  if (loadingDecoder && displaySignal != idx)
  {
    displaySignal = idx;
    loadingDecoder->setDecodeSignal(idx);
    cachingDecoder->setDecodeSignal(idx);

    // A different display signal was chosen. Invalidate the cache and signal that we will need a redraw.
    videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
    yuvVideo->showPixelValuesAsDiff = (idx == 2 || idx == 3);
    yuvVideo->invalidateAllBuffers();
    emit signalItemChanged(true, RECACHE_CLEAR);
  }
}

void playlistItemCompressedVideo::parseAnnexBFile(QScopedPointer<fileSourceAnnexBFile> &file, QScopedPointer<parserAnnexB> &parser)
{
  DEBUG_HEVC("playlistItemCompressedVideo::parseAnnexBFile");

  //Show a modal QProgressDialog while this operation is running.
  // If the user presses cancel, we will cancel and return false (opening the file failed).
  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
  QWidgetList l = QApplication::topLevelWidgets();
  QWidget *mainWindow = nullptr;
  for (QWidget *w : l)
  {
    MainWindow *mw = dynamic_cast<MainWindow*>(w);
    if (mw)
      mainWindow = mw;
  }
  // Create the dialog
  qint64 maxPos;
  if (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC || inputFormatType == inputAnnexBJEM)
    maxPos = inputFileAnnexB->getFileSize();
  else
    // TODO: What do we do for ffmpeg files?
    maxPos = 1000;
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing AnnexB bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // Just push all NAL units from the annexBFile into the annexBParser
  QByteArray nalData;
  int nalID = 0;
  quint64 filePos;
  while (!file->atEnd())
  {
    try
    {
      nalData = file->getNextNALUnit(filePos);

      parser->parseAndAddNALUnit(nalID, nalData, nullptr, filePos);

      // Update the progress dialog
      if (progress.wasCanceled())
        return;

      qint64 pos;
      if (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC || inputFormatType == inputAnnexBJEM)
        pos = inputFileAnnexB->pos();
      else
        // TODO: 
        pos = 55;

      int newPercentValue = pos * 100 / maxPos;
      if (newPercentValue != curPercentValue)
      {
        progress.setValue(newPercentValue);
        curPercentValue = newPercentValue;
      }
    
      // Next NAL
      nalID++;
    }
    catch (...)
    {
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
      DEBUG_HEVC(":parseAndAddNALUnit Exception thrown parsing NAL %d", nalID);
      nalID++;
    }
  }
  
  // We are done.
  file->seek(0);
  progress.close();
}

void playlistItemCompressedVideo::parseFFMpegFile(QScopedPointer<fileSourceFFMpegFile> &file, QScopedPointer<parserAVFormat> &parser)
{
  // Seek to the beginning of the stream.
  inputFileFFMpeg->seekToPTS(0);

  // Show a modal QProgressDialog while this operation is running.
  // If the user presses cancel, we will cancel and return false (opening the file failed).
  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
  QWidgetList l = QApplication::topLevelWidgets();
  QWidget *mainWindow = nullptr;
  for (QWidget *w : l)
  {
    MainWindow *mw = dynamic_cast<MainWindow*>(w);
    if (mw)
      mainWindow = mw;
  }
  // Create the dialog
  qint64 maxPTS = inputFileFFMpeg->getMaxPTS();
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing (indexing) bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // First get the extradata and push it to the parser
  int packetID = 0;
  QByteArray extradata = inputFileFFMpeg->getExtradata();
  parser->parseExtradata(extradata);

  // Now iterate over all packets and send them to the parser
  QByteArray packetData;
  while (!inputFileFFMpeg->atEnd())
  {
    packetData = inputFileFFMpeg->getNextPacket();
    avPacketInfo_t packetInfo = inputFileFFMpeg->getCurrentPacketInfo();

    try
    {
      parser->parseAVPacketData(packetID, packetInfo, packetData);
    }
    catch (...)
    {
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
      DEBUG_HEVC("parseAVPacketData Exception thrown parsing NAL %d", packetID);
    }
    packetID++;
  }
    
  // Seek back to the beginning of the stream.
  inputFileFFMpeg->seekToPTS(0);
  progress.close();
}