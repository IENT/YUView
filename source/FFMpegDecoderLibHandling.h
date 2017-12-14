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

#ifndef FFMPEGDECODERLIBHANDLING_H

#include "FFMpegDecoderCommonDefs.h"
#include "stdint.h"
#include <assert.h>
#include <QLibrary>

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
  
  // If loadFFmpegLibraryInPath returned false, this contains a string why.
  QString getErrorString() const { return errorString; }

  QString getLibPath() const { return libPath; }

  // From avformat
  void     (*av_register_all)           ();
  int      (*avformat_open_input)       (AVFormatContext **ps, const char *url, AVInputFormat *fmt, AVDictionary **options);
  void     (*avformat_close_input)      (AVFormatContext **s);
  int      (*avformat_find_stream_info) (AVFormatContext *ic, AVDictionary **options);
  int      (*av_read_frame)             (AVFormatContext *s, AVPacket *pkt);
  int      (*av_seek_frame)             (AVFormatContext *s, int stream_index, int64_t timestamp, int flags);
  unsigned (*avformat_version)          (void);

  // From avcodec
  AVCodec        *(*avcodec_find_decoder)   (AVCodecID id);
  AVCodecContext *(*avcodec_alloc_context3) (const AVCodec *codec);
  int             (*avcodec_open2)          (AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
  void            (*avcodec_free_context)   (AVCodecContext **avctx);
  void            (*av_init_packet)         (AVPacket *pkt);
  void            (*av_packet_unref)        (AVPacket *pkt);
  void            (*avcodec_flush_buffers)  (AVCodecContext *avctx);
  unsigned        (*avcodec_version)        (void);
  const char     *(*avcodec_get_name)       (AVCodecID id);

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
  AVFrame         *(*av_frame_alloc)  (void);
  void             (*av_frame_free)   (AVFrame **frame);
  void            *(*av_mallocz)      (size_t size);
  unsigned         (*avutil_version)  (void);
  int              (*av_dict_set)     (AVDictionary **pm, const char *key, const char *value, int flags);
  AVFrameSideData *(*av_frame_get_side_data) (const AVFrame *frame, AVFrameSideDataType type);

  // From swresample
  unsigned  (*swresample_version) (void);

protected:
  // The library was loaded from this path
  QString libPath;

private:
  // bind all functions from the loaded QLibraries.
  bool bindFunctionsFromLibraries();

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

  // Error handling
  bool setLibraryError(const QString &reason) { errorString = reason; return false; }
  QString errorString;

  QLibrary libAvutil;
  QLibrary libSwresample;
  QLibrary libAvcodec;
  QLibrary libAvformat;
};

/* This class abstracts from the different versions of FFmpeg (and the libraries within it).
 * With a given path, it will try to open all supported library versions, starting with the 
 * newest one. If a new FFmpeg version is released, support for it will have to be added here.
*/
class FFmpegVersionHandler
{
public:
  FFmpegVersionHandler();

  QString getErrorString() const { return lib.getErrorString(); }

  // Try to load the FFmpeg libraries from the given path.
  // Try the system paths if no path is provided. This function can be called multiple times.
  bool loadFFmpegLibraryInPath(QString path);
  // Try to load the four specific library files
  bool loadFFMpegLibrarySpecific(QString avFormatLib, QString avCodecLib, QString avUtilLib, QString swResampleLib);

  // Check if the given four files can be used to open FFmpeg.
  static bool checkLibraryFiles(QString avCodecLib, QString avFormatLib, QString avUtilLib, QString swResampleLib, QString &error);

  // Get values from AVPacket
  AVPacket *getNewPacket();
  void deletePacket(AVPacket *pkt);
  int AVPacketGetStreamIndex(AVPacket *pkt);
  int64_t AVPacketGetPTS(AVPacket *pkt);
  int AVPacketGetDuration(AVPacket *pkt);
  int AVPacketGetFlags(AVPacket *pkt);

  // AVContext related functions
  AVPixelFormat AVCodecContextGetPixelFormat(AVCodecContext *codecCtx);
  int AVCodecContexGetWidth(AVCodecContext *codecCtx);
  int AVCodecContextGetHeight(AVCodecContext *codecCtx);
  AVColorSpace AVCodecContextGetColorSpace(AVCodecContext *codecCtx);

  // AVCodecParameters related functions
  int AVCodecParametersGetWidth(AVCodecParameters *param);
  int AVCodecParametersGetHeight(AVCodecParameters *param);
  AVColorSpace AVCodecParametersGetColorSpace(AVCodecParameters *param);

  // AVFormatContext related functions
  /*AVInputFormat *AVFormatContextGetAVInputFormat(AVFormatContext *fmtCtx);
  AVMediaType AVFormatContextGetCodecTypeFromCodec(AVFormatContext *fmtCtx, int streamIdx);
  AVCodecID AVFormatContextGetCodecIDFromCodec(AVFormatContext *fmtCtx, int streamIdx);
  AVMediaType AVFormatContextGetCodecTypeFromCodecpar(AVFormatContext *fmtCtx, int streamIdx);
  AVCodecID AVFormatContextGetCodecIDFromCodecpar(AVFormatContext *fmtCtx, int streamIdx);
  AVRational AVFormatContextGetAvgFrameRate(AVFormatContext *fmtCtx, int streamIdx);
  int64_t AVFormatContextGetDuration(AVFormatContext *fmtCtx);
  AVRational AVFormatContextGetTimeBase(AVFormatContext *fmtCtx, int streamIdx);*/

  bool AVCodecContextCopyParameters(AVCodecContext *srcCtx, AVCodecContext *dstCtx);

  // AVFrame related functions
  int AVFrameGetWidth(AVFrame *frame);
  int AVFrameGetHeight(AVFrame *frame);
  int64_t AVFrameGetPTS(AVFrame *frame);
  AVPictureType AVFrameGetPictureType(AVFrame *frame);
  int AVFrameGetKeyFrame(AVFrame *frame);
  int AVFrameGetLinesize(AVFrame *frame, int idx);
  uint8_t *AVFrameGetData(AVFrame *frame, int idx);

  // AVFrameSideData
  AVFrameSideDataType getSideDataType(AVFrameSideData *sideData);
  uint8_t *getSideDataData(AVFrameSideData *sideData);
  int getSideDataNrMotionVectors(AVFrameSideData *sideData);

  // AVMotionVector
  void getMotionVectorValues(AVMotionVector *mv, int idx, int32_t &source, uint8_t &blockWidth, uint8_t &blockHeight, int16_t &src_x, int16_t &src_y, int16_t &dst_x, int16_t &dst_y );
  
  QString getLibPath() const { return lib.getLibPath(); }
  QString getLibVersionString() const;

  struct libraryVersion
  {
    int avutil;
    int swresample;
    int avcodec;
    int avformat;
    int avutil_minor;
    int swresample_minor;
    int avcodec_minor;
    int avformat_minor;
    int avutil_micro;
    int swresample_micro;
    int avcodec_micro;
    int avformat_micro;
  };
  libraryVersion libVersion;

  // These FFmpeg versions are supported. The numbers indicate the major version of 
  // the following libraries in this order: Util, codec, format, swresample
  // The versions are sorted from newest to oldest, so that we try to open the newest ones first.
  enum FFmpegVersions
  {
    FFMpegVersion_55_57_57_2,
    FFMpegVersion_54_56_56_1,
    FFMpegVersion_Num
  };

  class AVInputFormatWrapper
  {
  public:
    AVInputFormatWrapper() { fmt = nullptr; }
    void get_values_from_input_format(AVInputFormat *fmt, libraryVersion libVer);
    explicit operator bool() const { return fmt != nullptr; };

    QString name;
    QString long_name;
    int flags;
    QString extensions;
    //const struct AVCodecTag * const *codec_tag;
    //const AVClass *priv_class;
    QString mime_type;

  private:
    AVInputFormat *fmt;
    libraryVersion libVer;
  };

  class AVCodecContextWrapper
  {
  public:
    AVCodecContextWrapper() { codec = nullptr; }
    AVCodecContextWrapper(AVCodecContext *codec, libraryVersion libVer) { get_values_from_codec_context(codec, libVer); }
    void get_values_from_codec_context(AVCodecContext *codec, libraryVersion libVer);
    explicit operator bool() const { return codec != nullptr; };

    AVMediaType codec_type;
    QString codec_name;
    AVCodecID codec_id;
    QString codec_id_string;
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

  private:
    AVCodecContext *codec;
    libraryVersion libVer;
  };

  // This is a version independent wrapper for the version dependent ffmpeg AVCodecParameters.
  // These were added with AVFormat major version 57.
  // It is our own and can be created on the stack and is nicer to debug.
  class AVCodecParametersWrapper
  {
  public:
    AVCodecParametersWrapper() { param = nullptr; }
    AVCodecParametersWrapper(AVCodecParameters *param, libraryVersion libVer) { param = nullptr; get_values_from_parameters(param, libVer); }
    void get_values_from_parameters(AVCodecParameters *param, libraryVersion libVer);
    explicit operator bool() const { return param != nullptr; };

    AVMediaType codec_type;
    AVCodecID   codec_id;
    uint32_t    codec_tag;
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

  private:
    AVCodecParameters *param;
    libraryVersion libVer;
  };

  // This is a version independent wrapper for the version dependent ffmpeg AVStream.
  // It is our own and can be created on the stack and is nicer to debug.
  class AVStreamWrapper
  {
  public:
    AVStreamWrapper() { str = nullptr; }
    AVStreamWrapper(AVStream *src_str, libraryVersion v) { str = src_str; libVer = v; update_values(); }
    void update_values();
    explicit operator bool() const { return str != nullptr; };

    AVMediaType getCodecType();
    AVCodecID getCodecID();

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

  private:
    AVStream *str;
    libraryVersion libVer;
  };

  // This is a version independent wrapper for the version dependent ffmpeg AVFormatContext
  // It is our own and can be created on the stack and is nicer to debug.
  class AVFormatContextWrapper
  {
  public:
    AVFormatContextWrapper() { ctx = nullptr; };
    void get_values_from_format_context(AVFormatContext *ctx, libraryVersion libVer);
    void avformat_close_input(FFmpegVersionHandler &ver);
    explicit operator bool() const { return ctx != nullptr; };

    AVInputFormatWrapper iformat;

    int ctx_flags;
    unsigned int nb_streams;
    QList<AVStreamWrapper> streams;
    QString filename;
    int64_t start_time;
    int64_t duration;
    int bit_rate;
    unsigned int packet_size;
    int max_delay;
    int flags;

  private:
    AVFormatContext *ctx;
    libraryVersion libVer;
  };

  class AVCodecWrapper
  {
  public:
    AVCodecWrapper() { codec = nullptr; };
    AVCodecWrapper(AVCodec *codec, libraryVersion libVer) : codec(codec), libVer(libVer) {};
    explicit operator bool() const { return codec != nullptr; };
    AVCodec *getAVCodec() { return codec; }

  private:
    AVCodec *codec;
    libraryVersion libVer;
  };

  // Open the input file. This will call avformat_open_input and avformat_find_stream_info.
  int open_input(AVFormatContextWrapper &fmt, QString url);
  // Try to find a decoder for the given codecID (avcodec_find_decoder)
  AVCodecWrapper find_decoder(AVCodecID codec_id);
  // Allocate the decoder (avcodec_alloc_context3)
  AVCodecContextWrapper alloc_decoder(AVCodecWrapper codec);

private:

  // All the function pointers of the ffmpeg library
  FFmpegLibraryFunctions lib;

  // Check the version of the opened libraries
  bool checkLibraryVersions();

  // Map from a certain FFmpegVersions to the major version numbers of the libraries.
  int getLibVersionUtil(FFmpegVersions ver);
  int getLibVersionCodec(FFmpegVersions ver);
  int getLibVersionFormat(FFmpegVersions ver);
  int getLibVersionSwresample(FFmpegVersions ver);

  // What error occured while opening the libraries?
  QString versionErrorString;

  // ------------ AVPacket --------
  // AVPacket is part of avcodec. The definition is different for different major versions of avcodec.
  // These are the version independent functions to retrive data from AVPacket.
  typedef struct AVPacket_56
  {
    AVBufferRef *buf;
    int64_t pts;
    int64_t dts;
    uint8_t *data;
    int size;
    int stream_index;
    int flags;
    AVPacketSideData *side_data;
    int side_data_elems;
    int duration;
    void (*destruct)(struct AVPacket *);
    void *priv;
    int64_t pos;
    int64_t convergence_duration;
  } AVPacket_56;

  typedef struct AVPacket_57
  {
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
  } AVPacket_57;

  // ------------ AVCodecContext --------
  // AVCodecContext is part of avcodec

  typedef struct AVCodecContext_56 
  {
    const AVClass *av_class;
    int log_level_offset;

    AVMediaType codec_type; //
    const struct AVCodec *codec;
    char codec_name[32];
    AVCodecID codec_id; // 
    unsigned int codec_tag;
    unsigned int stream_codec_tag;
    void *priv_data;
    struct AVCodecInternal *internal;
    void *opaque;
    int bit_rate; //
    int bit_rate_tolerance;
    int global_quality;
    int compression_level;
    int flags;
    int flags2;
    uint8_t *extradata; //
    int extradata_size; //
    AVRational time_base;
    int ticks_per_frame;
    int delay;
    int width, height;  //
    int coded_width, coded_height;
    int gop_size;
    AVPixelFormat pix_fmt; //
    int me_method;
    void (*draw_horiz_band)(struct AVCodecContext *s, const AVFrame *src, int offset[AV_NUM_DATA_POINTERS], int y, int type, int height);
    AVPixelFormat (*get_format)(struct AVCodecContext *s, const enum AVPixelFormat * fmt);
    int max_b_frames;
    float b_quant_factor;
    int rc_strategy;
    int b_frame_strategy;
    float b_quant_offset;
    int has_b_frames; //
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
    int *slice_offset;
    AVRational sample_aspect_ratio; //
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
    uint16_t *intra_matrix;
    uint16_t *inter_matrix;
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
    AVColorPrimaries color_primaries; //
    AVColorTransferCharacteristic color_trc; //
    AVColorSpace colorspace;  //
    AVColorRange color_range; //
    AVChromaLocation chroma_sample_location; //

    // Actually, there is more here, but the variables above are the only we need.
  } AVCodecContext_56;
  
  typedef struct AVCodecContext_57 
  {
    const AVClass *av_class;
    int log_level_offset;
     AVMediaType codec_type;
    const struct AVCodec *codec;
    char codec_name[32];
    AVCodecID codec_id;
    unsigned int codec_tag;
    unsigned int stream_codec_tag;
    void *priv_data;
    struct AVCodecInternal *internal;
    void *opaque;
    int64_t bit_rate;
    int bit_rate_tolerance;
    int global_quality;
    int compression_level;
    int flags;
    int flags2;
    uint8_t *extradata;
    int extradata_size;
    AVRational time_base;
    int ticks_per_frame;
    int delay;
    int width, height;
    int coded_width, coded_height;
    int gop_size;
    AVPixelFormat pix_fmt;
    int me_method;
    void (*draw_horiz_band)(struct AVCodecContext *s, const AVFrame *src, int offset[AV_NUM_DATA_POINTERS], int y, int type, int height);
    AVPixelFormat (*get_format)(struct AVCodecContext *s, const AVPixelFormat * fmt);
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
    int *slice_offset;
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
    uint16_t *intra_matrix;
    uint16_t *inter_matrix;
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

    // Actually, there is more here, but the variables above are the only we need.
  } AVCodecContext_57;
  
  // ----------- AVCodecParameters ------------
  // AVCodecParameters is part of avcodec.

  typedef struct AVCodecParameters_57
  {
    AVMediaType codec_type;
    AVCodecID   codec_id;
    uint32_t    codec_tag;
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

    // Actually, there is more here, but the variables above are the only we need.
  } AVCodecParameters_57;

  // -------- AVFormatContext --------------
  // AVFormatContext is part of avformat.
  // These functions give us version independent access to the structs.

  typedef struct AVFormatContext_56 
  {
    const AVClass *av_class;
    struct AVInputFormat *iformat;
    struct AVOutputFormat *oformat;
    void *priv_data;
    AVIOContext *pb;
    int ctx_flags;
    unsigned int nb_streams; //
    AVStream **streams; //
    char filename[1024];
    int64_t start_time;
    int64_t duration; //
    int bit_rate;
    unsigned int packet_size;
    int max_delay;
    int flags;
  
    // Actually, there is more here, but the variables above are the only we need.
  } AVFormatContext_56;

  typedef struct AVFormatContext_57
  {
    const AVClass *av_class;
    struct AVInputFormat *iformat;
    struct AVOutputFormat *oformat;
    void *priv_data;
    AVIOContext *pb;
    int ctx_flags;
    unsigned int nb_streams;
    AVStream **streams;
    char filename[1024];
    int64_t start_time;
    int64_t duration;
    int64_t bit_rate;
    unsigned int packet_size;
    int max_delay;
    int flags;

    // Actually, there is more here, but the variables above are the only we need.
  } AVFormatContext_57;
  
  // ------------------- AVFrame ---------------
  // AVFrames is part of AVUtil

  typedef struct AVFrame_55 
  {
    uint8_t *data[AV_NUM_DATA_POINTERS];
    int linesize[AV_NUM_DATA_POINTERS];
    uint8_t **extended_data;
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
  
    // Actually, there is more here, but the variables above are the only we need.
  } AVFrame_55;

  typedef struct AVFrame_54 
  {
    uint8_t *data[AV_NUM_DATA_POINTERS];
    int linesize[AV_NUM_DATA_POINTERS];
    uint8_t **extended_data;
    int width, height;
    int nb_samples;
    int format;
    int key_frame;
    AVPictureType pict_type;
    uint8_t *base[AV_NUM_DATA_POINTERS];
    AVRational sample_aspect_ratio;
    int64_t pts;
    int64_t pkt_pts;
    int64_t pkt_dts;
    int coded_picture_number;
    int display_picture_number;
    int quality;

    // Actually, there is more here, but the variables above are the only we need.
  } AVFrame_54;

  // ------------------- AVFrameSideData ---------------
  // AVFrameSideData is part of AVUtil
  typedef struct AVFrameSideData_54_55
  {
    enum AVFrameSideDataType type;
    uint8_t *data;
    int      size;
    AVDictionary *metadata;
    AVBufferRef *buf;
  } AVFrameSideData_54_55;

  // ------------------- AVInputFormat ---------------
  // AVInputFormat is part of AVFormat
  typedef struct AVInputFormat_56_57
  {
    const char *name;
    const char *long_name;
    int flags;
    const char *extensions;
    const struct AVCodecTag * const *codec_tag;
    const AVClass *priv_class;
    const char *mime_type;
  } AVInputFormat_56_57;

  // ------------------- AVStream ---------------
  // AVStream is part of AVFormat
  typedef struct AVStream_56
  {
    int index;
    int id;
    AVCodecContext *codec;
    void *priv_data;
    struct AVFrac pts;
    AVRational time_base;
    int64_t start_time;
    int64_t duration;
    int64_t nb_frames;
    int disposition;
    enum AVDiscard discard;
    AVRational sample_aspect_ratio;
    AVDictionary *metadata;
    AVRational avg_frame_rate;
    AVPacket_56 attached_pic;
    AVPacketSideData *side_data;
    int nb_side_data;
    int event_flags;
  } AVStream_56;

  typedef struct AVStream_57
  {
    int index;
    int id;
    AVCodecContext *codec;
    void *priv_data;
    AVRational time_base;
    int64_t start_time;
    int64_t duration;
    int64_t nb_frames;
    int disposition;
    enum AVDiscard discard;
    AVRational sample_aspect_ratio;
    AVDictionary *metadata;
    AVRational avg_frame_rate;
    AVPacket_57 attached_pic;
    AVPacketSideData *side_data;
    int nb_side_data;
    int event_flags;
    AVRational r_frame_rate;
    char *recommended_encoder_configuration;
    AVCodecParameters *codecpar;
  } AVStream_57;
  
  // ------------------- AVMotionVector ---------------

  typedef struct AVMotionVector_54
  {
    int32_t source;
    uint8_t w, h;
    int16_t src_x, src_y;
    int16_t dst_x, dst_y;
    uint64_t flags;
  } AVMotionVector_54;

  typedef struct AVMotionVector_55 
  {
    int32_t source;
    uint8_t w, h;
    int16_t src_x, src_y;
    int16_t dst_x, dst_y;
    uint64_t flags;
    int32_t motion_x, motion_y;
    uint16_t motion_scale;
  } AVMotionVector_55;

};

#endif // FFMPEGDECODERLIBHANDLING_H