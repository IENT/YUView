/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2010-2014, ITU/ISO/IEC
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  * Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
*    be used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

/** \file libHMDecoder.h
 * Decoder library public API definition.
 * This public API defines the functions that are exported by the dynamic library libHMDecoder.
 * The API can be used like this:
 * \code{.cpp}
 * libHMDec_context *decCtx = libHMDec_new_decoder();
 * bool eof = false;
 * uint8_t *data = NULL;
 * int length = -1;
 * while (!eof)
 * {
 *   if (data == NULL && length == -1)
 *   {
 *     // Get the next NAL unit to push
 *     data = get_nal_unit_data();
 *     length = get_lenght_of_nal_unit_data();
 *     eof = is_this_the_last_nal_unit();
 *   }
 *
 *   bool new_picture, check_output;
 *   if (libHMDec_push_nal_unit(decCtx, data, length, eof, new_picture, check_output) != LIBHMDEC_OK)
 *     handle_the_error();
 *   if (!new_picture)
 *   {
 *     // The NAL was consumed and the next NAL can be pushed in the next call to libHMDec_push_nal_unit
 *     data = NULL;
 *     legth = -1;
 *   }
 *   
 *   // Next, check the output
 *   if (check_output)
 *   {
 *     libHMDec_picture *pic = libHMDec_get_picture(decCtx);
 *     while(pic != NULL)
 *     {
 *       // Do something with the picture...
 *       int width = libHMDEC_get_picture_width(pic, LIBHMDEC_LUMA);
 *       // Go to the next picture until no more pictures are present.
 *       pic = libHMDec_get_picture(decCtx);
 *     }
 *   }
 * }
 * libHMDec_free_decoder(decCtx);
 * \endcode
 */

#ifndef LIBHMDECODER_H
#define LIBHMDECODER_H

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#if defined(_MSC_VER)
#define HM_DEC_API __declspec(dllexport)
#else
#define HM_DEC_API
#endif

//! \ingroup libHMDecoder
//! \{

/** The error codes that are returned if something goes wrong
 */
typedef enum
{
  LIBHMDEC_OK = 0,            ///< No error occured
  LIBHMDEC_ERROR,             ///< There was an unspecified error
  LIBHMDEC_ERROR_READ_ERROR   ///< There was an error reading the provided data
} libHMDec_error;

/** Get info about the HM decoder version (e.g. "16.0")
 */
HM_DEC_API const char *libHMDec_get_version(void);

/** This private structure is the decoder. 
 * You can save a pointer to it and use all the following functions to access it 
 * but it is not further defined as part of the public API.
 */
typedef void libHMDec_context;

/** Allocate and initialize a new decoder.
  * \return Returns a pointer to the new decoder or NULL if an error occurred.
  */
HM_DEC_API libHMDec_context* libHMDec_new_decoder(void);

/** Destroy an existing decoder.
 * \param decCtx The decoder context to destroy that was created with libHMDec_new_decoder
 * \return Return an error code or LIBHMDEC_OK if no error occured.
 */
HM_DEC_API libHMDec_error libHMDec_free_decoder(libHMDec_context* decCtx);

/** Enable/disable checking the SEI hash (default enabled).
 * This should be set before the first NAL unit is pushed to the decoder.
 * \param decCtx The decoder context that was created with libHMDec_new_decoder
 * \param check_hash If set, the hash in the bitstream (if present) will be checked against the reconstruction.
 */
HM_DEC_API void libHMDec_set_SEI_Check(libHMDec_context* decCtx, bool check_hash);
/** Set the maximum temporal layer to decode.
 * By default, this is set to -1 (no limit).
 * This should be set before the first NAL unit is pushed to the decoder.
 * \param decCtx The decoder context that was created with libHMDec_new_decoder
 * \param max_layer Maximum layer id. -1 means no limit (all layers).
 */
HM_DEC_API void libHMDec_set_max_temporal_layer(libHMDec_context* decCtx, int max_layer);

/** Push a single NAL unit into the decoder (excluding the start code but including the NAL unit header).
 * This will perform decoding of the NAL unit. It must be exactly one NAL unit and the data array must
 * not be empty.
 * \param decCtx The decoder context that was created with libHMDec_new_decoder
 * \param data8 The raw byte data from the NAL unit starting with the first byte of the NAL unit header. 
 * \param length The length in number of bytes in the data
 * \param eof Is this NAL the last one in the bitstream?
 * \param bNewPicture This bool is set by the function if the NAL unit must be pushed to the decoder again after reading frames.
 * \param checkOutputPictures This bool is set by the function if pictures might be available (see libHMDec_get_picture).
 * \return An error code or LIBHMDEC_OK if no error occured
 */
HM_DEC_API libHMDec_error libHMDec_push_nal_unit(libHMDec_context *decCtx, const void* data8, int length, bool eof, bool &bNewPicture, bool &checkOutputPictures);

/** This private structure represents a picture.
 * You can save a pointer to it and use all the following functions to access it 
 * but it is not further defined as part of the public API.
 */
typedef void libHMDec_picture;

/** The YUV color components
 */
typedef enum
{
  LIBHMDEC_LUMA = 0,
  LIBHMDEC_CHROMA_U,
  LIBHMDEC_CHROMA_V
} libHMDec_ColorComponent;

/** Get the next output picture from the decoder if one is read for output.
 * When the checkOutputPictures flag was set in the last call of libHMDec_push_nal_unit, call this
 * function until no more pictures are available.
 * \param decCtx The decoder context that was created with libHMDec_new_decoder
 * \return Returns a pointer to a libHMDec_picture or NULL if no picture is ready for output.
*/
HM_DEC_API libHMDec_picture *libHMDec_get_picture(libHMDec_context* decCtx);

/** Get the POC of the given picture.
 * By definition, the POC of pictures obtained from libHMDec_get_picture must be strictly increasing.
 * However, the increase may be non-linear.
 * \param pic The libHMDec_picture that was obtained using libHMDec_get_picture
 * \return The picture order count (POC) of the picture.
 */
HM_DEC_API int libHMDEC_get_POC(libHMDec_picture *pic);

/** Get the width/height in pixel of the given picture.
 * This is excluding internal padding. Also, the conformance window is not considered.
 * \param pic The libHMDec_picture that was obtained using libHMDec_get_picture.
 * \param c The color component. Note: The width/height for the luma and chroma components can differ.
 * \return The width/height of the picture in pixel.
 */
HM_DEC_API int libHMDEC_get_picture_width(libHMDec_picture *pic, libHMDec_ColorComponent c);

/** @copydoc libHMDEC_get_picture_width(libHMDec_picture *,libHMDec_ColorComponent)
 */
HM_DEC_API int libHMDEC_get_picture_height(libHMDec_picture *pic, libHMDec_ColorComponent c);

/** Get the picture stride (the number of values to reach the next Y-line).
 * Do not confuse the stride and the width of the picture. Internally, the picture buffer may be wider than the output width.
 * \param pic The libHMDec_picture that was obtained using libHMDec_get_picture.
 * \param c The color component. Note: The stride for the luma and chroma components can differ.
 * \return The picture stride
 */
HM_DEC_API int libHMDEC_get_picture_stride(libHMDec_picture *pic, libHMDec_ColorComponent c);

/** Get access to the raw image plane.
 * The pointer will point to the top left pixel position. You can read "width" pixels from it. 
 * Add "stride" to the pointer to advance to the corresponding pixel in the next line.
 * \param pic The libHMDec_picture that was obtained using libHMDec_get_picture.
 * \param c The color component to access. Note that the width and stride may be different for the chroma components.
 * \return A pointer to the values as short. For 8-bit output, the upper 8 bit are zero and can be ignored.
 */
HM_DEC_API short *libHMDEC_get_image_plane(libHMDec_picture *pic, libHMDec_ColorComponent c);

/** The YUV subsampling types
*/
typedef enum
{
  LIBHMDEC_CHROMA_400 = 0,  ///< Monochrome (no chroma components)
  LIBHMDEC_CHROMA_420,      ///< 4:2:0 - Chroma subsampled by a factor of 2 in horizontal and vertical direction
  LIBHMDEC_CHROMA_422,      ///< 4:2:2 - Chroma subsampled by a factor of 2 in horizontal direction
  LIBHMDEC_CHROMA_444,      ///< 4:4:4 - No chroma subsampling
  LIBHMDEC_CHROMA_UNKNOWN
} libHMDec_ChromaFormat;

/** Get the YUV subsampling type of the given picture.
 * \param pic The libHMDec_picture that was obtained using libHMDec_get_picture.
 * \return The pictures subsampling type
 */
HM_DEC_API libHMDec_ChromaFormat libHMDEC_get_chroma_format(libHMDec_picture *pic);

/** Get the bit depth which is used internally for the given color component.
 * \param c The color component
 * \return The internal bit depth
 */
HM_DEC_API int libHMDEC_get_internal_bit_depth(libHMDec_ColorComponent c);

/** This struct is used to retrive internal coding data for a picture.
 * A block is defined by its position within an image (x,y) and its size (w,h).
 * The associated value is saved in 'value'.
 */
typedef struct
{
  unsigned short x, y, w, h;
  int value;
} libHMDec_BlockValue;

// TODO: Internals -> How to handle data compression? (pred mode and motion information)?

typedef enum
{
  LIBHMDEC_PREDICTION_MODE = 0,
  LIBHMDEC_PREDICTION_INTRA_MODE = 0
} libHMDec_info_type;

/** Get the internal coding information from the picture.
 * The pointer to the returned vector is always valid. However, the vector is changed if libHMDEC_get_internal_info
 * or libHMDEC_clear_internal_info is called.
 * \warning You must not alter the returned vector. Reading is ok but do not modify it!
 * \param decCtx The decoder context that was created with libHMDec_new_decoder
 * \param pic The libHMDec_picture that was obtained using libHMDec_get_picture.
 * \param type The type of data that you would like to obtain
 * \return A pointer to the vector of block data
 */
HM_DEC_API std::vector<libHMDec_BlockValue> *libHMDEC_get_internal_info(libHMDec_context *decCtx, libHMDec_picture *pic, libHMDec_info_type type);

/** Clear the internal storage for the internal info (the pointer returned by libHMDEC_get_internal_info).
 * If you no longer need the info in the internals vector, you can call this to free some space.
 * \param decCtx The decoder context that was created with libHMDec_new_decoder
 * \return An error code or LIBHMDEC_OK if no error occured
 */
HM_DEC_API libHMDec_error libHMDEC_clear_internal_info(libHMDec_context *decCtx);

#ifdef __cplusplus
}
#endif

//! \}

#endif // !LIBHMDECODER_H

