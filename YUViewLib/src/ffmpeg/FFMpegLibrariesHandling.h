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

#pragma once

#include <stdint.h>
#include <assert.h>
#include <QLibrary>

#include "ffmpeg/FFMpegLibrariesTypes.h"
#include "video/videoHandlerYUV.h"
#include "video/videoHandlerRGB.h"

using namespace FFmpeg;

/* This class is responsible for opening the ffmpeg libraries and binding all the functions.
 * The used FFmpeg structs (like AVFormatContext, AVPacket, ...) are not defined because it
 * depends on the loaded library versions how to interprete them.
*/
class FFmpegLibraryFunctions
{
public:

  FFmpegLibraryFunctions();

  // Load the FFmpeg libraries from the given path.
  // The 4 ints give the versions of the 4 libraries (Util, codec, format, swresample)
  bool loadFFmpegLibraryInPath(QString path, int libVersions[4]);
  // Try to load the 4 given specific libraries
  bool loadFFMpegLibrarySpecific(QString avFormatLib, QString avCodecLib, QString avUtilLib, QString swResampleLib);
  
  QStringList getLibPaths() const;

  // From avformat
  void     (*av_register_all)           ();
  int      (*avformat_open_input)       (AVFormatContext **ps, const char *url, AVInputFormat *fmt, AVDictionary **options);
  void     (*avformat_close_input)      (AVFormatContext **s);
  int      (*avformat_find_stream_info) (AVFormatContext *ic, AVDictionary **options);
  int      (*av_read_frame)             (AVFormatContext *s, AVPacket *pkt);
  int      (*av_seek_frame)             (AVFormatContext *s, int stream_index, int64_t timestamp, int flags);
  unsigned (*avformat_version)          (void);

  // From avcodec
  AVCodec           *(*avcodec_find_decoder)     (AVCodecID id);
  AVCodecContext    *(*avcodec_alloc_context3)   (const AVCodec *codec);
  int                (*avcodec_open2)            (AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
  void               (*avcodec_free_context)     (AVCodecContext **avctx);
  AVPacket          *(*av_packet_alloc)          (void);
  void               (*av_packet_free)           (AVPacket **pkt);
  void               (*av_init_packet)           (AVPacket *pkt);
  void               (*av_packet_unref)          (AVPacket *pkt);
  void               (*avcodec_flush_buffers)    (AVCodecContext *avctx);
  unsigned           (*avcodec_version)          (void);
  const char        *(*avcodec_get_name)         (AVCodecID id);
  AVCodecParameters *(*avcodec_parameters_alloc) (void);

  // The following functions are part of the new API.
  // The following function is quite new. We will check if it is available.
  // If not, we will use the old decoding API.
  int             (*avcodec_send_packet)           (AVCodecContext *avctx, const AVPacket *avpkt);
  int             (*avcodec_receive_frame)         (AVCodecContext *avctx, AVFrame *frame);
  int             (*avcodec_parameters_to_context) (AVCodecContext *codec, const AVCodecParameters *par);
  bool newParametersAPIAvailable;
  // This function is deprecated. So we only use it if the new API is not available.
  int             (*avcodec_decode_video2) (AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, const AVPacket *avpkt);

  // From avutil
  AVFrame                  *(*av_frame_alloc)         (void);
  void                      (*av_frame_free)          (AVFrame **frame);
  void                     *(*av_mallocz)             (size_t size);
  unsigned                  (*avutil_version)         (void);
  int                       (*av_dict_set)            (AVDictionary **pm, const char *key, const char *value, int flags);
  AVDictionaryEntry        *(*av_dict_get)            (AVDictionary *m, const char *key, const AVDictionaryEntry *prev, int flags);
  AVFrameSideData          *(*av_frame_get_side_data) (const AVFrame *frame, AVFrameSideDataType type);
  AVDictionary             *(*av_frame_get_metadata)  (const AVFrame *frame);
  void 	                    (*av_log_set_callback)    (void(*callback)(void *, int, const char *, va_list));
  void 	                    (*av_log_set_level)       (int level);  
  AVPixFmtDescriptor       *(*av_pix_fmt_desc_get)    (AVPixelFormat pix_fmt);
  AVPixFmtDescriptor       *(*av_pix_fmt_desc_next)   (const AVPixFmtDescriptor *prev);
  AVPixelFormat             (*av_pix_fmt_desc_get_id) (const AVPixFmtDescriptor *desc);

  // From swresample
  unsigned  (*swresample_version) (void);

  void setLogList(QStringList *l) { logList = l; }

private:
  // bind all functions from the loaded QLibraries.
  bool bindFunctionsFromLibraries();

  void addLibNamesToList(QString libName, QStringList &l, const QLibrary &lib) const;

  // Load the function pointers from the four individual libraries
  bool bindFunctionsFromAVFormatLib();
  bool bindFunctionsFromAVCodecLib();
  bool bindFunctionsFromAVUtilLib();
  bool bindFunctionsFromSWResampleLib();

  QFunctionPointer resolveAvUtil(const char *symbol);
  template <typename T> bool resolveAvUtil(T &ptr, const char *symbol);
  QFunctionPointer resolveAvFormat(const char *symbol);
  template <typename T> bool resolveAvFormat(T &ptr, const char *symbol);
  QFunctionPointer resolveAvCodec(const char *symbol, bool failIsError);
  template <typename T> bool resolveAvCodec(T &ptr, const char *symbol, bool failIsError=true);
  QFunctionPointer resolveSwresample(const char *symbol);
  template <typename T> bool resolveSwresample(T &ptr, const char *symbol);

  // Logging
  void log(QString f, QString s) { if (logList) logList->append(f + " " + s); }
  QStringList *logList { nullptr };

  QLibrary libAvutil;
  QLibrary libSwresample;
  QLibrary libAvcodec;
  QLibrary libAvformat;
};

struct FFmpegLibraryVersion
{
  int avutil {0};
  int swresample {0};
  int avcodec {0};
  int avformat {0};
  int avutil_minor {0};
  int swresample_minor {0};
  int avcodec_minor {0};
  int avformat_minor {0};
  int avutil_micro {0};
  int swresample_micro {0};
  int avcodec_micro {0};
  int avformat_micro {0};
};

class FFmpegVersionHandler;

class AVInputFormatWrapper
{
public:
  AVInputFormatWrapper() { fmt = nullptr; }
  AVInputFormatWrapper(AVInputFormat *f, FFmpegLibraryVersion v) { fmt = f; libVer = v; update(); }
  explicit operator bool() const { return fmt != nullptr; };

private:
  // Update all private values from the AVCodecContext
  void update();

  // These are here for debugging purposes.
  QString name;
  QString long_name;
  int flags;
  QString extensions;
  //const struct AVCodecTag * const *codec_tag;
  //const AVClass *priv_class;
  QString mime_type;

  AVInputFormat *fmt;
  FFmpegLibraryVersion libVer;
};

class AVCodecContextWrapper
{
public:
  AVCodecContextWrapper() { codec = nullptr; }
  AVCodecContextWrapper(AVCodecContext *c, FFmpegLibraryVersion v) { codec = c; libVer = v; update(); }
  explicit operator bool() const { return codec != nullptr; };

  AVMediaType getCodecType()  { update(); return codec_type; }
  AVCodecID getCodecID(){ update(); return codec_id; }
  AVCodecContext *getCodec() { return codec; }
  AVPixelFormat getPixelFormat() { update(); return pix_fmt; }
  int getWidth() { update(); return width; }
  int getHeight() { update(); return height; }
  AVColorSpace getColorspace() { update(); return colorspace; }
  AVRational getTimeBase() { update(); return time_base; }
  QByteArray getExtradata() { update(); return extradata; }

private:
  // Update all private values from the AVCodecContext
  void update();

  // These are private. Use "update" to update them from the AVCodecContext
  AVMediaType codec_type;
  QString codec_name;
  AVCodecID codec_id;
  unsigned int codec_tag;
  unsigned int stream_codec_tag;
  int bit_rate;
  int bit_rate_tolerance;
  int global_quality;
  int compression_level;
  int flags;
  int flags2;
  QByteArray extradata;
  AVRational time_base;
  int ticks_per_frame;
  int delay;
  int width, height;
  int coded_width, coded_height;
  int gop_size;
  AVPixelFormat pix_fmt;
  int me_method;
  int max_b_frames;
  float b_quant_factor;
  int rc_strategy;
  int b_frame_strategy;
  float b_quant_offset;
  int has_b_frames;
  int mpeg_quant;
  float i_quant_factor;
  float i_quant_offset;
  float lumi_masking;
  float temporal_cplx_masking;
  float spatial_cplx_masking;
  float p_masking;
  float dark_masking;
  int slice_count;
  int prediction_method;
  //int *slice_offset;
  AVRational sample_aspect_ratio;
  int me_cmp;
  int me_sub_cmp;
  int mb_cmp;
  int ildct_cmp;
  int dia_size;
  int last_predictor_count;
  int pre_me;
  int me_pre_cmp;
  int pre_dia_size;
  int me_subpel_quality;
  int dtg_active_format;
  int me_range;
  int intra_quant_bias;
  int inter_quant_bias;
  int slice_flags;
  int xvmc_acceleration;
  int mb_decision;
  //uint16_t *intra_matrix;
  //uint16_t *inter_matrix;
  int scenechange_threshold;
  int noise_reduction;
  int me_threshold;
  int mb_threshold;
  int intra_dc_precision;
  int skip_top;
  int skip_bottom;
  float border_masking;
  int mb_lmin;
  int mb_lmax;
  int me_penalty_compensation;
  int bidir_refine;
  int brd_scale;
  int keyint_min;
  int refs;
  int chromaoffset;
  int scenechange_factor;
  int mv0_threshold;
  int b_sensitivity;
  AVColorPrimaries color_primaries;
  AVColorTransferCharacteristic color_trc;
  AVColorSpace colorspace; 
  AVColorRange color_range;
  AVChromaLocation chroma_sample_location;

  AVCodecContext *codec;
  FFmpegLibraryVersion libVer;
};

// This is a version independent wrapper for the version dependent ffmpeg AVCodecParameters.
// These were added with AVFormat major version 57.
// It is our own and can be created on the stack and is nicer to debug.
class AVCodecParametersWrapper
{
public:
  AVCodecParametersWrapper() { param = nullptr; }
  AVCodecParametersWrapper(AVCodecParameters *p, FFmpegLibraryVersion v) { param = p; libVer = v; update(); }
  explicit operator bool() const { return param != nullptr; }
  QStringPairList getInfoText();

  AVMediaType getCodecType()    { update(); return codec_type; }
  AVCodecID getCodecID()        { update(); return codec_id; }
  int getWidth()                { update(); return width; }
  int getHeight()               { update(); return height; }
  AVColorSpace getColorspace()  { update(); return color_space; }
  Ratio getSampleAspectRatio()  { update(); return Ratio({sample_aspect_ratio.num, sample_aspect_ratio.den}); }

  // Set a default set of (unknown) values
  void setClearValues();

  void setAVMediaType(AVMediaType type);
  void setAVCodecID(AVCodecID id);
  void setExtradata(QByteArray extradata);
  void setSize(int width, int height);
  void setAVPixelFormat(AVPixelFormat f);
  void setProfileLevel(int profile, int level);
  void setSampleAspectRatio(int num, int den);
  
  AVCodecParameters *getCodecParameters() { return param; }

private:
  // Update all private values from the AVCodecParameters
  void update();

  // These are private. Use "update" to update them from the AVCodecParameters
  AVMediaType codec_type;
  AVCodecID codec_id;
  uint32_t codec_tag;
  uint8_t *extradata;
  int extradata_size;
  int format;
  int64_t bit_rate;
  int bits_per_coded_sample;
  int bits_per_raw_sample;
  int profile;
  int level;
  int width;
  int height;
  AVRational                    sample_aspect_ratio;
  AVFieldOrder                  field_order;
  AVColorRange                  color_range;
  AVColorPrimaries              color_primaries;
  AVColorTransferCharacteristic color_trc;
  AVColorSpace                  color_space;
  AVChromaLocation              chroma_location;
  int video_delay;

  // When setting custom metadata, we keep a reference to it here.
  QByteArray set_extradata;

  AVCodecParameters *param;
  FFmpegLibraryVersion libVer;
};

class AVCodecIDWrapper
{
public:
  AVCodecIDWrapper() {}
  AVCodecIDWrapper(AVCodecID codecID, QString codecName) : codecID(codecID), codecName(codecName) {}

  QString getCodecName() { return codecName; }

  void setTypeHEVC() { codecName = "hevc"; }
  void setTypeAVC()  { codecName = "h264";  }

  bool isHEVC()  { return codecName == "hevc"; }
  bool isAVC()   { return codecName == "h264"; }
  bool isMpeg2() { return codecName == "mpeg2video"; }
  bool isAV1()   { return codecName == "av1"; }

  bool isNone()  { return codecName.isEmpty() || codecName == "unknown_codec" || codecName == "none"; }

  bool operator==(const AVCodecIDWrapper &a) { return codecID == a.codecID; }

  friend FFmpegVersionHandler;
private:
  AVCodecID codecID {AV_CODEC_ID_NONE};
  QString codecName;
};

// This is a version independent wrapper for the version dependent ffmpeg AVStream.
// It is our own and can be created on the stack and is nicer to debug.
class AVStreamWrapper
{
public:
  AVStreamWrapper() { }
  AVStreamWrapper(AVStream *src_str, FFmpegLibraryVersion v) { this->str = src_str; this->libVer = v; update(); }
  explicit operator bool() const { return this->str != nullptr; };
  QStringPairList getInfoText(AVCodecIDWrapper &codecIdWrapper);

  AVMediaType getCodecType();
  QString getCodecTypeName();
  AVCodecID getCodecID();
  AVCodecContextWrapper &getCodec() { update(); return this->codec; };
  AVRational getAvgFrameRate()   { update(); return this->avg_frame_rate; }
  AVRational getTimeBase();
  int getFrameWidth();
  int getFrameHeight();
  AVColorSpace getColorspace();
  int getIndex() { update(); return index; }

  AVCodecParametersWrapper getCodecpar() { update(); return this->codecpar; }

private:
  void update();

  // These are private. Use "update" to update them from the AVFormatContext
  int index;
  int id;
  AVCodecContextWrapper codec;
  AVRational time_base;
  int64_t start_time;
  int64_t duration;
  int64_t nb_frames;
  int disposition;
  enum AVDiscard discard;
  AVRational sample_aspect_ratio;
  AVRational avg_frame_rate;
  int nb_side_data;
  int event_flags;
    
  // The AVCodecParameters are present from avformat major version 57 and up.
  AVCodecParametersWrapper codecpar;

  AVStream *str {nullptr};
  FFmpegLibraryVersion libVer;
};

// AVPacket data can be in one of two formats:
// 1: The raw annexB format with start codes (0x00000001 or 0x000001)
// 2: ISO/IEC 14496-15 mp4 format: The first 4 bytes determine the size of the NAL unit followed by the payload
enum packetDataFormat_t
{
  packetFormatUnknown,
  packetFormatRawNAL,
  packetFormatMP4,
  packetFormatOBU
};

enum class PacketType
{
  VIDEO,
  AUDIO,
  SUBTITLE_DVB,
  SUBTITLE_608,
  OTHER
};

// A wrapper around the different versions of the AVPacket versions.
// It also adds some functions like automatic deletion when it goes out of scope.
class AVPacketWrapper
{
public:
  AVPacketWrapper() { }
  AVPacketWrapper(FFmpegVersionHandler &ff) { this->allocatePaket(ff); }
  ~AVPacketWrapper();
  // Create a new paket and initilize it using av_init_packet.
  void allocatePaket(FFmpegVersionHandler &ff);
  void unrefPacket(FFmpegVersionHandler &ff);
  void freePacket(FFmpegVersionHandler &ff);
  void setData(QByteArray &set_data);
  void setPTS(int64_t pts);
  void setDTS(int64_t dts);
  explicit operator bool() const { return this->pkt != nullptr; };
  AVPacket *getPacket()      { update(); return this->pkt; }
  int      getStreamIndex()  { update(); return this->stream_index; }
  int64_t  getPTS()          { update(); return this->pts; }
  int64_t  getDTS()          { update(); return this->dts; }
  int64_t  getDuration()     { update(); return this->duration; }
  int      getFlags()        { update(); return this->flags; }
  bool     getFlagKeyframe() { update(); return this->flags & AV_PKT_FLAG_KEY; }
  bool     getFlagCorrupt()  { update(); return this->flags & AV_PKT_FLAG_CORRUPT; }
  bool     getFlagDiscard()  { update(); return this->flags & AV_PKT_FLAG_DISCARD; }
  uint8_t *getData()         { update(); return this->data; }
  int      getDataSize()     { update(); return this->size; }

  // This info is set externally (in FileSourceFFmpegFile) based on the stream info
  PacketType getPacketType()                      { return this->packetType; }
  void       setPacketType(PacketType packetType) { this->packetType = packetType; }
  
  // Guess the format. The actual guessing is only performed if the packetFormat is not set yet.
  packetDataFormat_t guessDataFormatFromData();

private:
  void update();

  bool checkForRawNALFormat(QByteArray &data, bool threeByteStartCode=false);
  bool checkForMp4Format(QByteArray &data);
  bool checkForObuFormat(QByteArray &data);

  // These are private. Use "update" to update them from the AVFormatContext
  AVBufferRef *buf;
  int64_t pts;
  int64_t dts;
  uint8_t *data;
  int size;
  int stream_index;
  int flags;
  AVPacketSideData *side_data;
  int side_data_elems;
  int64_t duration;
  int64_t pos;
  int64_t convergence_duration;

  PacketType packetType;

  AVPacket *pkt {nullptr};
  FFmpegLibraryVersion libVer;
  packetDataFormat_t packetFormat {packetFormatUnknown};
};

class AVDictionaryWrapper
{
public:
  AVDictionaryWrapper() {}
  AVDictionaryWrapper(AVDictionary *dict) : dict(dict) {}
  void setDictionary(AVDictionary *d) { this->dict = d; }
  explicit operator bool() const { return this->dict != nullptr; }
  AVDictionary *getDictionary() { return this->dict; }
  
private:
  AVDictionary *dict {nullptr};
};

// This is a version independent wrapper for the version dependent ffmpeg AVFormatContext
// It is our own and can be created on the stack and is nicer to debug.
class AVFormatContextWrapper
{
public:
  AVFormatContextWrapper() {};
  AVFormatContextWrapper(AVFormatContext *c, FFmpegLibraryVersion v) { this->ctx = c; this->libVer = v; update(); }
  void updateFrom(AVFormatContext *c) { assert(this->ctx == nullptr); this->ctx = c; update(); }
  void avformat_close_input(FFmpegVersionHandler &ver);
  explicit operator bool() const { return this->ctx != nullptr; };
  QStringPairList getInfoText();

  unsigned int getNbStreams() { update(); return this->nb_streams; }
  AVStreamWrapper getStream(int idx) { update(); return this->streams[idx]; }
  AVInputFormatWrapper getInputFormat() { update(); return this->iformat; }
  int64_t getStartTime() { update(); return this->start_time; }
  int64_t getDuration() { update(); return this->duration; }
  AVFormatContext *getFormatCtx() { return this->ctx; }
  AVDictionaryWrapper getMetadata() { update(); return this->metadata; }

  // Read a frame into the given pacetk (av_read_frame)
  int readFrame(FFmpegVersionHandler &ff, AVPacketWrapper &pkt);

private:
  // Update all private values from the AVFormatContext
  void update();

  AVInputFormatWrapper iformat;

  // These are private. Use "update" to update them from the AVFormatContext
  int ctx_flags {0};
  unsigned int nb_streams {0};
  QList<AVStreamWrapper> streams;
  QString filename;
  int64_t start_time {-1};
  int64_t duration {-1};
  int bit_rate {0};
  unsigned int packet_size {0};
  int max_delay {0};
  int flags {0};

  unsigned int probesize {0};
  int max_analyze_duration {0};
  QString key;
  unsigned int nb_programs {0};
  AVCodecID video_codec_id {AV_CODEC_ID_NONE};
  AVCodecID audio_codec_id {AV_CODEC_ID_NONE};
  AVCodecID subtitle_codec_id {AV_CODEC_ID_NONE};
  unsigned int max_index_size {0};
  unsigned int max_picture_buffer {0};
  unsigned int nb_chapters {0};
  AVDictionaryWrapper metadata;

  AVFormatContext *ctx {nullptr};
  FFmpegLibraryVersion libVer;
};

class AVCodecWrapper
{
public:
  AVCodecWrapper() {}
  AVCodecWrapper(AVCodec *codec, FFmpegLibraryVersion libVer) : codec(codec), libVer(libVer) {}
  explicit operator bool() const { return this->codec != nullptr; }
  AVCodec *getAVCodec() { return this->codec; }
  AVCodecID getCodecID() { update(); return this->id; }
  QString getName() { update(); return this->name; }
  QString getLongName() { update(); return this->long_name; }

private:
  void update();
  
  QString name;
  QString long_name;
  AVMediaType type;
  AVCodecID id {AV_CODEC_ID_NONE};
  int capabilities {0};                    ///< see AV_CODEC_CAP_
  QList<AVRational> supported_framerates;  ///< terminated by {0,0}
  QList<AVPixelFormat> pix_fmts;           ///< array is terminated by -1
  QList<int> supported_samplerates;        ///< array is terminated by 0
  QList<AVSampleFormat> sample_fmts;       ///< array is terminated by -1
  QList<uint64_t> channel_layouts;         ///< array is terminated by 0
  uint8_t max_lowres {0};                  ///< maximum value for lowres supported by the decoder

  AVCodec *codec {nullptr};
  FFmpegLibraryVersion libVer;
};

class AVFrameWrapper
{
public:
  AVFrameWrapper() { }
  ~AVFrameWrapper() { assert(this->frame == nullptr); }
  void allocateFrame(FFmpegVersionHandler &ff);
  void freeFrame(FFmpegVersionHandler &ff);
  uint8_t *getData(int component) { update(); return this->data[component]; }
  int getLineSize(int component) { update(); return this->linesize[component]; }
  AVFrame *getFrame() { return this->frame; }
  int getWidth() { update(); return this->width; }
  int getHeight() { update(); return this->height; }
  Size getSize() { update(); return Size({size_t(width), size_t(height)}); }
  int getPTS() { update(); return this->pts; }
  AVPictureType getPictType() { update(); return this->pict_type; }
  int getKeyFrame() { update(); return this->key_frame; }
    
  explicit operator bool() const { return this->frame != nullptr; }

private:
  void update();

  // These are private. Use "update" to update them from the AVFormatContext
  uint8_t *data[AV_NUM_DATA_POINTERS];
  int linesize[AV_NUM_DATA_POINTERS];
  int width, height;
  int nb_samples;
  int format;
  int key_frame;
  AVPictureType pict_type;
  AVRational sample_aspect_ratio;
  int64_t pts;
  int64_t pkt_pts;
  int64_t pkt_dts;
  int coded_picture_number;
  int display_picture_number;
  int quality;

  AVFrame *frame {nullptr};
  FFmpegLibraryVersion libVer;
};

class AVMotionVectorWrapper
{
public:
  AVMotionVectorWrapper() {}
  AVMotionVectorWrapper(AVMotionVector *vec, FFmpegLibraryVersion libVer) : vec(vec), libVer(libVer) { update(); }

  // For performance reasons, these are public here. Since update is called at construction, these should be valid.
  int32_t source;
  uint8_t w, h;
  int16_t src_x, src_y;
  int16_t dst_x, dst_y;
  uint64_t flags;
  // The following may be invalid (-1) in older ffmpeg versions)
  int32_t motion_x, motion_y;
  uint16_t motion_scale;

  explicit operator bool() const { return vec != nullptr; };

private:
  void update();

  AVMotionVector *vec {nullptr};
  FFmpegLibraryVersion libVer;
};

class AVFrameSideDataWrapper
{
public:
  AVFrameSideDataWrapper() {}
  AVFrameSideDataWrapper(AVFrameSideData *sideData, FFmpegLibraryVersion libVer) : sideData(sideData), libVer(libVer) { update(); }
  int getNumberMotionVectors();
  AVMotionVectorWrapper getMotionVector(int idx);

  explicit operator bool() const { return sideData != nullptr; }

private:
  void update();
  
  enum AVFrameSideDataType type;
  uint8_t *data {nullptr};
  int size;
  AVDictionary *metadata;
  AVBufferRef *buf;
  
  AVFrameSideData *sideData;
  FFmpegLibraryVersion libVer;
};

/* This class parses the AVPixFmtDescriptor independent of the ffmpeg library version.
 * Unfortunately, the AVPixelFormat enum differs with the version of the ffmpeg libraries and
 * also how the library was compiled. So we use the av_pix_fmt_desc_get function to get this 
 * descriptor which can give us all information about the pixel format.
 */
class AVPixFmtDescriptorWrapper
{
public:
  AVPixFmtDescriptorWrapper() {};
  AVPixFmtDescriptorWrapper(AVPixFmtDescriptor *sideData, FFmpegLibraryVersion libVer);

  YUView::RawFormat getRawFormat() { return flagIsRGB() ? YUView::raw_RGB : YUView::raw_YUV; }
  YUV_Internals::yuvPixelFormat getYUVPixelFormat();
  RGB_Internals::rgbPixelFormat getRGBPixelFormat();

  bool setValuesFromYUVPixelFormat(YUV_Internals::yuvPixelFormat fmt);

  // AVPixFmtDescriptor
  QString name;
  int nb_components {0};  ///< The number of components each pixel has, (1-4)

  /**
   * Amount to shift the luma width right to find the chroma width.
   * For YV12 this is 1 for example.
   * chroma_width = -((-luma_width) >> log2_chroma_w)
   * The note above is needed to ensure rounding up.
   * This value only refers to the chroma components.
   */
  int log2_chroma_w {0};
  
  /**
    * Amount to shift the luma height right to find the chroma height.
    * For YV12 this is 1 for example.
    * chroma_height= -((-luma_height) >> log2_chroma_h)
    * The note above is needed to ensure rounding up.
    * This value only refers to the chroma components.
    */
  int log2_chroma_h {0};

  // The flags. Fortunately, these do not change with the version of AVUtil.
  int flags {0};
  bool flagIsBigEndian()           { return this->flags & (1 << 0); } // Pixel format is big-endian.
  bool flagIsPallette()            { return this->flags & (1 << 1); } // Pixel format has a palette in data[1], values are indexes in this palette.
  bool flagIsBitWisePacked()       { return this->flags & (1 << 2); } // All values of a component are bit-wise packed end to end.
  bool flagIsHWAcceleratedFormat() { return this->flags & (1 << 3); } // Pixel format is an HW accelerated format.
  bool flagIsPlanar()              { return this->flags & (1 << 4); } // At least one pixel component is not in the first data plane.
  bool flagIsRGB()                 { return this->flags & (1 << 5); } // The pixel format contains RGB-like data (as opposed to YUV/grayscale).
  bool flagIsIsPseudoPallette()    { return this->flags & (1 << 6); } // The pixel format is "pseudo-paletted".
  bool flagHasAlphaPlane()         { return this->flags & (1 << 7); } // The pixel format has an alpha channel. This is set on all formats that support alpha in some way, including AV_PIX_FMT_PAL8. The alpha is always straight, never pre-multiplied.
  bool flagIsBayerPattern()        { return this->flags & (1 << 8); } // The pixel format is following a Bayer pattern
  bool flagIsFloat()               { return this->flags & (1 << 9); } // The pixel format contains IEEE-754 floating point values.

  /**
    * Parameters that describe how pixels are packed.
    * If the format has 2 or 4 components, then alpha is last.
    * If the format has 1 or 2 components, then luma is 0.
    * If the format has 3 or 4 components,
    * if the RGB flag is set then 0 is red, 1 is green and 2 is blue;
    * otherwise 0 is luma, 1 is chroma-U and 2 is chroma-V.
    */
  struct AVComponentDescriptor
  {
    int plane;    ///< which of the 4 planes contains the component
    int step;     ///< Number of elements between 2 horizontally consecutive pixels
    int offset;   ///< Number of elements before the component of the first pixel
    int shift;    ///< number of least significant bits that must be shifted away to get the value
    int depth;    ///< number of bits in the component
  };

  QString aliases;
  AVComponentDescriptor comp[4];

  bool operator==(const AVPixFmtDescriptorWrapper &a);

private:
  bool flagsSupported();
};

/* This class abstracts from the different versions of FFmpeg (and the libraries within it).
 * With a given path, it will try to open all supported library versions, starting with the 
 * newest one. If a new FFmpeg version is released, support for it will have to be added here.
*/
class FFmpegVersionHandler
{
public:
  FFmpegVersionHandler();

  // Try to load the ffmpeg libraries and get all the function pointers.
  bool loadFFmpegLibraries();
  
  QStringList getLibPaths() const { return lib.getLibPaths(); }
  QString getLibVersionString() const;

  // Only these functions can be used to get valid versions of these wrappers (they have to use ffmpeg functions
  // to retrieve the needed information)
  AVCodecIDWrapper getCodecIDWrapper(AVCodecID id);
  AVCodecID getCodecIDFromWrapper(AVCodecIDWrapper &wrapper);
  AVPixFmtDescriptorWrapper getAvPixFmtDescriptionFromAvPixelFormat(AVPixelFormat pixFmt);
  AVPixelFormat getAVPixelFormatFromYUVPixelFormat(YUV_Internals::yuvPixelFormat pixFmt);

  bool configureDecoder(AVCodecContextWrapper &decCtx, AVCodecParametersWrapper &codecpar);

  // Push a packet to the given decoder using avcodec_send_packet
  int pushPacketToDecoder(AVCodecContextWrapper & decCtx, AVPacketWrapper &pkt);
  // Retrive a frame using avcodec_receive_frame
  int getFrameFromDecoder(AVCodecContextWrapper & decCtx, AVFrameWrapper &frame);

  void flush_buffers(AVCodecContextWrapper &decCtx) { lib.avcodec_flush_buffers(decCtx.getCodec()); }

  FFmpegLibraryVersion libVersion;

  // These FFmpeg versions are supported. The numbers indicate the major version of 
  // the following libraries in this order: Util, codec, format, swresample
  // The versions are sorted from newest to oldest, so that we try to open the newest ones first.
  enum FFmpegVersions
  {
    FFMpegVersion_56_58_58_3,
    FFMpegVersion_55_57_57_2,
    FFMpegVersion_54_56_56_1,
    FFMpegVersion_Num
  };

  // Open the input file. This will call avformat_open_input and avformat_find_stream_info.
  bool openInput(AVFormatContextWrapper &fmt, QString url);
  // Try to find a decoder for the given codecID
  AVCodecWrapper findDecoder(AVCodecIDWrapper codecID);
  // Allocate the decoder (avcodec_alloc_context3)
  AVCodecContextWrapper allocDecoder(AVCodecWrapper &codec);
  // Set info in the dictionary
  int dictSet(AVDictionaryWrapper &dict, const char *key, const char *value, int flags);
  // Get all entries with the given key (leave empty for all)
  QStringPairList getDictionaryEntries(AVDictionaryWrapper d, QString key, int flags);
  // Allocate a new set of codec parameters 
  AVCodecParametersWrapper allocCodecParameters();

  // Open the codec
  int avcodecOpen2(AVCodecContextWrapper &decCtx, AVCodecWrapper &codec, AVDictionaryWrapper &dict);
  // Get side/meta data
  AVFrameSideDataWrapper getSideData(AVFrameWrapper &frame, AVFrameSideDataType type);
  AVDictionaryWrapper    getMetadata(AVFrameWrapper &frame);

  // Seek to a specific frame
  int seekFrame(AVFormatContextWrapper &fmt, int stream_idx, int dts);
  int seekBeginning(AVFormatContextWrapper & fmt);

  // All the function pointers of the ffmpeg library
  FFmpegLibraryFunctions lib;
  
  static AVPixelFormat convertYUVAVPixelFormat(YUV_Internals::yuvPixelFormat fmt);
  // Check if the given four files can be used to open FFmpeg.
  static bool checkLibraryFiles(QString avCodecLib, QString avFormatLib, QString avUtilLib, QString swResampleLib, QStringList &logging);

  // Logging. By default we set the logging level of ffmpeg to AV_LOG_ERROR (Log errors and everything worse)
  static QStringList getFFmpegLog() { return logListFFmpeg; }
  void enableLoggingWarning();

  QStringList getLog() const { return logList; }

private:

  // Try to load the FFmpeg libraries from the given path.
  // Try the system paths if no path is provided. This function can be called multiple times.
  bool loadFFmpegLibraryInPath(QString path);
  // Try to load the four specific library files
  bool loadFFMpegLibrarySpecific(QString avFormatLib, QString avCodecLib, QString avUtilLib, QString swResampleLib);
  bool librariesLoaded;

  // Check the version of the opened libraries
  bool checkLibraryVersions();

  // Map from a certain FFmpegVersions to the major version numbers of the libraries.
  int getLibVersionUtil(FFmpegVersions ver);
  int getLibVersionCodec(FFmpegVersions ver);
  int getLibVersionFormat(FFmpegVersions ver);
  int getLibVersionSwresample(FFmpegVersions ver);

  // Log what is happening when loading the libraries / opening files.
  void log(QString f, QString s) { logList.append(f + " " + s); }
  QStringList logList;

  // FFmpeg has a callback where it loggs stuff. This log goes here.
  static QStringList logListFFmpeg;
  static void avLogCallback(void *ptr, int level, const char *fmt, va_list vargs);
};
