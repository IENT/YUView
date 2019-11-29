/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2019, ITU/ISO/IEC
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

 /** \file libVTMDecoder.h
  * Decoder library public API definition.
  * This public API defines the functions that are exported by the dynamic library libVTMDecoder.
  * The API can be used like this:
  * \code{.cpp}
  * libVTMDec_context *decCtx = libVTMDec_new_decoder();
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
  *   if (libVTMDec_push_nal_unit(decCtx, data, length, eof, new_picture, check_output) != LIBVTMDEC_OK)
  *     handle_the_error();
  *   if (!new_picture)
  *   {
  *     // The NAL was consumed and the next NAL can be pushed in the next call to libVTMDec_push_nal_unit
  *     data = NULL;
  *     legth = -1;
  *   }
  *
  *   // Next, check the output
  *   if (check_output)
  *   {
  *     libVTMDec_picture *pic = libVTMDec_get_picture(decCtx);
  *     while(pic != NULL)
  *     {
  *       // Do something with the picture...
  *       int width = libVTMDec_get_picture_width(pic, LIBVTMDEC_LUMA);
  *       // Go to the next picture until no more pictures are present.
  *       pic = libVTMDec_get_picture(decCtx);
  *     }
  *   }
  * }
  * LIBVTMDEC_free_decoder(decCtx);
  * \endcode
  */

#ifndef libVTMDecoder_H
#define libVTMDecoder_H

#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#if defined(_MSC_VER)
#define VTM_DEC_API __declspec(dllexport)
#else
#define VTM_DEC_API
#endif

  //! \ingroup libVTMDecoder
  //! \{

  /** The error codes that are returned if something goes wrong
   */
  typedef enum
  {
    LIBVTMDEC_OK = 0,            ///< No error occured
    LIBVTMDEC_ERROR,             ///< There was an unspecified error
    LIBVTMDEC_ERROR_READ_ERROR   ///< There was an error reading the provided data
  } libVTMDec_error;

  /** Get info about the VTM decoder version (e.g. "5.0")
   */
  VTM_DEC_API const char *libVTMDec_get_version(void);

  /** This private structure is the decoder.
   * You can save a pointer to it and use all the following functions to access it
   * but it is not further defined as part of the public API.
   */
  typedef void libVTMDec_context;

  /** Allocate and initialize a new decoder.
    * \return Returns a pointer to the new decoder or NULL if an error occurred.
    */
  VTM_DEC_API libVTMDec_context* libVTMDec_new_decoder(void);

  /** Destroy an existing decoder.
   * \param decCtx The decoder context to destroy that was created with libVTMDec_new_decoder
   * \return Return an error code or LIBVTMDEC_OK if no error occured.
   */
  VTM_DEC_API libVTMDec_error libVTMDec_free_decoder(libVTMDec_context* decCtx);

  /** Enable/disable checking the SEI hash (default enabled).
   * This should be set before the first NAL unit is pushed to the decoder.
   * \param decCtx The decoder context that was created with libVTMDec_new_decoder
   * \param check_hash If set, the hash in the bitstream (if present) will be checked against the reconstruction.
   */
  VTM_DEC_API void libVTMDec_set_SEI_Check(libVTMDec_context* decCtx, bool check_hash);
  /** Set the maximum temporal layer to decode.
   * By default, this is set to -1 (no limit).
   * This should be set before the first NAL unit is pushed to the decoder.
   * \param decCtx The decoder context that was created with libVTMDec_new_decoder
   * \param max_layer Maximum layer id. -1 means no limit (all layers).
   */
  VTM_DEC_API void libVTMDec_set_max_temporal_layer(libVTMDec_context* decCtx, int max_layer);

  /** Push a single NAL unit into the decoder (excluding the start code but including the NAL unit header).
   * This will perform decoding of the NAL unit. It must be exactly one NAL unit and the data array must
   * not be empty.
   * \param decCtx The decoder context that was created with libVTMDec_new_decoder
   * \param data8 The raw byte data from the NAL unit starting with the first byte of the NAL unit header.
   * \param length The length in number of bytes in the data
   * \param eof Is this NAL the last one in the bitstream?
   * \param bNewPicture This bool is set by the function if the NAL unit must be pushed to the decoder again after reading frames.
   * \param checkOutputPictures This bool is set by the function if pictures might be available (see libVTMDec_get_picture).
   * \return An error code or LIBVTMDEC_OK if no error occured
   */
  VTM_DEC_API libVTMDec_error libVTMDec_push_nal_unit(libVTMDec_context *decCtx, const void* data8, int length, bool eof, bool &bNewPicture, bool &checkOutputPictures);

  /** This private structure represents a picture.
   * You can save a pointer to it and use all the following functions to access it
   * but it is not further defined as part of the public API.
   */
  typedef void libVTMDec_picture;

  /** The YUV color components
   */
  typedef enum
  {
    LIBVTMDEC_LUMA = 0,
    LIBVTMDEC_CHROMA_U,
    LIBVTMDEC_CHROMA_V
  } libVTMDec_ColorComponent;

  /** Get the next output picture from the decoder if one is read for output.
   * When the checkOutputPictures flag was set in the last call of libVTMDec_push_nal_unit, call this
   * function until no more pictures are available.
   * \param decCtx The decoder context that was created with libVTMDec_new_decoder
   * \return Returns a pointer to a libVTMDec_picture or NULL if no picture is ready for output.
  */
  VTM_DEC_API libVTMDec_picture *libVTMDec_get_picture(libVTMDec_context* decCtx);

  /** Get the POC of the given picture.
   * By definition, the POC of pictures obtained from libVTMDec_get_picture must be strictly increasing.
   * However, the increase may be non-linear.
   * \param pic The libVTMDec_picture that was obtained using libVTMDec_get_picture
   * \return The picture order count (POC) of the picture.
   */
  VTM_DEC_API int libVTMDec_get_POC(libVTMDec_picture *pic);

  /** Get the width/height in pixel of the given picture.
   * This is excluding internal padding. Also, the conformance window is not considered.
   * \param pic The libVTMDec_picture that was obtained using libVTMDec_get_picture.
   * \param c The color component. Note: The width/height for the luma and chroma components can differ.
   * \return The width/height of the picture in pixel.
   */
  VTM_DEC_API int libVTMDec_get_picture_width(libVTMDec_picture *pic, libVTMDec_ColorComponent c);

  /** @copydoc libVTMDec_get_picture_width(libVTMDec_picture *,LIBVTMDec_ColorComponent)
   */
  VTM_DEC_API int libVTMDec_get_picture_height(libVTMDec_picture *pic, libVTMDec_ColorComponent c);

  /** Get the picture stride (the number of values to reach the next Y-line).
   * Do not confuse the stride and the width of the picture. Internally, the picture buffer may be wider than the output width.
   * \param pic The libVTMDec_picture that was obtained using libVTMDec_get_picture.
   * \param c The color component. Note: The stride for the luma and chroma components can differ.
   * \return The picture stride
   */
  VTM_DEC_API int libVTMDec_get_picture_stride(libVTMDec_picture *pic, libVTMDec_ColorComponent c);

  /** Get access to the raw image plane.
   * The pointer will point to the top left pixel position. You can read "width" pixels from it.
   * Add "stride" to the pointer to advance to the corresponding pixel in the next line.
   * \param pic The libVTMDec_picture that was obtained using libVTMDec_get_picture.
   * \param c The color component to access. Note that the width and stride may be different for the chroma components.
   * \return A pointer to the values as short. For 8-bit output, the upper 8 bit are zero and can be ignored.
   */
  VTM_DEC_API const int16_t *libVTMDec_get_image_plane(libVTMDec_picture *pic, libVTMDec_ColorComponent c);

  /** The YUV subsampling types
  */
  typedef enum
  {
    LIBVTMDEC_CHROMA_400 = 0,  ///< Monochrome (no chroma components)
    LIBVTMDEC_CHROMA_420,      ///< 4:2:0 - Chroma subsampled by a factor of 2 in horizontal and vertical direction
    LIBVTMDEC_CHROMA_422,      ///< 4:2:2 - Chroma subsampled by a factor of 2 in horizontal direction
    LIBVTMDEC_CHROMA_444,      ///< 4:4:4 - No chroma subsampling
    LIBVTMDEC_CHROMA_UNKNOWN
  } libVTMDec_ChromaFormat;

  /** Get the YUV subsampling type of the given picture.
   * \param pic The libVTMDec_picture that was obtained using libVTMDec_get_picture.
   * \return The pictures subsampling type
   */
  VTM_DEC_API libVTMDec_ChromaFormat libVTMDec_get_chroma_format(libVTMDec_picture *pic);

  /** Get the bit depth which is used for the given picture and given color component.
   * \param c The color component
   * \param pic The picture to get the bit depth from
   * \return The internal bit depth
   */
  VTM_DEC_API int libVTMDec_get_internal_bit_depth(libVTMDec_picture *pic, libVTMDec_ColorComponent c);

  /** This struct is used to retrive internal coding data for a picture.
   * A block is defined by its position within an image (x,y) and its size (w,h).
   * The associated value is saved in 'value'. Its meaning depends on the LIBVTMDEC_info_type.
   * Some values (vectors) may have two values.
   */
  typedef struct
  {
    unsigned short x, y, w, h;
    int value;
    int value2;
  } libVTMDec_BlockValue;

  // TODO: Internals -> How to handle data compression? (pred mode and motion information)?

  typedef enum
  {
    LIBVTMDEC_CTU_SLICE_INDEX = 0,
    LIBVTMDEC_CU_PREDICTION_MODE      ///< Does the CU use inter (0) or intra(1) prediction?
  } libVTMDec_info_type;

  /** Get the internal coding information from the picture.
   * The pointer to the returned vector is always valid. However, the vector is changed if LIBVTMDEC_get_internal_info
   * or LIBVTMDEC_clear_internal_info is called.
   * \warning You must not alter the returned vector. Reading is ok but do not modify it!
   * \param decCtx The decoder context that was created with libVTMDec_new_decoder
   * \param pic The libVTMDec_picture that was obtained using libVTMDec_get_picture.
   * \param type The type of data that you would like to obtain
   * \return A pointer to the vector of block data
   */
  VTM_DEC_API std::vector<libVTMDec_BlockValue> *libVTMDec_get_internal_info(libVTMDec_context *decCtx, libVTMDec_picture *pic, libVTMDec_info_type type);

  /** Clear the internal storage for the internal info (the pointer returned by LIBVTMDEC_get_internal_info).
   * If you no longer need the info in the internals vector, you can call this to free some space.
   * \param decCtx The decoder context that was created with libVTMDec_new_decoder
   * \return An error code or LIBVTMDEC_OK if no error occured
   */
  VTM_DEC_API libVTMDec_error libVTMDec_clear_internal_info(libVTMDec_context *decCtx);

#ifdef __cplusplus
}
#endif

//! \}

#endif // !libVTMDecoder_H

