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

#include "frameHandler.h"

#include <QPainter>
#include "playlistItem.h"

// ------ Initialize the static list of frame size presets ----------

class frameHandler::frameSizePresetList
{
public:
  // Constructor. Fill the names and sizes lists
  frameSizePresetList();
  // Get all presets in a displayable format ("Name (xxx,yyy)")
  QStringList getFormattedNames() const;
  // Return the index of a certain size (0 (Custom Size) if not found)
  int findSize(const QSize &size) { int idx = sizes.indexOf(size); return (idx == -1) ? 0 : idx; }
  // Get the size with the given index.
  QSize getSize(int index) { return sizes[index]; }
private:
  QList<QString> names;
  QList<QSize>   sizes;
};

frameHandler::frameSizePresetList::frameSizePresetList()
{
  names << "Custom Size" << "QCIF" << "QVGA" << "WQVGA" << "CIF" << "VGA" << "WVGA" << "4CIF" << "ITU R.BT601" << "720i/p" << "1080i/p" << "4k" << "XGA" << "XGA+";
  sizes << QSize(-1,-1) << QSize(176,144) << QSize(320, 240) << QSize(416, 240) << QSize(352, 288) << QSize(640, 480) << QSize(832, 480) << QSize(704, 576) << QSize(720, 576) << QSize(1280, 720) << QSize(1920, 1080) << QSize(3840, 2160) << QSize(1024, 768) << QSize(1280, 960);
}

/* Get all the names of the preset frame sizes in the form "Name (xxx,yyy)" in a QStringList.
 * This can be used to directly fill the combo box.
 */
QStringList frameHandler::frameSizePresetList::getFormattedNames() const
{
  QStringList presetList;
  presetList.append("Custom Size");

  for (int i = 1; i < names.count(); i++)
  {
    QString str = QString("%1 (%2,%3)").arg(names[i]).arg(sizes[i].width()).arg(sizes[i].height());
    presetList.append(str);
  }

  return presetList;
}

frameHandler::frameSizePresetList frameHandler::presetFrameSizes;

// ---------------- frameHandler ---------------------------------

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define FRAMEHANDLER_DEBUG_LOADING 0
#if FRAMEHANDLER_DEBUG_LOADING && !NDEBUG
#define DEBUG_FRAME qDebug
#else
#define DEBUG_FRAME(fmt,...) ((void)0)
#endif

frameHandler::frameHandler()
{
}

QLayout *frameHandler::createFrameHandlerControls(bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  ui.setupUi();

  // Set default values
  ui.widthSpinBox->setMaximum(100000);
  ui.widthSpinBox->setValue(frameSize.width());
  ui.widthSpinBox->setEnabled(!isSizeFixed);
  ui.heightSpinBox->setMaximum(100000);
  ui.heightSpinBox->setValue(frameSize.height());
  ui.heightSpinBox->setEnabled(!isSizeFixed);
  ui.frameSizeComboBox->addItems(presetFrameSizes.getFormattedNames());
  int idx = presetFrameSizes.findSize(frameSize);
  ui.frameSizeComboBox->setCurrentIndex(idx);
  ui.frameSizeComboBox->setEnabled(!isSizeFixed);

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui.widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &frameHandler::slotVideoControlChanged);
  connect(ui.heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &frameHandler::slotVideoControlChanged);
  connect(ui.frameSizeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &frameHandler::slotVideoControlChanged);

  return ui.frameHandlerLayout;
}

void frameHandler::setFrameSize(const QSize &newSize)
{
  if (newSize != frameSize)
  {
    // Set the new size
    DEBUG_FRAME("frameHandler::setFrameSize %dx%d", newSize.width(), newSize.height());
    frameSize = newSize;
  }
}

bool frameHandler::loadCurrentImageFromFile(const QString &filePath)
{
  // Load the image and return if loading was successful
  currentImage = QImage(filePath);
  setFrameSize(currentImage.size());

  return (!currentImage.isNull());
}

void frameHandler::slotVideoControlChanged()
{
  // Update the controls and get the new selected size
  QSize newSize = getNewSizeFromControls();
  DEBUG_FRAME("frameHandler::slotVideoControlChanged new size %dx%d", newSize.width(), newSize.height());

  if (newSize != frameSize && newSize != QSize(-1,-1))
  {
    // Set the new size and update the controls.
    setFrameSize(newSize);
    // The frame size changed. We need to redraw/re-cache.
    emit signalHandlerChanged(true, RECACHE_CLEAR);
  }
}

QSize frameHandler::getNewSizeFromControls()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();

  QSize newSize;
  if (sender == ui.widthSpinBox || sender == ui.heightSpinBox)
  {
    newSize = QSize(ui.widthSpinBox->value(), ui.heightSpinBox->value());
    if (newSize != frameSize)
    {
      // Set the comboBox index without causing another signal to be emitted.
      const QSignalBlocker blocker(ui.frameSizeComboBox);
      int idx = presetFrameSizes.findSize(newSize);
      ui.frameSizeComboBox->setCurrentIndex(idx);
    }
  }
  else if (sender == ui.frameSizeComboBox)
  {
    newSize = presetFrameSizes.getSize(ui.frameSizeComboBox->currentIndex());
    
    // Set the width/height spin boxes without emitting another signal.
    const QSignalBlocker blocker1(ui.widthSpinBox);
    const QSignalBlocker blocker2(ui.heightSpinBox);
    ui.widthSpinBox->setValue(newSize.width());
    ui.heightSpinBox->setValue(newSize.height());
  }
  return newSize;
}

void frameHandler::drawFrame(QPainter *painter, double zoomFactor, bool drawRawValues)
{
  // Create the video QRect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(frameSize * zoomFactor);
  videoRect.moveCenter(QPoint(0,0));

  // Draw the current image (currentFrame)
  painter->drawImage(videoRect, currentImage);

  if (drawRawValues && zoomFactor >= SPLITVIEW_DRAW_VALUES_ZOOMFACTOR)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, 0, videoRect, zoomFactor);
  }
}

void frameHandler::drawPixelValues(QPainter *painter, const int frameIdx, const QRect &videoRect, const double zoomFactor, frameHandler *item2, const bool markDifference, const int frameIdxItem1)
{
  // Draw the pixel values onto the pixels
  Q_UNUSED(frameIdx);
  Q_UNUSED(frameIdxItem1);

  // TODO: Does this also work for sequences with width/height non divisible by 2? Not sure about that.
    
  // First determine which pixels from this item are actually visible, because we only have to draw the pixel values
  // of the pixels that are actually visible
  QRect viewport = painter->viewport();
  QTransform worldTransform = painter->worldTransform();
    
  int xMin = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width())) / zoomFactor;
  int yMax = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height())) / zoomFactor;

  // Clip the min/max visible pixel values to the size of the item (no pixels outside of the
  // item have to be labeled)
  xMin = clip(xMin, 0, frameSize.width()-1);
  yMin = clip(yMin, 0, frameSize.height()-1);
  xMax = clip(xMax, 0, frameSize.width()-1);
  yMax = clip(yMax, 0, frameSize.height()-1);

  // The center point of the pixel (0,0).
  QPoint centerPointZero = (QPoint(-frameSize.width(), -frameSize.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor)) / 2;
  // This QRect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize(QSize(zoomFactor, zoomFactor));
  for (int x = xMin; x <= xMax; x++)
  {
    for (int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);
     
      // Get the text to show
      bool drawWhite = false;
      QRgb pixVal;
      QString valText;
      if (item2 != nullptr)
      {
        QRgb pixel1 = getPixelVal(x, y);
        QRgb pixel2 = item2->getPixelVal(x, y);

        int dR = int(qRed(pixel1)) - int(qRed(pixel2));
        int dG = int(qGreen(pixel1)) - int(qGreen(pixel2));
        int dB = int(qBlue(pixel1)) - int(qBlue(pixel2));

        int r = clip(128 + dR, 0, 255);
        int g = clip(128 + dG, 0, 255);
        int b = clip(128 + dB, 0, 255);

        pixVal = qRgb(r,g,b);

        if (markDifference)
          drawWhite = (dR == 0 && dG == 0 && dB == 0);
        else
          drawWhite = (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128);
        valText = QString("R%1\nG%2\nB%3").arg(dR).arg(dG).arg(dB);
      }
      else
      {
        pixVal = getPixelVal(x, y);
        drawWhite = (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128);
        valText = QString("R%1\nG%2\nB%3").arg(qRed(pixVal)).arg(qGreen(pixVal)).arg(qBlue(pixVal));
      }
      
      painter->setPen(drawWhite ? Qt::white : Qt::black);
      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QImage frameHandler::calculateDifference(frameHandler *item2, const int frameIdxItem0, const int frameIdxItem1, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference)
{
  Q_UNUSED(frameIdxItem0);
  Q_UNUSED(frameIdxItem1);

  int width  = qMin(frameSize.width(), item2->frameSize.width());
  int height = qMin(frameSize.height(), item2->frameSize.height());

  QImage diffImg(width, height, platformImageFormat());

  // Also calculate the MSE while we're at it (R,G,B)
  int64_t mseAdd[3] = {0, 0, 0};

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      QRgb pixel1 = getPixelVal(x, y);
      QRgb pixel2 = item2->getPixelVal(x, y);

      int dR = int(qRed(pixel1)) - int(qRed(pixel2));
      int dG = int(qGreen(pixel1)) - int(qGreen(pixel2));
      int dB = int(qBlue(pixel1)) - int(qBlue(pixel2));

      int r, g, b;
      if (markDifference)
      {
        r = (dR != 0) ? 255 : 0;
        g = (dG != 0) ? 255 : 0;
        b = (dB != 0) ? 255 : 0;
      }
      else if (amplificationFactor != 1)
      {  
        r = clip(128 + dR * amplificationFactor, 0, 255);
        g = clip(128 + dG * amplificationFactor, 0, 255);
        b = clip(128 + dB * amplificationFactor, 0, 255);
      }
      else
      {  
        r = clip(128 + dR, 0, 255);
        g = clip(128 + dG, 0, 255);
        b = clip(128 + dB, 0, 255);
      }
      
      mseAdd[0] += dR * dR;
      mseAdd[1] += dG * dG;
      mseAdd[2] += dB * dB;

      QRgb val = qRgb(r, g, b);
      diffImg.setPixel(x, y, val);
    }
  }

  differenceInfoList.append(infoItem("Difference Type","RGB"));
  
  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  differenceInfoList.append(infoItem("MSE R",QString("%1").arg(mse[0])));
  differenceInfoList.append(infoItem("MSE G",QString("%1").arg(mse[1])));
  differenceInfoList.append(infoItem("MSE B",QString("%1").arg(mse[2])));
  differenceInfoList.append(infoItem("MSE All",QString("%1").arg(mse[3])));

  return diffImg;
}

bool frameHandler::isPixelDark(const QPoint &pixelPos)
{
  QRgb pixVal = getPixelVal(pixelPos);
  return (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128);
}

QStringPairList frameHandler::getPixelValues(const QPoint &pixelPos, int frameIdx, frameHandler *item2, const int frameIdx1)
{
  Q_UNUSED(frameIdx);
  Q_UNUSED(frameIdx1); 

  int width = (item2) ? qMin(frameSize.width(), item2->frameSize.width()) : frameSize.width();
  int height = (item2) ? qMin(frameSize.height(), item2->frameSize.height()) : frameSize.height();

  if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
    return QStringPairList();

  // Is the format (of both items) valid?
  if (!isFormatValid())
    return QStringPairList();
  if (item2 && !item2->isFormatValid())
    return QStringPairList();

  // Get the RGB values from the image
  QStringPairList values;

  if (item2)
  {
    // There is a second item. Return the difference values.
    QRgb pixel1 = getPixelVal(pixelPos);
    QRgb pixel2 = item2->getPixelVal(pixelPos);

    int r = int(qRed(pixel1)) - int(qRed(pixel2));
    int g = int(qGreen(pixel1)) - int(qGreen(pixel2));
    int b = int(qBlue(pixel1)) - int(qBlue(pixel2));

    values.append(QStringPair("R", QString::number(r)));
    values.append(QStringPair("G", QString::number(g)));
    values.append(QStringPair("B", QString::number(b)));
  }
  else
  {
    // No second item. Return the RGB values of this item.
    QRgb val = getPixelVal(pixelPos);
    values.append(QStringPair("R", QString::number(qRed(val))));
    values.append(QStringPair("G", QString::number(qGreen(val))));
    values.append(QStringPair("B", QString::number(qBlue(val))));
  }

  return values;
}
