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

#include <common/functions.h>

namespace video::yuv
{

videoHandlerYUVCustomFormatDialog::videoHandlerYUVCustomFormatDialog(
    const YUVPixelFormat &yuvFormat)
{
  this->ui.setupUi(this);

  // Fill the comboBoxes and set all values correctly from the given yuvFormat

  // Chroma subsampling
  this->ui.comboBoxChromaSubsampling->addItems(
      functions::toQStringList(SubsamplingMapper.getNames()));
  if (yuvFormat.getSubsampling() != Subsampling::UNKNOWN)
  {
    if (auto index = SubsamplingMapper.indexOf(yuvFormat.getSubsampling()))
    {
      this->ui.comboBoxChromaSubsampling->setCurrentIndex(int(index));
      // The Q_Object auto connection is performed later so call the slot manually.
      // This will fill comboBoxPackingOrder
      this->on_comboBoxChromaSubsampling_currentIndexChanged(
          this->ui.comboBoxChromaSubsampling->currentIndex());
    }
  }

  // Bit depth

  for (auto bitDepth : BitDepthList)
    this->ui.comboBoxBitDepth->addItem(QString("%1").arg(bitDepth));
  {
    auto idx = indexInVec(BitDepthList, yuvFormat.getBitsPerSample());
    if (idx == -1)
      idx = 0;
    this->ui.comboBoxBitDepth->setCurrentIndex(idx);
    this->ui.comboBoxEndianness->setEnabled(idx != 0);
  }

  // Endianness
  this->ui.comboBoxEndianness->setCurrentIndex(yuvFormat.isBigEndian() ? 0 : 1);

  // Chroma offsets
  this->ui.comboBoxChromaOffsetX->setCurrentIndex(yuvFormat.getChromaOffset().x);
  this->ui.comboBoxChromaOffsetY->setCurrentIndex(yuvFormat.getChromaOffset().y);

  // Plane order
  this->ui.comboBoxPlaneOrder->addItems(functions::toQStringList(PlaneOrderMapper.getNames()));

  if (yuvFormat.isPlanar())
  {
    // Set the plane order
    this->ui.groupBoxPlanar->setChecked(true);
    this->ui.comboBoxPlaneOrder->setCurrentIndex(
        int(PlaneOrderMapper.indexOf(yuvFormat.getPlaneOrder())));
    // Set UV(A) interleaved
    this->ui.checkBoxUVInterleaved->setChecked(yuvFormat.isUVInterleaved());
  }
  else
  {
    // Set the packing order
    this->ui.groupBoxPacked->setChecked(true);
    auto supportedPackingFormats = getSupportedPackingFormats(yuvFormat.getSubsampling());
    auto idx                     = indexInVec(supportedPackingFormats, yuvFormat.getPackingOrder());
    if (idx != -1)
      this->ui.comboBoxPackingOrder->setCurrentIndex(idx);
    this->ui.checkBoxBytePacking->setChecked(yuvFormat.isBytePacking());
  }
}

void videoHandlerYUVCustomFormatDialog::on_comboBoxChromaSubsampling_currentIndexChanged(int idx)
{
  // What packing types are supported?
  auto subsampling  = static_cast<Subsampling>(idx);
  auto packingTypes = getSupportedPackingFormats(subsampling);
  this->ui.comboBoxPackingOrder->clear();
  for (auto &packing : packingTypes)
    this->ui.comboBoxPackingOrder->addItem(
        QString::fromStdString(PackingOrderMapper.getName(packing)));

  bool packedSupported = (packingTypes.size() != 0);
  if (!packedSupported)
    // No packing supported for this subsampling. Select planar.
    this->ui.groupBoxPlanar->setChecked(true);
  else
    this->ui.comboBoxPackingOrder->setCurrentIndex(0);

  // What chroma offsets are possible?
  this->ui.comboBoxChromaOffsetX->clear();
  auto maxValsX = getMaxPossibleChromaOffsetValues(true, subsampling);
  if (maxValsX >= 1)
    this->ui.comboBoxChromaOffsetX->addItems(QStringList() << "0"
                                                           << "1/2");
  if (maxValsX >= 3)
    this->ui.comboBoxChromaOffsetX->addItems(QStringList() << "1"
                                                           << "3/2");
  if (maxValsX >= 7)
    this->ui.comboBoxChromaOffsetX->addItems(QStringList() << "2"
                                                           << "5/2"
                                                           << "3"
                                                           << "7/2");
  this->ui.comboBoxChromaOffsetY->clear();
  int maxValsY = getMaxPossibleChromaOffsetValues(false, subsampling);
  if (maxValsY >= 1)
    this->ui.comboBoxChromaOffsetY->addItems(QStringList() << "0"
                                                           << "1/2");
  if (maxValsY >= 3)
    this->ui.comboBoxChromaOffsetY->addItems(QStringList() << "1"
                                                           << "3/2");
  if (maxValsY >= 7)
    this->ui.comboBoxChromaOffsetY->addItems(QStringList() << "2"
                                                           << "5/2"
                                                           << "3"
                                                           << "7/2");

  // Disable the combo boxes if there are no chroma components
  bool chromaPresent = (subsampling != Subsampling::YUV_400);
  this->ui.groupBoxPacked->setEnabled(chromaPresent && packedSupported);
  this->ui.groupBoxPlanar->setEnabled(chromaPresent);
  this->ui.comboBoxChromaOffsetX->setEnabled(chromaPresent);
  this->ui.comboBoxChromaOffsetY->setEnabled(chromaPresent);
}

void videoHandlerYUVCustomFormatDialog::on_groupBoxPlanar_toggled(bool checked)
{
  if (!checked && !this->ui.groupBoxPacked->isEnabled())
    // If a packed format is not supported, do not allow the user to activate this
    this->ui.groupBoxPlanar->setChecked(true);
  else
    this->ui.groupBoxPacked->setChecked(!checked);
}

YUVPixelFormat videoHandlerYUVCustomFormatDialog::getSelectedYUVFormat() const
{
  // Subsampling
  auto idx = this->ui.comboBoxChromaSubsampling->currentIndex();
  if (idx < 0)
    return {};
  auto subsampling = SubsamplingMapper.at(unsigned(idx));
  if (!subsampling)
    return {};

  // Bit depth
  idx = this->ui.comboBoxBitDepth->currentIndex();
  if (idx < 0 || idx >= int(BitDepthList.size()))
    return {};
  auto bitsPerSample = BitDepthList.at(unsigned(idx));

  // Endianness
  auto bigEndian = (this->ui.comboBoxEndianness->currentIndex() == 0);

  // Set the chroma offset
  auto chromaOffset = Offset({this->ui.comboBoxChromaOffsetX->currentIndex(),
                              this->ui.comboBoxChromaOffsetY->currentIndex()});

  auto isPlanar = (this->ui.groupBoxPlanar->isChecked());
  if (isPlanar)
  {
    idx = this->ui.comboBoxPlaneOrder->currentIndex();
    if (idx < 0)
      return {};
    auto planeOrder = PlaneOrderMapper.at(unsigned(idx));
    if (!planeOrder)
      return {};

    auto uvInterleaved = this->ui.checkBoxUVInterleaved->isChecked();

    return YUVPixelFormat(
        *subsampling, bitsPerSample, *planeOrder, bigEndian, chromaOffset, uvInterleaved);
  }
  else
  {
    auto supportedPackingFormats = getSupportedPackingFormats(*subsampling);
    idx                          = this->ui.comboBoxPackingOrder->currentIndex();
    if (idx < 0 || idx >= int(supportedPackingFormats.size()))
      return {};

    auto packingOrder = supportedPackingFormats.at(idx);
    auto bytePacking  = (this->ui.checkBoxBytePacking->isChecked());

    return YUVPixelFormat(
        *subsampling, bitsPerSample, packingOrder, bytePacking, bigEndian, chromaOffset);
  }
}

void videoHandlerYUVCustomFormatDialog::on_comboBoxBitDepth_currentIndexChanged(int idx)
{
  // Endianness only makes sense when the bit depth is > 8bit.
  bool bitDepth8 = (idx == 0);
  this->ui.comboBoxEndianness->setEnabled(!bitDepth8);
}

} // namespace video::yuv
