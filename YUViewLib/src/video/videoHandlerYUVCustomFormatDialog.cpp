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

#include "videoHandlerYUVCustomFormatDialog.h"

using namespace YUV_Internals;

videoHandlerYUVCustomFormatDialog::videoHandlerYUVCustomFormatDialog(const yuvPixelFormat &yuvFormat)
{
  setupUi(this);

  // Fill the comboBoxes and set all values correctly from the given yuvFormat

  // Chroma subsampling
  comboBoxChromaSubsampling->addItems(subsamplingTextList);
  if (yuvFormat.subsampling != Subsampling::UNKNOWN)
  {
    auto subsamplingText = subsamplingToString(yuvFormat.subsampling);
    comboBoxChromaSubsampling->setCurrentText(subsamplingText);
    // The Q_Object auto connection is performed later so call the slot manually.
    // This will fill comboBoxPackingOrder
    on_comboBoxChromaSubsampling_currentIndexChanged(comboBoxChromaSubsampling->currentIndex());
  }

  // Bit depth
  for (auto bitDepth : bitDepthList)
    comboBoxBitDepth->addItem(QString("%1").arg(bitDepth));
  {
    auto idx = bitDepthList.indexOf(yuvFormat.bitsPerSample);
    if (idx == -1)
      idx = 0;
    comboBoxBitDepth->setCurrentIndex(idx);
    comboBoxEndianness->setEnabled(idx != 0);
  }

  // Endianness
  comboBoxEndianness->setCurrentIndex(yuvFormat.bigEndian ? 0 : 1);

  // Chroma offsets
  comboBoxChromaOffsetX->setCurrentIndex(yuvFormat.chromaOffset[0]);
  comboBoxChromaOffsetY->setCurrentIndex(yuvFormat.chromaOffset[1]);

  // Plane order
  comboBoxPlaneOrder->addItems(planeOrderNameList);

  if (yuvFormat.planar)
  {
    // Set the plane order
    groupBoxPlanar->setChecked(true);
    const auto idx = planeOrderList.indexOf(yuvFormat.planeOrder);
    if (idx == -1)
      comboBoxPlaneOrder->setCurrentIndex(idx);
    // Set UV(A) interleaved
    checkBoxUVInterleaved->setChecked(yuvFormat.uvInterleaved);
  }
  else
  {
    // Set the packing order
    groupBoxPacked->setChecked(true);
    const auto supportedPackingFormats = getSupportedPackingFormats(yuvFormat.subsampling);
    const auto idx = supportedPackingFormats.indexOf(yuvFormat.packingOrder);
    if (idx != -1)
      comboBoxPackingOrder->setCurrentIndex(idx);
  }
}

void videoHandlerYUVCustomFormatDialog::on_comboBoxChromaSubsampling_currentIndexChanged(int idx)
{
  // What packing types are supported?
  Subsampling subsampling = static_cast<Subsampling>(idx);
  QList<PackingOrder> packingTypes = getSupportedPackingFormats(subsampling);
  comboBoxPackingOrder->clear();
  for (PackingOrder packing : packingTypes)
    comboBoxPackingOrder->addItem(getPackingFormatString(packing));

  bool packedSupported = (packingTypes.count() != 0);
  if (!packedSupported)
    // No packing supported for this subsampling. Select planar.
    groupBoxPlanar->setChecked(true);
  else
    comboBoxPackingOrder->setCurrentIndex(0);

  // What chroma offsets are possible?
  comboBoxChromaOffsetX->clear();
  int maxValsX = getMaxPossibleChromaOffsetValues(true, subsampling);
  if (maxValsX >= 1)
    comboBoxChromaOffsetX->addItems(QStringList() << "0" << "1/2");
  if (maxValsX >= 3)
    comboBoxChromaOffsetX->addItems(QStringList() << "1" << "3/2");
  if (maxValsX >= 7)
    comboBoxChromaOffsetX->addItems(QStringList() << "2" << "5/2" << "3" << "7/2");
  comboBoxChromaOffsetY->clear();
  int maxValsY = getMaxPossibleChromaOffsetValues(false, subsampling);
  if (maxValsY >= 1)
    comboBoxChromaOffsetY->addItems(QStringList() << "0" << "1/2");
  if (maxValsY >= 3)
    comboBoxChromaOffsetY->addItems(QStringList() << "1" << "3/2");
  if (maxValsY >= 7)
    comboBoxChromaOffsetY->addItems(QStringList() << "2" << "5/2" << "3" << "7/2");

  // Disable the combo boxes if there are no chroma components
  bool chromaPresent = (subsampling != Subsampling::YUV_400);
  groupBoxPacked->setEnabled(chromaPresent && packedSupported);
  groupBoxPlanar->setEnabled(chromaPresent);
  comboBoxChromaOffsetX->setEnabled(chromaPresent);
  comboBoxChromaOffsetY->setEnabled(chromaPresent);
}

void videoHandlerYUVCustomFormatDialog::on_groupBoxPlanar_toggled(bool checked)
{ 
  if (!checked && !groupBoxPacked->isEnabled())
    // If a packed format is not supported, do not allow the user to activate this
    groupBoxPlanar->setChecked(true); 
  else 
    groupBoxPacked->setChecked(!checked); 
}

yuvPixelFormat videoHandlerYUVCustomFormatDialog::getYUVFormat() const
{
  // Get all the values from the controls
  yuvPixelFormat format;

  // Set the subsampling format
  int idx = comboBoxChromaSubsampling->currentIndex();
  Q_ASSERT(idx >= 0);
  format.subsampling = subsamplingList[idx];

  // Set the bit depth
  idx = comboBoxBitDepth->currentIndex();
  Q_ASSERT(idx >= 0);
  format.bitsPerSample = bitDepthList[idx];

  // Set the endianness
  format.bigEndian = (comboBoxEndianness->currentIndex() == 0);

  // Set the chroma offset
  format.chromaOffset[0] = comboBoxChromaOffsetX->currentIndex();
  format.chromaOffset[1] = comboBoxChromaOffsetY->currentIndex();

  // Planar or packed format?
  format.planar = (groupBoxPlanar->isChecked());
  if (format.planar)
  {
    idx = comboBoxPlaneOrder->currentIndex();
    Q_ASSERT(idx >= 0);
    format.planeOrder = planeOrderList[idx];
    format.uvInterleaved = checkBoxUVInterleaved->isChecked();
  }
  else
  {
    idx = comboBoxPackingOrder->currentIndex();
    Q_ASSERT(idx >= 0);
    const auto supportedPackingFormats = getSupportedPackingFormats(format.subsampling);
    format.packingOrder = supportedPackingFormats[idx];
    format.bytePacking = (checkBoxBytePacking->isChecked());
  }

  return format;
}

void videoHandlerYUVCustomFormatDialog::on_comboBoxBitDepth_currentIndexChanged(int idx)
{
  // Endianness only makes sense when the bit depth is > 8bit.
  bool bitDepth8 = (idx == 0);
  comboBoxEndianness->setEnabled(!bitDepth8);
}
