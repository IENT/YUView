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

#include "hevcDecoderBase.h"

#include <QDir>
#include <QSettings>

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define HEVCDECODERBASE_DEBUG_OUTPUT 0
#if HEVCDECODERBASE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if HEVCDECODERBASE_DEBUG_OUTPUT == 1
#define DEBUG_HEVCDECODERBASE if(!isCachingDecoder) qDebug
#elif HEVCDECODERBASE_DEBUG_OUTPUT == 2
#define DEBUG_HEVCDECODERBASE if(isCachingDecoder) qDebug
#elif HEVCDECODERBASE_DEBUG_OUTPUT == 3
#define DEBUG_HEVCDECODERBASE if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_HEVCDECODERBASE(fmt,...) ((void)0)
#endif

// Conversion from intra prediction mode to vector.
// Coordinates are in x,y with the axes going right and down.
#define VECTOR_SCALING 0.25
const int hevcDecoderBase::vectorTable[35][2] = 
{
  {0,0}, {0,0},
  {32, -32},
  {32, -26}, {32, -21}, {32, -17}, { 32, -13}, { 32,  -9}, { 32, -5}, { 32, -2},
  {32,   0},
  {32,   2}, {32,   5}, {32,   9}, { 32,  13}, { 32,  17}, { 32, 21}, { 32, 26},
  {32,  32},
  {26,  32}, {21,  32}, {17,  32}, { 13,  32}, {  9,  32}, {  5, 32}, {  2, 32},
  {0,   32},
  {-2,  32}, {-5,  32}, {-9,  32}, {-13,  32}, {-17,  32}, {-21, 32}, {-26, 32},
  {-32, 32} 
};

bool hevcDecoderBase::openFile(QString fileName, decoderBase *otherDecoder)
{ 
  hevcDecoderBase *otherHEVCDecoder = dynamic_cast<hevcDecoderBase*>(otherDecoder);
  // Open the file, decode the first frame and return if this was successfull.
  if (otherHEVCDecoder)
    parsingError = !hevcAnnexBFile.openFile(fileName, false, &otherHEVCDecoder->hevcAnnexBFile);
  else
    parsingError = !hevcAnnexBFile.openFile(fileName);
  
  if (!parsingError)
  {
    // Once the annexB file is opened, the frame size and the YUV format is known.
    frameSize = hevcAnnexBFile.getSequenceSizeSamples();
    nrBitsC0 = hevcAnnexBFile.getSequenceBitDepth(Luma);
    pixelFormat = hevcAnnexBFile.getSequenceSubsampling();
  }

  return !parsingError && !decoderError;
}