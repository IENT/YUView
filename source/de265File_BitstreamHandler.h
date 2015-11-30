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

#ifndef DE265FILE_BITSTREAMHANDLER_H
#define DE265FILE_BITSTREAMHANDLER_H

#include <QFile>
#include <QMap>

#define BUFFER_SIZE 40960

class de265File_FileHandler
{
public:
  de265File_FileHandler();

  bool loadFile(QString fileName);
  QString fileName() { return p_srcFile ? p_srcFile->fileName() : QString(""); }

  // Is the file at the end?
  bool atEnd() { return p_FileBufferSize == 0; }

  // Seek to the first byte of the payload data of the next NAL unit
  // Return false if not successfull (eg. file ended)
  bool seekToNextNALUnit();

  // Get the remaining bytes in the NAL unit or maxBytes (if set).
  // This function might also return less than maxBytes if a NAL header is encountered before reading maxBytes bytes.
  // Or: do getCurByte(), gotoNextByte until we find a new start code.
  QByteArray getRemainingNALBytes( int maxBytes=-1 );
  
  // Move the file to the next byte. Update the buffer if necessary.
  // Return false if the operation failed.
  bool gotoNextByte();

  // Get the current byte in the buffer
  char getCurByte() { return p_FileBuffer.at(p_posInBuffer); }

  // Get if the current position is the one byte of a start code
  bool curPosAtStartCode() { return p_numZeroBytes >= 2 && getCurByte() == (char)1; }

  // The current absolut position in the file (byte precise)
  quint64 tell() { return p_bufferStartPosInFile + p_posInBuffer; }

  // How many POC's have been found in the file
  int getNumberPOCs() { return p_POC_List.size(); }
  // What is the width and height in pixels of the sequence?
  QSize getSequenceSize();
  // What it the framerate?
  double getFramerate();

  // Calculate the closest random access point (RAP) before the given frame number.
  // Return the frame number of that random access point.
  int getClosestSeekableFrameNumber(int frameIdx);

  // Seek the file to the given frame number. The given frame number has to be a random 
  // access point. We can start decoding the file from here. Use getClosestSeekableFrameNumber to find a random access point.
  // Returns the active parameter sets as a byte array. This has to be given to the decoder first.
  QByteArray seekToFrameNumber(int iFrameNr);

  // For the current file position get all active parameter sets that will be
  // needed to start decoding from the current file position on.
  QByteArray getActiveParameterSetsBitstream();

  // Read the remaining bytes from the buffer and return them. Then load the next buffer.
  QByteArray getRemainingBuffer_Update() { QByteArray retArr = p_FileBuffer.mid(p_posInBuffer, p_FileBufferSize-p_posInBuffer); updateBuffer(); return retArr; }

protected:
  // ----- Some nested classes that are only used in the scope of this file handler class

  // All the different NAL unit types (T-REC-H.265-201504 Page 85)
  enum nal_unit_type
  {
    TRAIL_N,        TRAIL_R,     TSA_N,          TSA_R,          STSA_N,      STSA_R,      RADL_N,     RADL_R,     RASL_N,     RASL_R, 
    RSV_VCL_N10,    RSV_VCL_N12, RSV_VCL_N14,    RSV_VCL_R11,    RSV_VCL_R13, RSV_VCL_R15, BLA_W_LP,   BLA_W_RADL, BLA_N_LP,   IDR_W_RADL,
    IDR_N_LP,       CRA_NUT,     RSV_IRAP_VCL22, RSV_IRAP_VCL23, RSV_VCL24,   RSV_VCL25,   RSV_VCL26,  RSV_VCL27,  RSV_VCL28,  RSV_VCL29,
    RSV_VCL30,      RSV_VCL31,   VPS_NUT,        SPS_NUT,        PPS_NUT,     AUD_NUT,     EOS_NUT,    EOB_NUT,    FD_NUT,     PREFIX_SEI_NUT,
    SUFFIX_SEI_NUT, RSV_NVCL41,  RSV_NVCL42,     RSV_NVCL43,     RSV_NVCL44,  RSV_NVCL45,  RSV_NVCL46, RSV_NVCL47, UNSPECIFIED
  };

  /* This class provides the ability to read a byte array bit wise. Reading of ue(v) symbols is also supported.
  */
  class sub_byte_reader
  {
  public:
    sub_byte_reader(QByteArray inArr)
    { 
      p_posInBuffer_bytes = 0;
      p_posInBuffer_bits = 0;
      p_numEmuPrevZeroBytes = 0;
      p_byteArray = inArr;
    };

    // Read the given number of bits and return as integer. Returns -1 if an error occured while reading.
    int readBits(int nrBits);

    // Read an UE(v) code from the array
    int readUE_V();

  protected:
    QByteArray p_byteArray;

    // Move to the next byte and look for an emulation prevention 3 byte. Remove it (skip it) if found.
    // This function is just used by the internal reading functions.
    bool p_gotoNextByte();

    int p_posInBuffer_bytes;   // The byte position in the buffer
    int p_posInBuffer_bits;    // The sub byte (bit) position in the buffer (0...7)
    int p_numEmuPrevZeroBytes; // The number of emulation prevention three bytes that were found
  };

  /* The basic NAL unit. Contains the NAL header and the file position of the unit.
  */
  class nal_unit
  {
  public:
    nal_unit(quint64 pos, nal_unit_type type, int layer, int temporalID) {
      filePos = pos;
      nal_type = type;
      nuh_layer_id = layer;
      nuh_temporal_id_plus1 = temporalID;
    }
    virtual ~nal_unit() {};

    bool isIRAP() { return (nal_type == BLA_W_LP       || nal_type == BLA_W_RADL ||
                            nal_type == BLA_N_LP       || nal_type == IDR_W_RADL ||
                            nal_type == IDR_N_LP       || nal_type == CRA_NUT    ||
                            nal_type == RSV_IRAP_VCL22 || nal_type == RSV_IRAP_VCL23); }

    bool isSLNR() { return (nal_type == TRAIL_N     || nal_type == TSA_N       ||
                            nal_type == STSA_N      || nal_type == RADL_N      ||
                            nal_type == RASL_N      || nal_type == RSV_VCL_N10 ||
                            nal_type == RSV_VCL_N12 || nal_type == RSV_VCL_N14); }

    bool isRADL() { return (nal_type == RADL_N || nal_type == RADL_R); }
    bool isRASL() { return (nal_type == RASL_N || nal_type == RASL_R); }

    bool isSlice() { return nal_type == IDR_W_RADL || nal_type == IDR_N_LP   || nal_type == CRA_NUT  ||
                            nal_type == BLA_W_LP   || nal_type == BLA_W_RADL || nal_type == BLA_N_LP ||
                            nal_type == TRAIL_N    || nal_type == TRAIL_R    || nal_type == TSA_N    ||
                            nal_type == TSA_R      || nal_type == STSA_N     || nal_type == STSA_R   ||
                            nal_type == RADL_N     || nal_type == RADL_R     || nal_type == RASL_N   ||
                            nal_type == RASL_R; }

    /// Pointer to the first byte of the start code of the NAL unit
    quint64 filePos;

    //( Get the NAL header including the start code
    QByteArray getNALHeader() { 
      int out = ((int)nal_type << 9) + (nuh_layer_id << 3) + nuh_temporal_id_plus1;
      char c[6] = { 0, 0, 0, 1,  (char)(out >> 8), (char)out };
      return QByteArray(c, 6);
    }

    /// The information of the NAL unit header
    nal_unit_type nal_type;
    int nuh_layer_id;
    int nuh_temporal_id_plus1;
  };

  /* The basic parameter set. A parameter set can save it's actual payload data.
  */
  class parameter_set_nal : public nal_unit
  {
  public:
    parameter_set_nal(quint64 filePos, nal_unit_type type, int layer, int temporalID) :
      nal_unit(filePos, type, layer, temporalID) {};
    virtual ~parameter_set_nal() {};

    QByteArray getParameterSetData() { return getNALHeader() + parameter_set_data; }
    bool parse_profile_tier_level(sub_byte_reader &reader, bool profilePresentFlag, int maxNumSubLayersMinus1);
  
    // The payload of the parameter set
    QByteArray parameter_set_data;
  };

  /* The video parameter set. 
  */
  class vps : public parameter_set_nal
  {
  public:
    vps(quint64 filePos, nal_unit_type type, int layer, int temporalID) :
      parameter_set_nal(filePos, type, layer, temporalID)
    {
      vps_timing_info_present_flag = false;
      frameRate = 0.0;
    };
    virtual ~vps() {};

    bool parse_vps(QByteArray parameterSetData);

    int vps_video_parameter_set_id; /// vps ID
    int vps_max_layers_minus1;		  /// How many layers are there. Is this a scalable bitstream?
    bool vps_timing_info_present_flag;
  
    double frameRate;
  };

  /* The sequence parameter set. 
  */
  class sps : public parameter_set_nal
  {
  public:
    sps(quint64 filePos, nal_unit_type type, int layer, int temporalID) :
      parameter_set_nal(filePos, type, layer, temporalID)
    {
      vui_timing_info_present_flag = false;
      frameRate = 0.0;
    };
    virtual ~sps() {};
    bool parse_sps(QByteArray parameterSetData);

    int sps_max_sub_layers_minus1;
    int sps_video_parameter_set_id;
    int sps_seq_parameter_set_id;
    int chroma_format_idc;
    int separate_colour_plane_flag;
    int pic_width_in_luma_samples;
    int pic_height_in_luma_samples;
    int conformance_window_flag;
  
    int conf_win_left_offset;
    int conf_win_right_offset;
    int conf_win_top_offset;
    int conf_win_bottom_offset;
  
    int bit_depth_luma_minus8;
    int bit_depth_chroma_minus8;
    int log2_max_pic_order_cnt_lsb_minus4;

    bool vui_timing_info_present_flag;
    double frameRate;

    // Calculated values
    int SubWidthC, SubHeightC;
  
    // Get the actual size of the image that will be returned. Internally the image might be bigger.
    int get_conformance_cropping_width() {return (pic_width_in_luma_samples - (SubWidthC * conf_win_right_offset) - SubWidthC * conf_win_left_offset); }
    int get_conformance_cropping_height() {return (pic_height_in_luma_samples - (SubHeightC * conf_win_bottom_offset) - SubHeightC * conf_win_top_offset); }
  };

  /* The picture parameter set. 
  */
  class pps : public parameter_set_nal
  {
  public:
    pps(quint64 filePos, nal_unit_type type, int layer, int temporalID) :
      parameter_set_nal(filePos, type, layer, temporalID) {};
    virtual ~pps() {};
    bool parse_pps(QByteArray parameterSetData);
    
    int pps_pic_parameter_set_id;
    int pps_seq_parameter_set_id;

    bool output_flag_present_flag;
    int num_extra_slice_header_bits;
  };

  /* A slice NAL unit. 
  */
  class slice : public nal_unit
  {
  public:
    slice(quint64 filePos, nal_unit_type type, int layer, int temporalID) :
      nal_unit(filePos, type, layer, temporalID)
    {
      PicOrderCntVal = -1;
      PicOrderCntMsb = -1;
    };
    virtual ~slice() {};
    bool parse_slice(QByteArray sliceHeaderData,
                     QMap<int, sps*> p_active_SPS_list,
                     QMap<int, pps*> p_active_PPS_list );

    bool first_slice_segment_in_pic_flag;
    int slice_pic_parameter_set_id;
    int slice_type;
    bool pic_output_flag;
    int colour_plane_id;
    int slice_pic_order_cnt_lsb;

    // Calculated values
    int PicOrderCntVal; // The slice POC
    int PicOrderCntMsb;

    // Static variables for keeping track of the decoding order
    static bool bFirstAUInDecodingOrder;
    static int prevTid0Pic_slice_pic_order_cnt_lsb;
    static int prevTid0Pic_PicOrderCntMsb;
  };
  
  // The source binary file
  QFile *p_srcFile;
  QByteArray p_FileBuffer;
  quint64 p_FileBufferSize;
  int     p_posInBuffer;		      ///< The current position in the input buffer in bytes
  quint64 p_bufferStartPosInFile; ///< The byte position in the file of the start of the currently loaded buffer
  int     p_numZeroBytes;         ///< The number of zero bytes that occured. (This will be updated by gotoNextByte() and seekToNextNALUnit()

  // The start code pattern
  QByteArray p_startCode;

  // A list of nal units sorted by position in the file.
  // Only parameter sets and random access positions go in here.
  // So basically all information we need to start the decoder at a certain position.
  QList<nal_unit*> p_nalUnitList;

  // A list of all POCs in the sequence (in coding order). POC's don't have to be consecutive, so the only
  // way to know how many pictures are in a sequences is to keep a list of all POCs.
  QList<int> p_POC_List;
  // Returns false if the POC was already present int the list
  bool p_addPOCToList(int poc);
  
  // Scan the file NAL by NAL. Keep track of all possible random access points and parameter sets in
  // p_nalUnitList. Also collect a list of all POCs in coding order in p_POC_List.
  bool scanFileForNalUnits();

  // load the next buffer
  bool updateBuffer();

  // Seek the file to the given byte position. Update the buffer.
  bool p_seekToFilePos(quint64 pos);
};

#endif //DE265FILE_BITSTREAMHANDLER_H