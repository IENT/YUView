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

#ifndef PLAYLISTITEMYUVFILE_H
#define PLAYLISTITEMYUVFILE_H

#include "playlistitem.h"
#include "typedef.h"
#include "fileSource.h"
#include <QString>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include "yuvsource.h"

class playlistItemYUVFile :
  public playlistItem,
  public fileSource,
  public yuvSource
{
  Q_OBJECT

public:
  playlistItemYUVFile(QString yuvFilePath, bool tryFormatGuess=true);
  ~playlistItemYUVFile();

  // Overload from playlistItem. Save the yuv file item to playlist.
  virtual void savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir);

  bool isIndexedByFrame() { return true; }
  virtual indexRange getFrameIndexRange() { return indexRange(startFrame, endFrame); }

  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor);

  // Return the info title and info list to be shown in the fileInfo groupBox.
  // Get these from the display object.
  virtual QString getInfoTitel() { return "YUVFile Info"; }
  virtual QList<infoItem> getInfoList();

  virtual QString getPropertiesTitle() { return "YUV File Properties"; }

  // ------ Overload from playlistItem

  // a yuv file provides video but no statistics
  virtual bool providesVideo() { return true; }

  virtual double getFrameRate() { return frameRate; }
  virtual QSize  getVideoSize() { return getYUVFrameSize(); }

  // ------ Overload from yuvSource

  // Create a new playlistItemYUVFile from the playlist file entry. Return NULL if parsing failed.
  static playlistItemYUVFile *newplaylistItemYUVFile(QDomElement stringElement, QString playlistFilePath);

protected:

  // Try to get and set the format from file name. If after calling this function isFormatValid()
  // returns false then it failed.
  void setFormatFromFileName();
  
  // Try to guess and set the format (frameSize/srcPixelFormat) from the file itself.
  // If after calling this function isFormatValid() returns false then it failed.
  void setFormatFromCorrelation();

  double frameRate;
  int startFrame, endFrame, sampling;

private:
  QSpinBox  *widthSpinBox;
  QSpinBox  *heightSpinBox;
  QSpinBox  *startSpinBox;
  QSpinBox  *endSpinBox;
  QDoubleSpinBox  *rateSpinBox;
  QSpinBox  *samplingSpinBox;
  QComboBox *frameSizeComboBox;
  QComboBox *yuvFileFormatComboBox;
  QComboBox *colorComponentsComboBox;
  QComboBox *chromaInterpolationComboBox;
  QComboBox *colorConversionComboBox;
  QSpinBox  *lumaScaleSpinBox;
  QSpinBox  *lumaOffsetSpinBox;
  QCheckBox *lumaInvertCheckBox;
  QSpinBox  *chromaScaleSpinBox;
  QSpinBox  *chromaOffsetSpinBox;
  QCheckBox *chromaInvertCheckBox;

  // A list of all frame size presets. Only used privately in this class. Defined in the .cpp file.
  class frameSizePresetList
  {
  public:
    // Constructor. Fill the names and sizes lists
    frameSizePresetList();
    // Get all presets in a displayable format ("Name (xxx,yyy)")
    QStringList getFormatedNames();
    // Return the index of a certain size (0 (Custom Size) if not found)
    int findSize(QSize size) { int idx = sizes.indexOf( size ); return (idx == -1) ? 0 : idx; }
    // Get the size with the given index.
    QSize getSize(int index) { return sizes[index]; }
  private:
    QList<QString> names;
    QList<QSize>   sizes;
  };

  int getNumberFrames();

  // The (static) list of frame size presets (like CIF, QCIF, 4k ...)
  static frameSizePresetList presetFrameSizes;
  QStringList getFrameSizePresetNames();

  // Overload from playlistItem. Create a properties widget custom to the YUVFile
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget();

  // TODO: Remove. Temporary drawing static variable
  static int randomColorStat;
  int randomColor;

  // --- Drawing: We keep a buffer of the current frame as RGB image so wen don't have to ´convert
  // it from YUV every time a draw event is triggered. But it currentFrameIdx is not identical to 
  // the requested frame in the draw event we will have to update currentFrame.
  // We also keep a temporary byte array for one frame in YUV format to save the overhead of
  // creating/resizing it every time we want to convert an image.
  QPixmap    currentFrame;     
  int        currentFrameIdx;
  QByteArray tempYUVFrameBuffer;

private slots:
  // All the valueChanged() signals from the controls are connected here.
  void slotControlChanged();
};

#endif