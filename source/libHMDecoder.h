
#ifndef LIBHMDECODER_H
#define LIBHMDECODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#if defined(_MSC_VER)
#define HM_DEC_API __declspec(dllexport)
#else
#define HM_DEC_API
#endif

// The error codes
typedef enum
{
  LIBHMDEC_OK = 0,
  LIBHMDEC_ERROR,
  LIBHMDEC_ERROR_READ_ERROR
} libHMDec_error;

// Get info about the decoder version
HM_DEC_API const char *libHMDec_get_version(void);

// This private structure is the decoder. You can save a pointer to it
// and use all the following functions to access it.
typedef void libHMDec_context;

// Allocate a new decoder / free an existing decoder
HM_DEC_API libHMDec_context* libHMDec_new_decoder(void);
HM_DEC_API libHMDec_error    libHMDec_free_decoder(libHMDec_context*);

/* The following functions can be used to configure the deoder.
 * These are best set before you start decoding.
*/
// Enable/disable checking the SEI hash (default enabled).
HM_DEC_API void libHMDec_set_SEI_Check(libHMDec_context* decCtx, bool check_hash);
// Set the maximum temporal layer to decode. -1: All layers (default)
HM_DEC_API void libHMDec_set_max_temporal_layer(libHMDec_context* decCtx, int max_layer);

// Push a single NAL unit into the decoder (excluding the start code but including the NAL unit header).
// This will perform decoding.
HM_DEC_API libHMDec_error libHMDec_push_nal_unit(libHMDec_context *decCtx, const void* data8, int length, bool eof, bool &bNewPicture, bool &checkOutputPictures);

// This pcrivate structure represents a picture. You can save a pointer to it and
// use the subsequent functions to access it.
typedef void libHMDec_picture;

typedef enum
{
  LIBHMDEC_LUMA = 0,
  LIBHMDEC_CHROMA_U,
  LIBHMDEC_CHROMA_V
} libHMDec_ColorComponent;

typedef enum
{
  LIBHMDEC_CHROMA_400 = 0,
  LIBHMDEC_CHROMA_420,
  LIBHMDEC_CHROMA_422,
  LIBHMDEC_CHROMA_444,
  LIBHMDEC_CHROMA_UNKNOWN
} libHMDec_ChromaFormat;


// Get the next output picture. NULL if no picture can be read right now.
HM_DEC_API libHMDec_picture *libHMDec_get_picture(libHMDec_context* decCtx);
// Get info from the picture
HM_DEC_API int libHMDEC_get_POC(libHMDec_picture *pic);
HM_DEC_API int libHMDEC_get_picture_width(libHMDec_picture *pic, libHMDec_ColorComponent c);
HM_DEC_API int libHMDEC_get_picture_height(libHMDec_picture *pic, libHMDec_ColorComponent c);
HM_DEC_API int libHMDEC_get_picture_stride(libHMDec_picture *pic, libHMDec_ColorComponent c);
HM_DEC_API short *libHMDEC_get_image_plane(libHMDec_picture *pic, libHMDec_ColorComponent c);
HM_DEC_API libHMDec_ChromaFormat libHMDEC_get_chroma_format(libHMDec_picture *pic);

HM_DEC_API int libHMDEC_get_internal_bit_depth(libHMDec_ColorComponent c);

#ifdef __cplusplus
}
#endif

#endif // !LIBHMDECODER_H

