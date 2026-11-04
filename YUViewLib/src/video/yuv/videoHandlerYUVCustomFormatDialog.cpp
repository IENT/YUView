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

#include <common/Functions.h>

namespace video::yuv
{

videoHandlerYUVCustomFormatDialog::videoHandlerYUVCustomFormatDialog(
  const PixelFormatYUV &yuvFormat)
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
    const auto idx = vectorIndexOf(BitDepthList, yuvFormat.getBitsPerSample());
    this->ui.comboBoxBitDepth->setCurrentIndex(idx ? static_cast<int>(*idx) : 0);
    this->ui.comboBoxEndianness->setEnabled(idx.has_value());
  }

  // Endianness
  this->ui.comboBoxEndianness->setCurrentIndex(yuvFormat.getEndianness() == Endianness::Big ? 0
                                                                                            : 1);

  // Chroma offsets
  this->ui.comboBoxChromaOffsetX->setCurrentIndex(yuvFormat.getChromaOffset().x);
  this->ui.comboBoxChromaOffsetY->setCurrentIndex(yuvFormat.getChromaOffset().y);

  // Plane order
  this->ui.comboBoxPlaneOrder->addItems(functions::toQStringList(PlaneOrderMapper.getNames()));

  if (const auto planarFormat = yuvFormat.getPlanarFormat())
  {
    this->ui.groupBoxPlanar->setChecked(true);
    this->ui.comboBoxPlaneOrder->setCurrentIndex(
      int(PlaneOrderMapper.indexOf(planarFormat->planeOrder)));

    this->ui.checkBoxUVInterleaved->setChecked(planarFormat->uvPlanesInterleaved);
    this->ui.checkBoxBytePackingPYUV->setChecked(planarFormat->bytePacking);

    this->enablePossibleInterleavedAndPackingModeCheckboxes();
  }
  else if (const auto packedFormat = yuvFormat.getPackedFormat())
  {
    this->ui.groupBoxPacked->setChecked(true);
    auto supportedPackingFormats = getSupportedPackingFormats(packedFormat->subsampling);
    if (const auto idx = vectorIndexOf(supportedPackingFormats, packedFormat->packingOrder))
      this->ui.comboBoxPackingOrder->setCurrentIndex(static_cast<int>(*idx));
    this->ui.checkBoxBytePacking->setChecked(packedFormat->bytePacking);
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
      QString::fromStdString(std::string(PackingOrderMapper.getName(packing))));

  bool packedSupported = (packingTypes.size() != 0);
  if (!packedSupported)
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

  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

void videoHandlerYUVCustomFormatDialog::on_groupBoxPlanar_toggled(bool checked)
{
  if (!checked && !this->ui.groupBoxPacked->isEnabled())
    // If a packed format is not supported, do not allow the user to activate this
    this->ui.groupBoxPlanar->setChecked(true);
  else
    this->ui.groupBoxPacked->setChecked(!checked);

  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

void videoHandlerYUVCustomFormatDialog::on_groupBoxPacked_toggled(bool checked)
{
  this->ui.groupBoxPlanar->setChecked(!checked);
  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

PixelFormatYUV videoHandlerYUVCustomFormatDialog::getSelectedYUVFormat() const
{
  const auto subsamplingIndex = this->ui.comboBoxChromaSubsampling->currentIndex();
  if (subsamplingIndex < 0)
    return {};
  const auto subsampling = SubsamplingMapper.getValueAt(static_cast<std::size_t>(subsamplingIndex));
  if (!subsampling)
    return {};

  const auto bitDepthIndex = this->ui.comboBoxBitDepth->currentIndex();
  if (bitDepthIndex < 0 || bitDepthIndex >= int(BitDepthList.size()))
    return {};
  const auto bitsPerSample = BitDepthList.at(unsigned(bitDepthIndex));

  const auto endianness =
    (this->ui.comboBoxEndianness->currentIndex() == 0 ? Endianness::Big : Endianness::Little);
  const auto chromaOffset = Offset({this->ui.comboBoxChromaOffsetX->currentIndex(),
                                    this->ui.comboBoxChromaOffsetY->currentIndex()});

  const auto isPlanar = (this->ui.groupBoxPlanar->isChecked());
  if (isPlanar)
  {
    const auto planeOrderIndex = this->ui.comboBoxPlaneOrder->currentIndex();
    if (planeOrderIndex < 0)
      return {};
    const auto planeOrder = PlaneOrderMapper.getValueAt(static_cast<std::size_t>(planeOrderIndex));
    if (!planeOrder)
      return {};

    const auto uvInterleaved = this->ui.checkBoxUVInterleaved->isChecked();
    const auto bytePacking   = this->ui.checkBoxBytePackingPYUV->isChecked();

    return PlanarPixelFormat({*subsampling,
                              bitsPerSample,
                              *planeOrder,
                              endianness,
                              chromaOffset,
                              uvInterleaved,
                              bytePacking});
  }
  else
  {
    const auto supportedPackingFormats = getSupportedPackingFormats(*subsampling);
    const auto packingOrderIndex       = this->ui.comboBoxPackingOrder->currentIndex();
    if (packingOrderIndex < 0 || packingOrderIndex >= int(supportedPackingFormats.size()))
      return {};

    const auto packingOrder = supportedPackingFormats.at(packingOrderIndex);
    const auto bytePacking  = (this->ui.checkBoxBytePacking->isChecked());

    return PackedPixelFormat(
      {*subsampling, bitsPerSample, packingOrder, endianness, chromaOffset, bytePacking});
  }
}

void videoHandlerYUVCustomFormatDialog::on_comboBoxBitDepth_currentIndexChanged(int idx)
{
  // Endianness only makes sense when the bit depth is > 8bit.
  const bool bitDepth8 = (idx == 0);
  this->ui.comboBoxEndianness->setEnabled(!bitDepth8);

  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

void videoHandlerYUVCustomFormatDialog::on_comboBoxEndianness_currentIndexChanged(int)
{
  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

void videoHandlerYUVCustomFormatDialog::on_comboBoxPlaneOrder_currentIndexChanged(int)
{
  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

void videoHandlerYUVCustomFormatDialog::on_checkBoxUVInterleaved_checkStateChanged(Qt::CheckState)
{
  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

void videoHandlerYUVCustomFormatDialog::on_checkBoxBytePackingPYUV_checkStateChanged(Qt::CheckState)
{
  this->enablePossibleInterleavedAndPackingModeCheckboxes();
}

void videoHandlerYUVCustomFormatDialog::enablePossibleInterleavedAndPackingModeCheckboxes()
{
  auto currentPixelFormat = this->getSelectedYUVFormat().getPlanarFormat();
  if (!currentPixelFormat)
    return;

  if (!this->ui.checkBoxUVInterleaved->isChecked())
  {
    auto currentFormatWithUVInterleaved                = *currentPixelFormat;
    currentFormatWithUVInterleaved.uvPlanesInterleaved = true;
    const auto canBeActivated = PixelFormatYUV(currentFormatWithUVInterleaved).isValid();
    this->ui.checkBoxUVInterleaved->setEnabled(canBeActivated);
  }

  if (!this->ui.checkBoxBytePackingPYUV->isChecked())
  {
    auto currentFormatWithBytePacking        = *currentPixelFormat;
    currentFormatWithBytePacking.bytePacking = true;
    const auto canBeActivated = PixelFormatYUV(currentFormatWithBytePacking).isValid();
    this->ui.checkBoxBytePackingPYUV->setEnabled(canBeActivated);
  }
}

} // namespace video::yuv
