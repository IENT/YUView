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
#include <assert.h>
#include <QDebug>
#include <QUrl>
#include <QPainter>
#include <QtConcurrent>

#define HEVC_DECODING_TEXT "Decoding..."

#define HEVC_DEBUG_OUTPUT 0
#if HEVC_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_HEVC qDebug
#else
#define DEBUG_HEVC(fmt,...) ((void)0)
#endif

#define SET_INTERNALERROR_RETURN(errTxt) { internalError = true; StatusText = errTxt; return; }
#define DISABLE_INTERNALS_RETURN()       { internalsSupported = false; return; }

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


playlistItemHEVCFile::playlistItemHEVCFile(QString hevcFilePath)
  : playlistItemIndexed(hevcFilePath)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_television.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // Init variables
  decoder = NULL;
  decError = DE265_OK;
  retrieveStatistics = false;
  internalsSupported = true;
  internalError = false;
  statsCacheCurPOC = -1;
  drawDecodingMessage = false;
  cancelBackgroundDecoding = false;

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

  // Try to load the decoder library (.dll on windows, .so on linux, .dylib on mac)
  loadDecoderLibrary();

  if (internalError)
    // There was an internal error while loading/initializing the decoder. Abort.
    return;

  // Allocate the decoder
  allocateNewDecoder();

  // Fill the list of statistics that we can provide
  fillStatisticList();

  // Set the frame number limits
  startEndFrame = getstartEndFrameLimits();

  if (startEndFrame.second == -1)
    // No frames to decode
    return;

  // Load frame 0. This will decode the first frame in the sequence and set the 
  // correct frame size/YUV format.
  playbackRunning = true;   //< Set this to true for this first loading so that it is not performed in the background.
  loadYUVData(0);

  // If the yuvVideHandler requests raw YUV data, we provide it from the file
  connect(&yuvVideo, SIGNAL(signalRequesRawData(int)), this, SLOT(loadYUVData(int)), Qt::DirectConnection);
  connect(&yuvVideo, SIGNAL(signalHandlerChanged(bool,bool)), this, SLOT(slotEmitSignalItemChanged(bool,bool)));
  connect(&yuvVideo, SIGNAL(signalUpdateFrameLimits()), this, SLOT(slotUpdateFrameLimits()));
  connect(&statSource, SIGNAL(updateItem(bool)), this, SLOT(updateStatSource(bool)));
  connect(&statSource, SIGNAL(requestStatisticsLoading(int,int)), this, SLOT(loadStatisticToCache(int,int)));

  // A HEVC file can be cached. TODO!
  cachingEnabled = false;
}

playlistItemHEVCFile::~playlistItemHEVCFile()
{
}

void playlistItemHEVCFile::savePlaylist(QDomElement &root, QDir playlistDir)
{
  // Determine the relative path to the hevc file. We save both in the playlist.
  QUrl fileURL( annexBFile.getAbsoluteFilePath() );
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath( annexBFile.getAbsoluteFilePath() );

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemHEVCFile");

  // Append the properties of the playlistItemIndexed
  playlistItemIndexed::appendPropertiesToPlaylist(d);

  // Apppend all the properties of the hevc file (the path to the file. Relative and absolute)
  d.appendProperiteChild( "absolutePath", fileURL.toString() );
  d.appendProperiteChild( "relativePath", relativePath  );
  
  root.appendChild(d);
}

playlistItemHEVCFile *playlistItemHEVCFile::newplaylistItemHEVCFile(QDomElementYUView root, QString playlistFilePath)
{
  // Parse the dom element. It should have all values of a playlistItemHEVCFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  
  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return NULL;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemHEVCFile *newFile = new playlistItemHEVCFile(filePath);

  // Load the propertied of the playlistItemIndexed
  playlistItemIndexed::loadPropertiesFromPlaylist(root, newFile);
  
  return newFile;
}

QList<infoItem> playlistItemHEVCFile::getInfoList()
{
  QList<infoItem> infoList;

  // At first append the file information part (path, date created, file size...)
  infoList.append(annexBFile.getFileInfoList());

  if (internalError)
  {
    infoList.append(infoItem("Error", StatusText));
  }
  else
  {
    QSize videoSize = yuvVideo.getFrameSize();
    infoList.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
    infoList.append(infoItem("Num POCs", QString::number(annexBFile.getNumberPOCs()), "The number of pictures in the stream."));
    infoList.append(infoItem("Frames Cached",QString::number(yuvVideo.getNrFramesCached())));
    infoList.append(infoItem("Internals", internalsSupported ? "Yes" : "No", "Is the decoder able to provide internals (statistics)?"));
    infoList.append(infoItem("Stat Parsing", retrieveStatistics ? "Yes" : "No", "Are the statistics of the sequence currently extracted from the stream?"));
  }

  return infoList;
}

void playlistItemHEVCFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback)
{
  playbackRunning = playback;

  if (frameIdx != -1)
    yuvVideo.drawFrame(painter, frameIdx, zoomFactor);

  // drawDecodingMessage will be true if in yuvVideo.drawFrame is was noticed that we need to decode a new frame
  // and that background process is now running.
  if (drawDecodingMessage)
  {
    if (backgroundImage.isNull())
    {
      // Create the background image (which is the last shown frame in gray)
      QImage grayscaleImage = yuvVideo.getCurrentFrameAsImage();

      // Convert the image to grayscale
      for (int i = 0; i < grayscaleImage.height(); i++)
      {
        uchar* scan = grayscaleImage.scanLine(i);
        for (int j = 0; j < grayscaleImage.width(); j++) 
        {
          QRgb* rgbpixel = reinterpret_cast<QRgb*>(scan + j * 4);
          int gray = qGray(*rgbpixel);
          *rgbpixel = QColor(gray, gray, gray).rgba();
        }
      }

      backgroundImage = QPixmap::fromImage(grayscaleImage);
    }

    // Draw the background image
    QRect videoRect;
    videoRect.setSize(backgroundImage.size() * zoomFactor);
    videoRect.moveCenter(QPoint(0,0));
    painter->drawPixmap(videoRect, backgroundImage);

    // Set font using the zoom factor
    QFont displayFont = painter->font();
    displayFont.setPointSizeF(painter->font().pointSizeF() * zoomFactor);
    painter->setFont(displayFont);

    // Set the rect where to show the text
    QFontMetrics metrics(displayFont);
    QSize textSize = metrics.size(0, HEVC_DECODING_TEXT);
        
    QRect textRect;
    textRect.setSize( textSize );
    textRect.moveCenter( QPoint(0,0) );

    // Draw a rect around the text in white with a black border
    QRect boxRect = textRect + QMargins(2*zoomFactor, 2*zoomFactor, 2*zoomFactor, 2*zoomFactor);
    painter->setPen(QPen(Qt::black, 1));
    painter->fillRect(boxRect,Qt::white);
    painter->drawRect(boxRect);
    
    // Draw the text
    painter->drawText(textRect, Qt::AlignCenter, HEVC_DECODING_TEXT);
  }
  else
  {
    backgroundImage = QPixmap();
    statSource.paintStatistics(painter, frameIdx, zoomFactor);
  }
}

void playlistItemHEVCFile::loadYUVData(int frameIdx)
{
  DEBUG_HEVC("Request frame %d", frameIdx);
  if (internalError)
    return;

  if (backgroundDecodingFuture.isRunning())
  {
    // Aborth the background process and perform the next decoding here in this function.
    cancelBackgroundDecoding = true;
    backgroundDecodingFuture.waitForFinished();
  }

  // At first check if the request is for the frame that has been requested in the
  // last call to this function.
  if (frameIdx == currentOutputBufferFrameIndex)
  {
    assert(!currentOutputBuffer.isEmpty()); // Must not be empty or something is wrong
    yuvVideo.rawData = currentOutputBuffer;
    yuvVideo.rawData_frameIdx = frameIdx;

    return;
  }

  // We have to decode the requested frame.
  bool seeked = false;
  QByteArray parameterSets;
  if ((int)frameIdx < currentOutputBufferFrameIndex || currentOutputBufferFrameIndex == -1)
  {
    // The requested frame lies before the current one. We will have to rewind and decoder it (again).
    int seekFrameIdx = annexBFile.getClosestSeekableFrameNumber(frameIdx);

    DEBUG_HEVC("Seek to frame %d", seekFrameIdx);
    parameterSets = annexBFile.seekToFrameNumber(seekFrameIdx);
    currentOutputBufferFrameIndex = seekFrameIdx - 1;
    seeked = true;
  }
  else if (frameIdx > currentOutputBufferFrameIndex+2)
  {
    // The requested frame is not the next one or the one after that. Maybe it would be faster to seek ahead in the bitstream and start decoding there.
    // Check if there is a random access point closer to the requested frame than the position that we are at right now.
    int seekFrameIdx = annexBFile.getClosestSeekableFrameNumber(frameIdx);
    if (seekFrameIdx > currentOutputBufferFrameIndex) {
      // Yes we can (and should) seek ahead in the file
      DEBUG_HEVC("Seek to frame %d", seekFrameIdx);
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
    if (err != DE265_OK) {
      // Freeing the decoder failed.
      if (decError != err) {
        decError = err;
      }
      return;
    }

    decoder = NULL;

    // Create new decoder
    allocateNewDecoder();

    // Feed the parameter sets
    err = de265_push_data(decoder, parameterSets.data(), parameterSets.size(), 0, NULL);
  }

  // Perfrom the decoding in the background if playback is not running.
  if (playbackRunning)
  {
    // Perform the decoding right now blocking the main thread. 
    // Decode frames until we recieve the one we are looking for.
    while (currentOutputBufferFrameIndex != frameIdx)
    {
      if (!decodeOnePicture(currentOutputBuffer))
        return;
    }
    yuvVideo.rawData = currentOutputBuffer;
    yuvVideo.rawData_frameIdx = frameIdx;
  }
  else
  {
    // Playback is not running. Perform decoding in the background and show a "decoding" message in the meantime.
    yuvVideo.rawData = QByteArray();
    yuvVideo.rawData_frameIdx = -1;

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
    DEBUG_HEVC( "Background decoding %d - %d", currentOutputBufferFrameIndex, backgroundDecodingFrameIndex );
    if (!decodeOnePicture(currentOutputBuffer))
      return;
  }

  if (currentOutputBufferFrameIndex == backgroundDecodingFrameIndex)
  {
    // Background decoding is done and was successfull
    yuvVideo.rawData = currentOutputBuffer;
    yuvVideo.rawData_frameIdx = backgroundDecodingFrameIndex;

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
        if (chunk.size() > 0) {
          err = de265_push_data(decoder, chunk.data(), chunk.size(), 0, NULL);
          if (err != DE265_OK && err != DE265_ERROR_WAITING_FOR_INPUT_DATA)
          {
            // An error occured
            if (decError != err) {
              decError = err;
            }
            return false;
          }
        }

        if (annexBFile.atEnd()) {
          // The file ended.
          err = de265_flush_data(decoder);
        }
      }

      if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && annexBFile.atEnd())
      {
        // The decoder wants more data but there is no more file.
        // We found the end of the sequence. Get the remaininf frames from the decoder until
        // more is 0.
      }
      else if (err != DE265_OK) {
        // Something went wrong
        more = 0;
        break;
      }

      const de265_image* img = de265_get_next_picture(decoder);
      if (img) {
        // We have recieved an output image
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
        DEBUG_HEVC("One picture decoded %d", currentOutputBufferFrameIndex);
        return true;
      }
    }

    if (err != DE265_OK)
    {
      // The encoding loop ended becuase of an error
      if (decError != err)
        decError = err;

      return false;
    }
    if (more == 0)
    {
      // The loop ended because there is nothing more to decode but no error occured.
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
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemHEVCFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);

  QFrame *lineOne = new QFrame(propertiesWidget);
  lineOne->setObjectName(QStringLiteral("line"));
  lineOne->setFrameShape(QFrame::HLine);
  lineOne->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first index controllers (start/end...) then yuv controls (format,...)
  vAllLaout->addLayout( createIndexControllers(propertiesWidget) );
  vAllLaout->addWidget( lineOne );
  vAllLaout->addLayout( yuvVideo.createVideoHandlerControls(propertiesWidget, true) );

  if (internalsSupported)
  {
    QFrame *line2 = new QFrame(propertiesWidget);
    line2->setObjectName(QStringLiteral("line"));
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);

    vAllLaout->addWidget( line2 );
    vAllLaout->addLayout( statSource.createStatisticsHandlerControls(propertiesWidget), 1 );
  }
  else
  {
    // Insert a stretch at the bottom of the vertical global layout so that everything
    // gets 'pushed' to the top.
    vAllLaout->insertStretch(5, 1);
  }

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

void playlistItemHEVCFile::loadDecoderLibrary()
{
  // Try to load the libde265 library from the current working directory
  // Unfortunately relative paths like this do not work: (at leat on windows)
  //decLib.setFileName(".\\libde265");

#ifdef Q_OS_MAC
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib
  QString libName = "/libde265.dylib";
#else
  // On windows and linux omnitting the extension works
  QString libName = "/libde265";
#endif

  QString libDir = QDir::currentPath() + "/" + libName;
  decLib.setFileName(libDir);
  bool libLoaded = decLib.load();
  if (!libLoaded)
  {
  // Loading failed. Try subdirectory libde265
  QString strErr = decLib.errorString();
  libDir = QDir::currentPath() + "/libde265/" + libName;
      decLib.setFileName(libDir);
      libLoaded = decLib.load();
  }

  if (!libLoaded)
  {
    // Loading failed. Try the directory that the executable is in.
    libDir = QCoreApplication::applicationDirPath() + "/" + libName;
    decLib.setFileName(libDir);
    libLoaded = decLib.load();
  }

  if (!libLoaded)
  {
    // Loading failed. Try the subdirector libde265 of the directory that the executable is in.
    libDir = QCoreApplication::applicationDirPath() + "/libde265/" + libName;
    decLib.setFileName(libDir);
    libLoaded = decLib.load();
  }

  if (!libLoaded)
  {
    // Loading failed. Try system directories.
    QString strErr = decLib.errorString();
    libDir = "libde265";
    decLib.setFileName(libDir);
    libLoaded = decLib.load();
  }

  if (!libLoaded)
  {
    // Loading still failed.
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: " + decLib.errorString())
  }

  // Get/check function pointers
  de265_new_decoder = (f_de265_new_decoder)decLib.resolve("de265_new_decoder");
  if (!de265_new_decoder)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_new_decoder was not found.")

  de265_set_parameter_bool = (f_de265_set_parameter_bool)decLib.resolve("de265_set_parameter_bool");
  if (!de265_set_parameter_bool)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_parameter_bool was not found.")

  de265_set_parameter_int = (f_de265_set_parameter_int)decLib.resolve("de265_set_parameter_int");
  if (!de265_set_parameter_int)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_parameter_int was not found.")

  de265_disable_logging = (f_de265_disable_logging)decLib.resolve("de265_disable_logging");
  if (!de265_disable_logging)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_disable_logging was not found.")

  de265_set_verbosity= (f_de265_set_verbosity)decLib.resolve("de265_set_verbosity");
  if (!de265_set_verbosity)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_verbosity was not found.")

  de265_start_worker_threads= (f_de265_start_worker_threads)decLib.resolve("de265_start_worker_threads");
  if (!de265_start_worker_threads)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_start_worker_threads was not found.")

  de265_set_limit_TID = (f_de265_set_limit_TID)decLib.resolve("de265_set_limit_TID");
  if (!de265_set_limit_TID)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_limit_TID was not found.")

  de265_get_error_text = (f_de265_get_error_text)decLib.resolve("de265_get_error_text");
  if (!de265_get_error_text)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_error_text was not found.")

  de265_get_chroma_format = (f_de265_get_chroma_format)decLib.resolve("de265_get_chroma_format");
  if (!de265_get_chroma_format)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_chroma_format was not found.")

  de265_get_image_width = (f_de265_get_image_width)decLib.resolve("de265_get_image_width");
  if (!de265_get_image_width)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_image_width was not found.")

  de265_get_image_height = (f_de265_get_image_height)decLib.resolve("de265_get_image_height");
  if (!de265_get_image_height)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_image_height was not found.")

  de265_get_image_plane = (f_de265_get_image_plane)decLib.resolve("de265_get_image_plane");
  if (!de265_get_image_plane)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_image_plane was not found.")

  de265_get_bits_per_pixel = (f_de265_get_bits_per_pixel)decLib.resolve("de265_get_bits_per_pixel");
  if (!de265_get_bits_per_pixel)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_bits_per_pixel was not found.")

  de265_decode = (f_de265_decode)decLib.resolve("de265_decode");
  if (!de265_decode)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_decode was not found.")

  de265_push_data = (f_de265_push_data)decLib.resolve("de265_push_data");
  if (!de265_push_data)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_push_data was not found.")

  de265_flush_data = (f_de265_flush_data)decLib.resolve("de265_flush_data");
  if (!de265_flush_data)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_flush_data was not found.")

  de265_get_next_picture = (f_de265_get_next_picture)decLib.resolve("de265_get_next_picture");
  if (!de265_get_next_picture)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_next_picture was not found.")

  de265_free_decoder = (f_de265_free_decoder)decLib.resolve("de265_free_decoder");
  if (!de265_free_decoder)
    SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_free_decoder was not found.")

  //// Get pointers to the internals/statistics functions (if present)
  //// If not, disable the statistics extraction. Normal decoding of the video will still work.

  de265_internals_get_CTB_Info_Layout = (f_de265_internals_get_CTB_Info_Layout)decLib.resolve("de265_internals_get_CTB_Info_Layout");
  if (!de265_internals_get_CTB_Info_Layout)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_CTB_sliceIdx = (f_de265_internals_get_CTB_sliceIdx)decLib.resolve("de265_internals_get_CTB_sliceIdx");
  if (!de265_internals_get_CTB_sliceIdx)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_CB_Info_Layout = (f_de265_internals_get_CB_Info_Layout)decLib.resolve("de265_internals_get_CB_Info_Layout");
  if (!de265_internals_get_CB_Info_Layout)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_CB_info = (f_de265_internals_get_CB_info)decLib.resolve("de265_internals_get_CB_info");
  if (!de265_internals_get_CB_info)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_PB_Info_layout = (f_de265_internals_get_PB_Info_layout)decLib.resolve("de265_internals_get_PB_Info_layout");
  if (!de265_internals_get_PB_Info_layout)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_PB_info = (f_de265_internals_get_PB_info)decLib.resolve("de265_internals_get_PB_info");
  if (!de265_internals_get_PB_info)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_IntraDir_Info_layout = (f_de265_internals_get_IntraDir_Info_layout)decLib.resolve("de265_internals_get_IntraDir_Info_layout");
  if (!de265_internals_get_IntraDir_Info_layout)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_intraDir_info = (f_de265_internals_get_intraDir_info)decLib.resolve("de265_internals_get_intraDir_info");
  if (!de265_internals_get_intraDir_info)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_TUInfo_Info_layout = (f_de265_internals_get_TUInfo_Info_layout)decLib.resolve("de265_internals_get_TUInfo_Info_layout");
  if (!de265_internals_get_TUInfo_Info_layout)
    DISABLE_INTERNALS_RETURN();

  de265_internals_get_TUInfo_info = (f_de265_internals_get_TUInfo_info)decLib.resolve("de265_internals_get_TUInfo_info");
  if (!de265_internals_get_TUInfo_info)
    DISABLE_INTERNALS_RETURN();
}

void playlistItemHEVCFile::allocateNewDecoder()
{
  if (decoder != NULL)
    return;

  // Create new decoder object
  decoder = de265_new_decoder();

  // Set some decoder parameters
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_SUPPRESS_FAULTY_PICTURES, false);

  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, false);
  de265_set_parameter_bool(decoder, DE265_DECODER_PARAM_DISABLE_SAO, false);

  // You could disanle SSE acceleration ... not really recommended
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
  if (!internalsSupported)
    return;

  // Clear the local statistics cache
  curPOCStats.clear();

  StatisticsItem anItem;
  anItem.rawValues[1] = 0;

  /// --- CTB internals/statistics
  int widthInCTB, heightInCTB, log2CTBSize;
  de265_internals_get_CTB_Info_Layout(img, &widthInCTB, &heightInCTB, &log2CTBSize);
  int ctb_size = 1 << log2CTBSize;	// width and height of each ctb

  // Save Slice index
  StatisticsType *statsTypeSliceIdx = statSource.getStatisticsType(0);
  anItem.type = blockType;
  uint16_t *tmpArr = new uint16_t[ widthInCTB * heightInCTB ];
  de265_internals_get_CTB_sliceIdx(img, tmpArr);
  for (int y = 0; y < heightInCTB; y++)
    for (int x = 0; x < widthInCTB; x++)
    {
      uint16_t val = tmpArr[ y * widthInCTB + x ];
      anItem.color = statsTypeSliceIdx->colorRange->getColor((float)val);
      anItem.rawValues[0] = (int)val;
      anItem.positionRect = QRect(x*ctb_size, y*ctb_size, ctb_size, ctb_size);

      curPOCStats[0].append(anItem);
    }

  delete tmpArr;
  tmpArr = NULL;

  /// --- CB internals/statistics (part Size, prediction mode, pcm flag, CU trans quant bypass flag)

  // Get CB info array layout from image
  int widthInCB, heightInCB, log2CBInfoUnitSize;
  de265_internals_get_CB_Info_Layout(img, &widthInCB, &heightInCB, &log2CBInfoUnitSize);
  int cb_infoUnit_size = 1 << log2CBInfoUnitSize;
  // Get CB info from image
  uint16_t *cbInfoArr = new uint16_t[widthInCB * heightInCB];
  de265_internals_get_CB_info(img, cbInfoArr);

  // Get PB array layout from image
  int widthInPB, heightInPB, log2PBInfoUnitSize;
  de265_internals_get_PB_Info_layout(img, &widthInPB, &heightInPB, &log2PBInfoUnitSize);
  int pb_infoUnit_size = 1 << log2PBInfoUnitSize;

  // Get PB info from image
  int16_t *refPOC0 = new int16_t[widthInPB*heightInPB];
  int16_t *refPOC1 = new int16_t[widthInPB*heightInPB];
  int16_t *vec0_x  = new int16_t[widthInPB*heightInPB];
  int16_t *vec0_y  = new int16_t[widthInPB*heightInPB];
  int16_t *vec1_x  = new int16_t[widthInPB*heightInPB];
  int16_t *vec1_y  = new int16_t[widthInPB*heightInPB];
  de265_internals_get_PB_info(img, refPOC0, refPOC1, vec0_x, vec0_y, vec1_x, vec1_y);

  // Get intra pred mode (intra dir) layout from image
  int widthInIntraDirUnits, heightInIntraDirUnits, log2IntraDirUnitsSize;
  de265_internals_get_IntraDir_Info_layout(img, &widthInIntraDirUnits, &heightInIntraDirUnits, &log2IntraDirUnitsSize);
  int intraDir_infoUnit_size = 1 << log2IntraDirUnitsSize;

  // Get intra pred mode (intra dir) from image
  uint8_t *intraDirY = new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits];
  uint8_t *intraDirC = new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits];
  de265_internals_get_intraDir_info(img, intraDirY, intraDirC);

  // Get tu info array layou
  int widthInTUInfoUnits, heightInTUInfoUnits, log2TUInfoUnitSize;
  de265_internals_get_TUInfo_Info_layout(img, &widthInTUInfoUnits, &heightInTUInfoUnits, &log2TUInfoUnitSize);
  int tuInfo_unit_size = 1 << log2TUInfoUnitSize;

  // Get TU info
  uint8_t *tuInfo = new uint8_t[widthInTUInfoUnits*heightInTUInfoUnits];
  de265_internals_get_TUInfo_info(img, tuInfo);

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
        bool    tqBypass = (val & 512);        // Next bit (Transquant bypass flag)

        // Set CB position
        anItem.positionRect = QRect(cbPosX, cbPosY, cbSizePix, cbSizePix);

        // Set part mode (ID 1)
        anItem.rawValues[0] = partMode;
        anItem.color = statSource.getStatisticsType(1)->colorRange->getColor(partMode);
        curPOCStats[1].append(anItem);

        // Set pred mode (ID 2)
        anItem.rawValues[0] = predMode;
        anItem.color = statSource.getStatisticsType(2)->colorRange->getColor(predMode);
        curPOCStats[2].append(anItem);

        // Set PCM flag (ID 3)
        anItem.rawValues[0] = pcmFlag;
        anItem.color = statSource.getStatisticsType(3)->colorRange->getColor(pcmFlag);
        curPOCStats[3].append(anItem);

        // Set transquant bypass flag (ID 4)
        anItem.rawValues[0] = tqBypass;
        anItem.color = statSource.getStatisticsType(4)->colorRange->getColor(tqBypass);
        curPOCStats[4].append(anItem);

        if (predMode != 0) {
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

            StatisticsItem pbItem;
            pbItem.type = blockType;
            pbItem.positionRect = QRect(pbX, pbY, pbW, pbH);

            // Add ref index 0 (ID 5)
            int16_t ref0 = refPOC0[pbIdx];
            if (ref0 != -1)
            {
              pbItem.rawValues[0] = ref0;
              pbItem.color = statSource.getStatisticsType(5)->colorRange->getColor(ref0-iPOC);
             curPOCStats[5].append(pbItem);
            }

            // Add ref index 1 (ID 6)
            int16_t ref1 = refPOC1[pbIdx];
            if (ref1 != -1)
            {
              pbItem.rawValues[0] = ref1;
              pbItem.color = statSource.getStatisticsType(6)->colorRange->getColor(ref1-iPOC);
              curPOCStats[6].append(pbItem);
            }

            // Add motion vector 0 (ID 7)
            pbItem.type = arrowType;
            if (ref0 != -1)
            {
              pbItem.vector[0] = (float)(vec0_x[pbIdx]) / 4;
              pbItem.vector[1] = (float)(vec0_y[pbIdx]) / 4;
              pbItem.color = statSource.getStatisticsType(7)->colorRange->getColor(ref0-iPOC);	// Color vector according to referecen idx
              curPOCStats[7].append(pbItem);
            }

            // Add motion vector 1 (ID 8)
            if (ref1 != -1)
            {
              pbItem.vector[0] = (float)(vec1_x[pbIdx]) / 4;
              pbItem.vector[1] = (float)(vec1_y[pbIdx]) / 4;
              pbItem.color = statSource.getStatisticsType(8)->colorRange->getColor(ref1-iPOC);	// Color vector according to referecen idx
              curPOCStats[8].append(pbItem);
            }

          }
        }
        else if (predMode == 0)
        {
          // Get index for this xy position in the intraDir array
          int intraDirIdx = (cbPosY / intraDir_infoUnit_size) * widthInIntraDirUnits + (cbPosX / intraDir_infoUnit_size);

          // For setting the vector
          StatisticsItem intraDirVec;
          intraDirVec.positionRect = anItem.positionRect;
          intraDirVec.type = arrowType;

          // Set Intra prediction direction Luma (ID 9)
          int intraDirLuma = intraDirY[intraDirIdx];
          if (intraDirLuma <= 34)
          {
            anItem.rawValues[0] = intraDirLuma;
            anItem.color = statSource.getStatisticsType(9)->colorRange->getColor(intraDirLuma);
            curPOCStats[9].append(anItem);

            if (intraDirLuma >= 2)
            {
              // Set Intra prediction direction Luma (ID 9) as vector
              intraDirVec.vector[0] = (float)vectorTable[intraDirLuma][0] * VECTOR_SCALING;
              intraDirVec.vector[1] = (float)vectorTable[intraDirLuma][1] * VECTOR_SCALING;
              intraDirVec.color = QColor(0, 0, 0);
              curPOCStats[9].append(intraDirVec);
            }
          }

          // Set Intra prediction direction Chroma (ID 10)
          int intraDirChroma = intraDirC[intraDirIdx];
          if (intraDirChroma <= 34)
          {
            anItem.rawValues[0] = intraDirChroma;
            anItem.color = statSource.getStatisticsType(10)->colorRange->getColor(intraDirChroma);
            curPOCStats[10].append(anItem);

            if (intraDirChroma >= 2)
            {
              // Set Intra prediction direction Chroma (ID 10) as vector
              intraDirVec.vector[0] = (float)vectorTable[intraDirChroma][0] * VECTOR_SCALING;
              intraDirVec.vector[1] = (float)vectorTable[intraDirChroma][1] * VECTOR_SCALING;
              intraDirVec.color = QColor(0, 0, 0);
              curPOCStats[10].append(intraDirVec);
            }
          }
        }

        // Walk into the TU tree
        int tuIdx = (cbPosY / tuInfo_unit_size) * widthInTUInfoUnits + (cbPosX / tuInfo_unit_size);
        cacheStatistics_TUTree_recursive(tuInfo, widthInTUInfoUnits, tuInfo_unit_size, iPOC, tuIdx, cbSizePix / tuInfo_unit_size, 0);
      }
    }
  }

  // Delete all temporary array
  delete cbInfoArr;
  cbInfoArr = NULL;

  delete refPOC0; refPOC0 = NULL;
  delete refPOC1;	refPOC1 = NULL;
  delete vec0_x;	vec0_x  = NULL;
  delete vec0_y;	vec0_y  = NULL;
  delete vec1_x;	vec1_x  = NULL;
  delete vec1_y;	vec1_y  = NULL;

  // The cache now contains the statistics for iPOC
  statsCacheCurPOC = iPOC;
}

void playlistItemHEVCFile::getPBSubPosition(int partMode, int cbSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH)
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
 * \param tuUnitSizePix: The size of ont TU unit in pixels
 * \param iPOC: The current POC
 * \param tuIdx: The top left index of the currently handled TU in tuInfo
 * \param tuWidth_units: The WIdth of the TU in units
 * \param trDepth: The current transform tree depth
*/
void playlistItemHEVCFile::cacheStatistics_TUTree_recursive(uint8_t *tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int tuWidth_units, int trDepth)
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
    StatisticsItem tuDepth;
    int tuWidth = tuWidth_units * tuUnitSizePix;
    int posX_units = tuIdx % tuInfoWidth;
    int posY_units = tuIdx / tuInfoWidth;
    tuDepth.positionRect = QRect(posX_units * tuUnitSizePix, posY_units * tuUnitSizePix, tuWidth, tuWidth);
    tuDepth.type = blockType;
    tuDepth.rawValues[0] = trDepth;
    tuDepth.color = statSource.getStatisticsType(11)->colorRange->getColor(trDepth);
    curPOCStats[11].append(tuDepth);
  }
}

/* Convert the de265_chroma format to a YUVCPixelFormatType and set it.
*/
void playlistItemHEVCFile::setDe265ChromaMode(const de265_image *img)
{
  de265_chroma cMode = de265_get_chroma_format(img);
  int nrBitsC0 = de265_get_bits_per_pixel(img, 0);
  if (cMode == de265_chroma_mono && nrBitsC0 == 8)
    yuvVideo.setSrcPixelFormatByName("4:0:0 8-bit");
  else if (cMode == de265_chroma_420 && nrBitsC0 == 8)
    yuvVideo.setSrcPixelFormatByName("4:2:0 Y'CbCr 8-bit planar");
  else if (cMode == de265_chroma_420 && nrBitsC0 == 10)
    yuvVideo.setSrcPixelFormatByName("4:2:0 Y'CbCr 10-bit LE planar");
  else if (cMode == de265_chroma_422 && nrBitsC0 == 8)
    yuvVideo.setSrcPixelFormatByName("4:2:2 Y'CbCr 8-bit planar");
  else if (cMode == de265_chroma_422 && nrBitsC0 == 10)
    yuvVideo.setSrcPixelFormatByName("4:2:2 10-bit packed 'v210'");
  else if (cMode == de265_chroma_444 && nrBitsC0 == 8)
    yuvVideo.setSrcPixelFormatByName("4:4:4 Y'CbCr 8-bit planar");
  else if (cMode == de265_chroma_444 && nrBitsC0 == 10)
    yuvVideo.setSrcPixelFormatByName("4:4:4 Y'CbCr 10-bit LE planar");
  else if (cMode == de265_chroma_444 && nrBitsC0 == 12)
    yuvVideo.setSrcPixelFormatByName("4:4:4 Y'CbCr 12-bit LE planar");
  else if (cMode == de265_chroma_444 && nrBitsC0 == 16)
    yuvVideo.setSrcPixelFormatByName("4:4:4 Y'CbCr 16-bit LE planar");
}

void playlistItemHEVCFile::fillStatisticList()
{
  if (!internalsSupported)
    return;

  StatisticsType sliceIdx(0, "Slice Index", colorRangeType, 0, QColor(0, 0, 0), 10, QColor(255,0,0));
  statSource.statsTypeList.append(sliceIdx);

  StatisticsType partSize(1, "Part Size", "jet", 0, 7);
  partSize.valMap.insert(0, "PART_2Nx2N");
  partSize.valMap.insert(1, "PART_2NxN");
  partSize.valMap.insert(2, "PART_Nx2N");
  partSize.valMap.insert(3, "PART_NxN");
  partSize.valMap.insert(4, "PART_2NxnU");
  partSize.valMap.insert(5, "PART_2NxnD");
  partSize.valMap.insert(6, "PART_nLx2N");
  partSize.valMap.insert(7, "PART_nRx2N");
  statSource.statsTypeList.append(partSize);

  StatisticsType predMode(2, "Pred Mode", "jet", 0, 2);
  predMode.valMap.insert(0, "INTRA");
  predMode.valMap.insert(1, "INTER");
  predMode.valMap.insert(2, "SKIP");
  statSource.statsTypeList.append(predMode);

  StatisticsType pcmFlag(3, "PCM flag", colorRangeType, 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.statsTypeList.append(pcmFlag);

  StatisticsType transQuantBypass(4, "Transquant Bypass Flag", colorRangeType, 0, QColor(0, 0, 0), 1, QColor(255,0,0));
  statSource.statsTypeList.append(transQuantBypass);

  StatisticsType refIdx0(5, "Ref POC 0", "col3_bblg", -16, 16);
  statSource.statsTypeList.append(refIdx0);

  StatisticsType refIdx1(6, "Ref POC 1", "col3_bblg", -16, 16);
  statSource.statsTypeList.append(refIdx1);

  StatisticsType motionVec0(7, "Motion Vector 0", vectorType);
  motionVec0.colorRange = new DefaultColorRange("col3_bblg", -16, 16);
  statSource.statsTypeList.append(motionVec0);

  StatisticsType motionVec1(8, "Motion Vector 1", vectorType);
  motionVec1.colorRange = new DefaultColorRange("col3_bblg", -16, 16);
  statSource.statsTypeList.append(motionVec1);

  StatisticsType intraDirY(9, "Intra Dir Luma", "jet", 0, 34);
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
  statSource.statsTypeList.append(intraDirY);

  StatisticsType intraDirC(10, "Intra Dir Chroma", "jet", 0, 34);
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
  statSource.statsTypeList.append(intraDirC);

  StatisticsType transformDepth(11, "Transform Depth", colorRangeType, 0, QColor(0, 0, 0), 3, QColor(0,255,0));
  statSource.statsTypeList.append(transformDepth);
}

void playlistItemHEVCFile::loadStatisticToCache(int frameIdx, int typeIdx)
{
  Q_UNUSED(typeIdx);
  DEBUG_HEVC("Request statistics type %d for frame %d", typeIdx, frameIdx);

  if (!internalsSupported)
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
    currentOutputBufferFrameIndex ++;

    loadYUVData(frameIdx);
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

ValuePairListSets playlistItemHEVCFile::getPixelValues(QPoint pixelPos) 
{ 
  ValuePairListSets newSet;
  
  newSet.append("YUV", yuvVideo.getPixelValues(pixelPos));
  if (internalsSupported && retrieveStatistics)
    newSet.append("Stats", statSource.getValuesAt(pixelPos));

  return newSet;
}