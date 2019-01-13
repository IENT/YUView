/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef PARSERANNEXB_H
#define PARSERANNEXB_H

#include <QList>
#include <QAbstractItemModel>
#include "videoHandlerYUV.h"
#include "parserBase.h"
#include "fileSourceAnnexBFile.h"

using namespace YUV_Internals;

/* The (abstract) base class for the various types of AnnexB files (AVC, HEVC, JEM) that we can parse.
*/
class parserAnnexB : public parserBase
{
  Q_OBJECT

public:
  parserAnnexB(QObject *parent = nullptr) : parserBase(parent) {};
  virtual ~parserAnnexB() {};

  // How many POC's have been found in the file
  int getNumberPOCs() const { return frameList.size(); }

  // Clear all knowledge about the bitstream.
  void clearData();

  QList<QTreeWidgetItem*> getStreamInfo() Q_DECL_OVERRIDE { return stream_info.getStreamInfo(); }
  unsigned int getNrStreams() Q_DECL_OVERRIDE { return 1; }

  // This function must be overloaded and parse the NAL unit header and whatever the NAL unit may contain.
  // It also adds the unit to the nalUnitList (if it is a parameter set or an RA point).
  // When there are no more NAL units in the file (the file ends), call this function one last time with empty data and a nalID of -1.
  virtual bool parseAndAddNALUnit(int nalID, QByteArray data, parserCommon::TreeItem *parent=nullptr, QUint64Pair nalStartEndPosFile = QUint64Pair(-1,-1), QString *nalTypeName=nullptr) = 0;
  
  // Get some format properties
  virtual double getFramerate() const = 0;
  virtual QSize getSequenceSizeSamples() const = 0;
  virtual yuvPixelFormat getPixelFormat() const = 0;

  // When we want to seek to a specific frame number, this function return the parameter sets that you need
  // to start decoding (without start codes). If file positions were set for the NAL units, the file position 
  // where decoding can begin will also be returned.
  virtual QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) = 0;

  // Look through the random access points and find the closest one before (or equal)
  // the given frameIdx where we can start decoding
  // frameIdx: The frame index in display order that we want to seek to
  // codingOrderFrameIdx: The index of the frame in coding order (for use with getFrameStartEndPos).
  int getClosestSeekableFrameNumberBefore(int frameIdx, int &codingOrderFrameIdx) const;

  // Get the parameters sets as extradata. The format of this depends on the underlying codec.
  virtual QByteArray getExtradata() = 0;
  // Get some other properties of the bitstream in order to configure the FFMpegDecoder
  virtual QPair<int,int> getProfileLevel() = 0;
  virtual QPair<int,int> getSampleAspectRatio() = 0;

  QUint64Pair getFrameStartEndPos(int codingOrderFrameIdx);

  bool parseAnnexBFile(QScopedPointer<fileSourceAnnexBFile> &file, QWidget *mainWindow=nullptr);

  // Called from the bitstream analyzer. This function can run in a background process.
  bool runParsingOfFile(QString compressedFilePath) Q_DECL_OVERRIDE;

protected:
  
  /* The basic NAL unit. Contains the NAL header and the file position of the unit.
  */
  struct nal_unit
  {
    nal_unit(QUint64Pair filePosStartEnd, int nal_idx) : filePosStartEnd(filePosStartEnd), nal_idx(nal_idx), nal_unit_type_id(-1) {}
    virtual ~nal_unit() {} // This class is meant to be derived from.

    // Parse the header from the given data bytes. If a parserCommon::TreeItem pointer is provided, the values will be added to the tree as well.
    virtual bool parse_nal_unit_header(const QByteArray &header_data, parserCommon::TreeItem *root) = 0;

    /// Pointer to the first byte of the start code of the NAL unit
    QUint64Pair filePosStartEnd;

    // The index of the nal within the bitstream
    int nal_idx;

    // Get the NAL header including the start code
    virtual QByteArray getNALHeader() const = 0;
    virtual bool isParameterSet() const = 0;
    virtual int  getPOC() const { return -1; }
    // Get the raw NAL unit (excluding a start code, including nal unit header and payload)
    // This only works if the payload was saved of course
    QByteArray getRawNALData() const { return getNALHeader() + nalPayload; }

    // Each nal unit (in all known standards) has a type id
    unsigned int nal_unit_type_id;

    // Optionally, the NAL unit can store it's payload. A parameter set, for example, can thusly be saved completely.
    QByteArray nalPayload;
  };

  // Parsing of an SEI message may fail when the required parameter sets are not yet available and parsing has to be performed
  // once the required parameter sets are recieved.
  enum sei_parsing_return_t
  {
    SEI_PARSING_OK,                      // Parsing is done
    SEI_PARSING_ERROR,                   // A parsing error occured
    SEI_PARSING_WAIT_FOR_PARAMETER_SETS  // We have to wait for valid parameter sets before we can parse this SEI
  };
  
  struct annexBFrame
  {
    int poc;                     //< The poc of this frame
    QUint64Pair fileStartEndPos; //< The start and end position of all slice NAL units
    bool randomAccessPoint;      //< Can we start decoding here?
  };

  // A list of all frames in the sequence (in coding order) with POC and the file positions of all slice NAL units associated with a frame.
  // POC's don't have to be consecutive, so the only way to know how many pictures are in a sequences is to keep a list of all POCs.
  QList<annexBFrame> frameList;

  // We also keep a sorted list of POC values in order to map from frame indices to POC
  QList<int> POCList;

  // Returns false if the POC was already present int the list
  bool addFrameToList(int poc, QUint64Pair fileStartEndPos, bool randomAccessPoint);

  // A list of nal units sorted by position in the file.
  // Only parameter sets and random access positions go in here.
  // So basically all information we need to seek in the stream and get the active parameter sets to start the decoder at a certain position.
  QList<QSharedPointer<nal_unit>> nalUnitList;

  int pocOfFirstRandomAccessFrame {-1};

  // Save general information about the file here
  struct stream_info_type
  {
    QList<QTreeWidgetItem*> getStreamInfo();

    int64_t file_size;
    int nr_nal_units { 0 };
    int nr_frames    { 0 };
    bool parsing     { false };
  };
  stream_info_type stream_info;
};

#endif // PARSERANNEXB_H
