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

#define HEVC_DEBUG_OUTPUT 0
#if HEVC_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_HEVC qDebug
#else
#define DEBUG_HEVC(fmt,...) ((void)0)
#endif

// Conversion from intra prediction mode to vector.
// Coordinates are in x,y with the axes going right and down.
#define VECTOR_SCALING 0.25
const int playlistItemHEVCFile::vectorTable[35][2] = {
  {0,0}, {0,0},
  {32, -32},
  {32, -26}, {32, -21}, {32, -17}, { 32, -13}, { 32,  -9}, { 32, -5}, { 32, -2},
  {32,   0},
  {32,   2}, {32,   5}, {32,   9}, { 32,  13}, { 32,  17}, { 32, 21}, { 32, 26},
  {32,  32},
  {26,  32}, {21,  32}, {17,  32}, { 13,  32}, {  9,  32}, {  5, 32}, {  2, 32},
  {0,   32},
  {-2,  32}, {-5,  32}, {-9,  32}, {-13,  32}, {-17,  32}, {-21, 32}, {-26, 32},
  {-32, 32} };


playlistItemHEVCFile::playlistItemHEVCFile(const QString &hevcFilePath)
  : playlistItem(hevcFilePath, playlistItem_Indexed)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_videoHEVC.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // Initialize variables
  decoder = nullptr;
  decError = DE265_OK;
  retrieveStatistics = false;
  statsCacheCurPOC = -1;
  drawDecodingMessage = false;
  cancelBackgroundDecoding = false;
  playbackRunning = false;

  // Open the input file.
  // This will parse the bitstream and look for random access points in the stream. After this is complete
  // we also know how many pictures there are in this stream.
  annexBFile.openFile(hevcFilePath);

  if (!annexBFile.isOk())
    // Opening the file failed.
    return;

  // The buffer holding the last requested frame (and its POC). (Empty when constructing this)
  // When using the zoom box the getOneFrame function is called frequently so we
  // keep this buffer to not decode the same frame over and over again.
  currentOutputBufferFrameIndex = -1;

  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  loadDecoderLibrary();

  if (wrapperError())
    // There was an internal error while loading/initializing the decoder. Abort.
    return;

  // Allocate the decoder
  allocateNewDecoder();

  // Fill the list of statistics that we can provide
  fillStatisticList();

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  if (startEndFrame.second == -1)
    // No frames to decode
    return;

  // Load frame 0. This will decode the first frame in the sequence and set the
  // correct frame size/YUV format.
  loadYUVData(0, true);

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
  QUrl fileURL( annexBFile.getAbsoluteFilePath() );
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath( annexBFile.getAbsoluteFilePath() );

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
  info.items.append(annexBFile.getFileInfoList());

  if (wrapperError())
  {
    info.items.append(infoItem("Error", wrapperErrorString()));
  }
  else
  {
    QSize videoSize = yuvVideo.getFrameSize();
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
    info.items.append(infoItem("Num POCs", QString::number(annexBFile.getNumberPOCs()), "The number of pictures in the stream."));
    info.items.append(infoItem("Frames Cached",QString::number(yuvVideo.getNrFramesCached())));
    info.items.append(infoItem("Internals", wrapperInternalsSupported() ? "Yes" : "No", "Is the decoder able to provide internals (statistics)?"));
    info.items.append(infoItem("Stat Parsing", retrieveStatistics ? "Yes" : "No", "Are the statistics of the sequence currently extracted from the stream?"));
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
  //TODO: This also has to be handled by the background loading thread but not here.
  //playbackRunning = playback;

  bool noLoadingNeeded = true;
  if (frameIdx != -1)
    yuvVideo.drawFrame(painter, frameIdx, zoomFactor);

  if (!drawDecodingMessage)
    statSource.paintStatistics(painter, frameIdx, zoomFactor);
}

void playlistItemHEVCFile::loadYUVData(int frameIdx, bool forceDecodingNow)
{
  DEBUG_HEVC("playlistItemHEVCFile::loadYUVData %d", frameIdx);
  if (wrapperError())
    return;

  if (frameIdx > startEndFrame.second || frameIdx < 0)
    // Invalid frame index
    return;

  if (backgroundDecodingFuture.isRunning())
  {
    // Abort the background process and perform the next decoding here in this function.
    cancelBackgroundDecoding = true;
    backgroundDecodingFuture.waitForFinished();
  }

  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    yuvVideo.rawYUVData = currentOutputBuffer;
    yuvVideo.rawYUVData_frameIdx = frameIdx;

    return;
  }

  // We have to decode the requested frame.
  bool seeked = false;
  QByteArray parameterSets;
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and decoder it (again).
    int seekFrameIdx = annexBFile.getClosestSeekableFrameNumber(frameIdx);

    DEBUG_HEVC("playlistItemHEVCFile::loadYUVData Seek to %d", seekFrameIdx);
    parameterSets = annexBFile.seekToFrameNumber(seekFrameIdx);
    currentOutputBufferFrameIndex = seekFrameIdx - 1;
    seeked = true;
  }
  else if (frameIdx > currentOutputBufferFrameIndex+2)
  {
    // The requested frame is not the next one or the one after that. Maybe it would be faster to seek ahead in the bitstream and start decoding there.
    // Check if there is a random access point closer to the requested frame than the position that we are at right now.
    int seekFrameIdx = annexBFile.getClosestSeekableFrameNumber(frameIdx);
    if (seekFrameIdx > currentOutputBufferFrameIndex)
    {
      // Yes we can (and should) seek ahead in the file
      DEBUG_HEVC("playlistItemHEVCFile::loadYUVData Seek to %d", seekFrameIdx);
      parameterSets = annexBFile.seekToFrameNumber(seekFrameIdx);
      currentOutputBufferFrameIndex = seekFrameIdx - 1;
      seeked = true;
    }
  }

  if (seeked)
  {
    // Reset the decoder and feed the parameter sets to it.
    // Then start normal decoding

    if (parameterSets.size() == 0)
      return;

    // Delete decoder
    de265_error err = de265_free_decoder(decoder);
    if (err != DE265_OK)
    {
      // Freeing the decoder failed.
      if (decError != err)
        decError = err;
      return;
    }

    decoder = nullptr;

    // Create new decoder
    allocateNewDecoder();

    // Feed the parameter sets
    err = de265_push_data(decoder, parameterSets.data(), parameterSets.size(), 0, nullptr);
  }

  // Perform the decoding in the background if playback is not running.
  if (playbackRunning || forceDecodingNow)
  {
    // Perform the decoding right now blocking the main thread.
    // Decode frames until we receive the one we are looking for.
    while (currentOutputBufferFrameIndex != frameIdx)
    {
      if (!decodeOnePicture(currentOutputBuffer))
        return;
    }
    yuvVideo.rawYUVData = currentOutputBuffer;
    yuvVideo.rawYUVData_frameIdx = frameIdx;
  }
  else
  {
    // Playback is not running. Perform decoding in the background and show a "decoding" message in the meantime.
    yuvVideo.rawYUVData = QByteArray();
    yuvVideo.rawYUVData_frameIdx = -1;

    // Start the background process
    drawDecodingMessage = true;
    cancelBackgroundDecoding = false;
    backgroundDecodingFrameIndex = frameIdx;
    backgroundDecodingFuture = QtConcurrent::run(this, &playlistItemHEVCFile::backgroundProcessDecode);
  }
}

void playlistItemHEVCFile::backgroundProcessDecode()
{
  while (currentOutputBufferFrameIndex != backgroundDecodingFrameIndex && !cancelBackgroundDecoding)
  {
    DEBUG_HEVC("playlistItemHEVCFile::backgroundProcessDecode %d - %d", currentOutputBufferFrameIndex, backgroundDecodingFrameIndex);
    if (!decodeOnePicture(currentOutputBuffer))
    {
      DEBUG_HEVC("playlistItemHEVCFile::backgroundProcessDecode decoding picture failed");
      return;
    }
  }

  if (currentOutputBufferFrameIndex == backgroundDecodingFrameIndex)
  {
    // Background decoding is done and was successful
    yuvVideo.rawYUVData = currentOutputBuffer;
    yuvVideo.rawYUVData_frameIdx = backgroundDecodingFrameIndex;

    // Load the statistics that might have been requested
    for (int i = 0; i < backgroundStatisticsToLoad.count(); i++)
    {
      int typeIdx = backgroundStatisticsToLoad[i];
      statSource.statsCache[typeIdx] = curPOCStats[typeIdx];
    }

    drawDecodingMessage = false;
    emit signalItemChanged(true, false);
  }
}

#if SSE_CONVERSION
bool playlistItemHEVCFile::decodeOnePicture(byteArrayAligned &buffer)
#else
bool playlistItemHEVCFile::decodeOnePicture(QByteArray &buffer)
#endif
{
  de265_error err;
  while (true)
  {
    int more = 1;
    while (more)
    {
      more = 0;

      err = de265_decode(decoder, &more);
      while (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && !annexBFile.atEnd())
      {
        // The decoder needs more data. Get it from the file.
        QByteArray chunk = annexBFile.getRemainingBuffer_Update();

        // Push the data to the decoder
        if (chunk.size() > 0)
        {
          err = de265_push_data(decoder, chunk.data(), chunk.size(), 0, nullptr);
          if (err != DE265_OK && err != DE265_ERROR_WAITING_FOR_INPUT_DATA)
          {
            // An error occurred
            if (decError != err)
              decError = err;
            return false;
          }
        }

        if (annexBFile.atEnd())
          // The file ended.
          err = de265_flush_data(decoder);
      }

      if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && annexBFile.atEnd())
      {
        // The decoder wants more data but there is no more file.
        // We found the end of the sequence. Get the remaining frames from the decoder until
        // more is 0.
      }
      else if (err != DE265_OK)
      {
        // Something went wrong
        more = 0;
        break;
      }

      const de265_image* img = de265_get_next_picture(decoder);
      if (img)
      {
        // We have received an output image
        currentOutputBufferFrameIndex++;

        // First update the chroma format and frame size
        setDe265ChromaMode(img);
        QSize frameSize = QSize(de265_get_image_width(img, 0), de265_get_image_height(img, 0));
        yuvVideo.setFrameSize(frameSize, false);
        statSource.statFrameSize = frameSize;

        // Put image data into buffer
        copyImgToByteArray(img, buffer);

        if (retrieveStatistics)
          // Get the statistics from the image and put them into the statistics cache
          cacheStatistics(img, currentOutputBufferFrameIndex);

        // Picture decoded
        DEBUG_HEVC("playlistItemHEVCFile::decodeOnePicture decoded frame %d", currentOutputBufferFrameIndex);
        return true;
      }
    }

    if (err != DE265_OK)
    {
      // The encoding loop ended because of an error
      if (decError != err)
        decError = err;

      return false;
    }
    if (more == 0)
    {
      // The loop ended because there is nothing more to decode but no error occurred.
      // We are at the end of the sequence.

      // Reset the decoder and restart decoding from the beginning

    }
  }

  // Background parser was canceled
  return false;
}
#if SSE_CONVERSION
void playlistItemHEVCFile::copyImgToByteArray(const de265_image *src, byteArrayAligned &dst)
#else
void playlistItemHEVCFile::copyImgToByteArray(const de265_image *src, QByteArray &dst)
#endif
{
  // How many image planes are there?
  de265_chroma cMode = de265_get_chroma_format(src);
  int nrPlanes = (cMode == de265_chroma_mono) ? 1 : 3;

  // At first get how many bytes we are going to write
  int nrBytes = 0;
  int stride;
  for (int c = 0; c < nrPlanes; c++)
  {
    int width = de265_get_image_width(src, c);
    int height = de265_get_image_height(src, c);
    int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;

    nrBytes += width * height * nrBytesPerSample;
  }

  // Is the output big enough?
  if (dst.capacity() < nrBytes)
    dst.resize(nrBytes);

  // We can now copy from src to dst
  char* dst_c = dst.data();
  for (int c = 0; c < nrPlanes; c++)
  {
    const uint8_t* img_c = de265_get_image_plane(src, c, &stride);
    int width = de265_get_image_width(src, c);
    int height = de265_get_image_height(src, c);
    int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;
    size_t size = width * nrBytesPerSample;

    for (int y = 0; y < height; y++)
    {
      memcpy(dst_c, img_c, size);
      img_c += stride;
      dst_c += size;
    }
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

  if (wrapperInternalsSupported())
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

void playlistItemHEVCFile::allocateNewDecoder()
{
  if (decoder != nullptr)
    return;

  // Create new decoder object
  decoder = de265_new_decoder();

  // Set some decoder parameters
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_SUPPRESS_FAULTY_PICTURES, false);

  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_SAO, false);

  // You could disable SSE acceleration ... not really recommended
  //de265_set_parameter_int(decoder, DE265_DECODER_PARAM_ACCELERATION_CODE, de265_acceleration_SCALAR);

  de265_disable_logging();

  // Verbosity level (0...3(highest))
  de265_set_verbosity(0);

  // Set the number of decoder threads. Libde265 can use wavefronts to utilize these.
  decError = de265_start_worker_threads(decoder, 8);

  // The highest temporal ID to decode. Set this to very high (all) by default.
  de265_set_limit_TID(decoder, 100);
}

void playlistItemHEVCFile::cacheStatistics(const de265_image *img, int iPOC)
{
  if (!wrapperInternalsSupported())
    return;

  // Clear the local statistics cache
  curPOCStats.clear();

  /// --- CTB internals/statistics
  int widthInCTB, heightInCTB, log2CTBSize;
  de265_internals_get_CTB_Info_Layout(img, &widthInCTB, &heightInCTB, &log2CTBSize);
  int ctb_size = 1 << log2CTBSize;	// width and height of each CTB

  // Save Slice index
  {
    QScopedArrayPointer<uint16_t> tmpArr(new uint16_t[ widthInCTB * heightInCTB ]);
    de265_internals_get_CTB_sliceIdx(img, tmpArr.data());
    for (int y = 0; y < heightInCTB; y++)
      for (int x = 0; x < widthInCTB; x++)
      {
        uint16_t val = tmpArr[ y * widthInCTB + x ];
        curPOCStats[0].addBlockValue(x*ctb_size, y*ctb_size, ctb_size, ctb_size, (int)val);
      }
  }

  /// --- CB internals/statistics (part Size, prediction mode, PCM flag, CU trans_quant_bypass_flag)

  // Get CB info array layout from image
  int widthInCB, heightInCB, log2CBInfoUnitSize;
  de265_internals_get_CB_Info_Layout(img, &widthInCB, &heightInCB, &log2CBInfoUnitSize);
  int cb_infoUnit_size = 1 << log2CBInfoUnitSize;
  // Get CB info from image
  QScopedArrayPointer<uint16_t> cbInfoArr(new uint16_t[widthInCB * heightInCB]);
  de265_internals_get_CB_info(img, cbInfoArr.data());

  // Get PB array layout from image
  int widthInPB, heightInPB, log2PBInfoUnitSize;
  de265_internals_get_PB_Info_layout(img, &widthInPB, &heightInPB, &log2PBInfoUnitSize);
  int pb_infoUnit_size = 1 << log2PBInfoUnitSize;

  // Get PB info from image
  QScopedArrayPointer<int16_t> refPOC0(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> refPOC1(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec0_x(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec0_y(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec1_x(new int16_t[widthInPB*heightInPB]);
  QScopedArrayPointer<int16_t> vec1_y(new int16_t[widthInPB*heightInPB]);
  de265_internals_get_PB_info(img, refPOC0.data(), refPOC1.data(), vec0_x.data(), vec0_y.data(), vec1_x.data(), vec1_y.data());

  // Get intra prediction mode (intra direction) layout from image
  int widthInIntraDirUnits, heightInIntraDirUnits, log2IntraDirUnitsSize;
  de265_internals_get_IntraDir_Info_layout(img, &widthInIntraDirUnits, &heightInIntraDirUnits, &log2IntraDirUnitsSize);
  int intraDir_infoUnit_size = 1 << log2IntraDirUnitsSize;

  // Get intra prediction mode (intra direction) from image
  QScopedArrayPointer<uint8_t> intraDirY(new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits]);
  QScopedArrayPointer<uint8_t> intraDirC( new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits]);
  de265_internals_get_intraDir_info(img, intraDirY.data(), intraDirC.data());

  // Get TU info array layout
  int widthInTUInfoUnits, heightInTUInfoUnits, log2TUInfoUnitSize;
  de265_internals_get_TUInfo_Info_layout(img, &widthInTUInfoUnits, &heightInTUInfoUnits, &log2TUInfoUnitSize);
  int tuInfo_unit_size = 1 << log2TUInfoUnitSize;

  // Get TU info
  QScopedArrayPointer<uint8_t> tuInfo(new uint8_t[widthInTUInfoUnits*heightInTUInfoUnits]);
  de265_internals_get_TUInfo_info(img, tuInfo.data());

  for (int y = 0; y < heightInCB; y++)
  {
    for (int x = 0; x < widthInCB; x++)
    {
      uint16_t val = cbInfoArr[ y * widthInCB + x ];

      uint8_t log2_cbSize = (val & 7);	 // Extract lowest 3 bits;

      if (log2_cbSize > 0) {
        // We are in the top left position of a CB.

        // Get values of this CB
        uint8_t cbSizePix = 1 << log2_cbSize;  // Size (w,h) in pixels
        int cbPosX = x * cb_infoUnit_size;	   // Position of this CB in pixels
        int cbPosY = y * cb_infoUnit_size;
        uint8_t partMode = ((val >> 3) & 7);   // Extract next 3 bits (part size);
        uint8_t predMode = ((val >> 6) & 3);   // Extract next 2 bits (prediction mode);
        bool    pcmFlag  = (val & 256);		   // Next bit (PCM flag)
        bool    tqBypass = (val & 512);        // Next bit (TransQuant bypass flag)

        // Set part mode (ID 1)
        curPOCStats[1].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, partMode);

        // Set prediction mode (ID 2)
        curPOCStats[2].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, predMode);

        // Set PCM flag (ID 3)
        curPOCStats[3].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, pcmFlag);

        // Set transQuant bypass flag (ID 4)
        curPOCStats[4].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, tqBypass);

        if (predMode != 0)
        {
          // For each of the prediction blocks set some info

          int numPB = (partMode == 0) ? 1 : (partMode == 3) ? 4 : 2;
          for (int i=0; i<numPB; i++)
          {
            // Get pb position/size
            int pbSubX, pbSubY, pbW, pbH;
            getPBSubPosition(partMode, cbSizePix, i, &pbSubX, &pbSubY, &pbW, &pbH);
            int pbX = cbPosX + pbSubX;
            int pbY = cbPosY + pbSubY;

            // Get index for this xy position in pb_info array
            int pbIdx = (pbY / pb_infoUnit_size) * widthInPB + (pbX / pb_infoUnit_size);

            // Add ref index 0 (ID 5)
            int16_t ref0 = refPOC0[pbIdx];
            if (ref0 != -1)
              curPOCStats[5].addBlockValue(pbX, pbY, pbW, pbH, ref0-iPOC);

            // Add ref index 1 (ID 6)
            int16_t ref1 = refPOC1[pbIdx];
            if (ref1 != -1)
              curPOCStats[6].addBlockValue(pbX, pbY, pbW, pbH, ref1-iPOC);

            // Add motion vector 0 (ID 7)
            if (ref0 != -1)
              curPOCStats[7].addBlockVector(pbX, pbY, pbW, pbH, vec0_x[pbIdx], vec0_y[pbIdx]);

            // Add motion vector 1 (ID 8)
            if (ref1 != -1)
              curPOCStats[8].addBlockVector(pbX, pbY, pbW, pbH, vec1_x[pbIdx], vec1_y[pbIdx]);
          }
        }
        else if (predMode == 0)
        {
          // Get index for this xy position in the intraDir array
          int intraDirIdx = (cbPosY / intraDir_infoUnit_size) * widthInIntraDirUnits + (cbPosX / intraDir_infoUnit_size);

          // Set Intra prediction direction Luma (ID 9)
          int intraDirLuma = intraDirY[intraDirIdx];
          if (intraDirLuma <= 34)
          {
            curPOCStats[9].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, intraDirLuma);

            if (intraDirLuma >= 2)
            {
              // Set Intra prediction direction Luma (ID 9) as vector
              int vecX = (float)vectorTable[intraDirLuma][0] * cbSizePix / 128;
              int vecY = (float)vectorTable[intraDirLuma][1] * cbSizePix / 128;
              curPOCStats[9].addBlockVector(cbPosX, cbPosY, cbSizePix, cbSizePix, vecX, vecY);
            }
          }

          // Set Intra prediction direction Chroma (ID 10)
          int intraDirChroma = intraDirC[intraDirIdx];
          if (intraDirChroma <= 34)
          {
            curPOCStats[10].addBlockValue(cbPosX, cbPosY, cbSizePix, cbSizePix, intraDirChroma);

            if (intraDirChroma >= 2)
            {
              // Set Intra prediction direction Chroma (ID 10) as vector
              int vecX = (float)vectorTable[intraDirChroma][0] * cbSizePix / 128;
              int vecY = (float)vectorTable[intraDirChroma][1] * cbSizePix / 128;
              curPOCStats[10].addBlockVector(cbPosX, cbPosY, cbSizePix, cbSizePix, vecX, vecY);
            }
          }
        }

        // Walk into the TU tree
        int tuIdx = (cbPosY / tuInfo_unit_size) * widthInTUInfoUnits + (cbPosX / tuInfo_unit_size);
        cacheStatistics_TUTree_recursive(tuInfo.data(), widthInTUInfoUnits, tuInfo_unit_size, iPOC, tuIdx, cbSizePix / tuInfo_unit_size, 0);
      }
    }
  }

  // The cache now contains the statistics for iPOC
  statsCacheCurPOC = iPOC;
}

void playlistItemHEVCFile::getPBSubPosition(int partMode, int cbSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH) const
{
  // Get the position/width/height of the PB
  if (partMode == 0) // PART_2Nx2N
  {
    *pbW = cbSizePix;
    *pbH = cbSizePix;
    *pbX = 0;
    *pbY = 0;
  }
  else if (partMode == 1) // PART_2NxN
  {
    *pbW = cbSizePix;
    *pbH = cbSizePix / 2;
    *pbX = 0;
    *pbY = (pbIdx == 0) ? 0 : cbSizePix / 2;
  }
  else if (partMode == 2) // PART_Nx2N
  {
    *pbW = cbSizePix / 2;
    *pbH = cbSizePix;
    *pbX = (pbIdx == 0) ? 0 : cbSizePix / 2;
    *pbY = 0;
  }
  else if (partMode == 3) // PART_NxN
  {
    *pbW = cbSizePix / 2;
    *pbH = cbSizePix / 2;
    *pbX = (pbIdx == 0 || pbIdx == 2) ? 0 : cbSizePix / 2;
    *pbY = (pbIdx == 0 || pbIdx == 1) ? 0 : cbSizePix / 2;
  }
  else if (partMode == 4) // PART_2NxnU
  {
    *pbW = cbSizePix;
    *pbH = (pbIdx == 0) ? cbSizePix / 4 : cbSizePix / 4 * 3;
    *pbX = 0;
    *pbY = (pbIdx == 0) ? 0 : cbSizePix / 4;
  }
  else if (partMode == 5) // PART_2NxnD
  {
    *pbW = cbSizePix;
    *pbH = (pbIdx == 0) ? cbSizePix / 4 * 3 : cbSizePix / 4;
    *pbX = 0;
    *pbY = (pbIdx == 0) ? 0 : cbSizePix / 4 * 3;
  }
  else if (partMode == 6) // PART_nLx2N
  {
    *pbW = (pbIdx == 0) ? cbSizePix / 4 : cbSizePix / 4 * 3;
    *pbH = cbSizePix;
    *pbX = (pbIdx == 0) ? 0 : cbSizePix / 4;
    *pbY = 0;
  }
  else if (partMode == 7) // PART_nRx2N
  {
    *pbW = (pbIdx == 0) ? cbSizePix / 4 * 3 : cbSizePix / 4;
    *pbH = cbSizePix;
    *pbX = (pbIdx == 0) ? 0 : cbSizePix / 4 * 3;
    *pbY = 0;
  }
}

/* Walk into the TU tree and set the tree depth as a statistic value if the TU is not further split
 * \param tuInfo: The tuInfo array
 * \param tuInfoWidth: The number of TU units per line in the tuInfo array
 * \param tuUnitSizePix: The size of one TU unit in pixels
 * \param iPOC: The current POC
 * \param tuIdx: The top left index of the currently handled TU in tuInfo
 * \param tuWidth_units: The WIdth of the TU in units
 * \param trDepth: The current transform tree depth
*/
void playlistItemHEVCFile::cacheStatistics_TUTree_recursive(uint8_t *const tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int tuWidth_units, int trDepth)
{
  // Check if the TU is further split.
  if (tuInfo[tuIdx] & (1 << trDepth))
  {
    // The transform is split further
    int yOffset = (tuWidth_units / 2) * tuInfoWidth;
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx                              , tuWidth_units / 2, trDepth+1);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx           + tuWidth_units / 2, tuWidth_units / 2, trDepth+1);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset                    , tuWidth_units / 2, trDepth+1);
    cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset + tuWidth_units / 2, tuWidth_units / 2, trDepth+1);
  }
  else
  {
    // The transform is not split any further. Add the TU depth to the statistics (ID 11)
    int tuWidth = tuWidth_units * tuUnitSizePix;
    int posX_units = tuIdx % tuInfoWidth;
    int posY_units = tuIdx / tuInfoWidth;
    curPOCStats[11].addBlockValue(posX_units * tuUnitSizePix, posY_units * tuUnitSizePix, tuWidth, tuWidth, trDepth);
  }
}

/* Convert the de265_chroma format to a YUVCPixelFormatType and set it.
*/
void playlistItemHEVCFile::setDe265ChromaMode(const de265_image *img)
{
  using namespace YUV_Internals;

  de265_chroma cMode = de265_get_chroma_format(img);
  int nrBitsC0 = de265_get_bits_per_pixel(img, 0);
  if (cMode == de265_chroma_mono)
    yuvVideo.setYUVPixelFormat(yuvPixelFormat(YUV_400, nrBitsC0));
  else if (cMode == de265_chroma_420)
    yuvVideo.setYUVPixelFormat(yuvPixelFormat(YUV_420, nrBitsC0));
  else if (cMode == de265_chroma_422)
    yuvVideo.setYUVPixelFormat(yuvPixelFormat(YUV_422, nrBitsC0));
  else if (cMode == de265_chroma_444)
    yuvVideo.setYUVPixelFormat(yuvPixelFormat(YUV_444, nrBitsC0));
}

void playlistItemHEVCFile::fillStatisticList()
{
  if (!wrapperInternalsSupported())
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
  DEBUG_HEVC("Request statistics type %d for frame %d", typeIdx, frameIdx);

  if (!wrapperInternalsSupported())
    return;

  if (!retrieveStatistics)
  {
    // The value of retrieveStatistics changed. We need to update the info of this item but
    // no redraw is needed.
    retrieveStatistics = true;
    emit signalItemChanged(false, false);
  }

  // We will have to decode the current frame again to get the internals/statistics
  // This can be done like this:
  if (frameIdx != statsCacheCurPOC)
  {
    if (frameIdx == currentOutputBufferFrameIndex)
      currentOutputBufferFrameIndex ++;

    loadYUVData(frameIdx, false);
  }

  // The statistics should now be in the local cache
  if (frameIdx == statsCacheCurPOC)
  {
    // The statistics are in the cache of statistics for the current POC.
    // Push the statistics of the given type index to the statSource
    statSource.statsCache[typeIdx] = curPOCStats[typeIdx];
  }
  else
  {
    if (backgroundDecodingFrameIndex == frameIdx)
    {
      // The frame is being decoded in the background.
      // When the background process is done, it shall load the statistics.
      backgroundStatisticsToLoad.append(typeIdx);
    }
  }
}

ValuePairListSets playlistItemHEVCFile::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;

  newSet.append("YUV", yuvVideo.getPixelValues(pixelPos, frameIdx));
  if (wrapperInternalsSupported() && retrieveStatistics)
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
  if (wrapperError())
    // Nothing is working, so there is nothing to reset.
    return;

  // Abort the background decoding process if it is running
  if (backgroundDecodingFuture.isRunning())
  {
    cancelBackgroundDecoding = true;
    backgroundDecodingFuture.waitForFinished();
  }

  // Reset the playlistItemHEVCFile variables/buffers.
  decError = DE265_OK;
  statsCacheCurPOC = -1;
  drawDecodingMessage = false;
  cancelBackgroundDecoding = false;
  currentOutputBufferFrameIndex = -1;

  // Re-open the input file. This will reload the bitstream as if it was completely unknown.
  annexBFile.openFile(plItemNameOrFileName);

  if (!annexBFile.isOk())
    // Opening the file failed.
    return;

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  // Reset the videoHandlerYUV source. With the next draw event, the videoHandlerYUV will request to decode the frame again.
  yuvVideo.invalidateAllBuffers();

  // Load frame 0. This will decode the first frame in the sequence and set the
  // correct frame size/YUV format.
  loadYUVData(0, true);
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
