/* -----------------------------------------------------------------------------
The copyright in this software is being made available under the BSD
License, included below. No patent rights, trademark rights and/or 
other Intellectual Property Rights other than the copyrights concerning 
the Software are granted under this license.

For any license concerning other Intellectual Property rights than the software, 
especially patent licenses, a separate Agreement needs to be closed. 
For more information please contact:

Fraunhofer Heinrich Hertz Institute
Einsteinufer 37
10587 Berlin, Germany
www.hhi.fraunhofer.de/vvc
vvc@hhi.fraunhofer.de

Copyright (c) 2018-2020, Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of Fraunhofer nor the names of its contributors may
   be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.


------------------------------------------------------------------------------------------- */

#pragma once

/** \file libvvdec.h
 * Decoder library public API definition.
 * This public API defines the functions that are exported by the dynamic library libvvdec.
 * The API can be used like this:
 * \code{.cpp}
 * libvvdec_context *decCtx = libvvdec_new_decoder();
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
 *   bool check_output;
 *   if (libvvdec_push_nal_unit(decCtx, data, length, check_output) != LIBvvdec_OK)
 *     handle_the_error();
 *
 *   // Next, check the output
 *   if (check_output)
 *   {
 *     auto picData = libvvdec_get_picture_plane(decCtx, LIBvvdec_LUMA);
 *     if (!picData)
 *       handle_error();
 *     else
 *     {
 *       auto width = libvvdec_get_picture_width(pic, LIBvvdec_LUMA);
 *       auto height = libvvdec_get_picture_height(pic, LIBvvdec_LUMA);
 *       // ... Save the frame data
 *     }
 *   }
 * }
 * // Flush
 * if (libvvdec_push_nal_unit(decCtx, nullptr, 0, check_output) != LIBvvdec_OK)
 *   handle_the_error();
 * while(true)
 * {
 *   auto picData = libvvdec_get_picture_plane(decCtx, LIBvvdec_LUMA);
 *   if (!picData)
 *     break;
 *   // As above get the frame and do something with it.
 *   if (libvvdec_push_nal_unit(decCtx, nullptr, 0, check_output) != LIBvvdec_OK)
 *     handle_the_error();
 * }
 * 
 * libvvdec_free_decoder(decCtx);
 * \endcode
 */

#ifdef __cplusplus
extern "C" {
#endif

//#include <stdint.h>
#include <cstdint>

#if defined(_MSC_VER)
#define vvdecAPI __declspec(dllexport)
#else
#define vvdecAPI
#endif

typedef enum
{
  LIBvvdec_OK = 0,            ///< No error occured
  LIBvvdec_ERROR,             ///< There was an unspecified error
  LIBvvdec_ERROR_READ_ERROR   ///< There was an error reading the provided data
} libvvdec_error;

typedef enum
{
  LIBvvdec_LOGLEVEL_SILENT  = 0,
  LIBvvdec_LOGLEVEL_ERROR   = 1,
  LIBvvdec_LOGLEVEL_WARNING = 2,
  LIBvvdec_LOGLEVEL_INFO    = 3,
  LIBvvdec_LOGLEVEL_NOTICE  = 4,
  LIBvvdec_LOGLEVEL_VERBOSE = 5,
  LIBvvdec_LOGLEVEL_DETAILS = 6
} libvvdec_loglevel;

typedef void (*libvvdec_logging_callback)(void*, int, const char*);

/** Get info about the decoder version (e.g. "2.0")
 */
vvdecAPI const char *libvvdec_get_version(void);

/** This private structure is the decoder.
 * You can save a pointer to it and use all the following functions to access it
 * but it is not further defined as part of the public API.
 */
typedef void libvvdec_context;

/** Allocate and initialize a new decoder.
  * \return Returns a pointer to the new decoder or NULL if an error occurred.
  */
vvdecAPI libvvdec_context* libvvdec_new_decoder(void);

/** Set a logging callback with the given log level.
 */
vvdecAPI libvvdec_error libvvdec_set_logging_callback(libvvdec_context* decCtx, libvvdec_logging_callback callback, void *userData, libvvdec_loglevel loglevel);

/** Destroy an existing decoder.
 * \param decCtx The decoder context to destroy that was created with libvvdec_new_decoder
 * \return Return an error code or LIBvvdec_OK if no error occured.
 */
vvdecAPI libvvdec_error libvvdec_free_decoder(libvvdec_context* decCtx);

/** Push a single NAL unit into the decoder (excluding the start code but including the NAL unit header).
 * This will perform decoding of the NAL unit. It can be multiple NAL units (e.g. a full AU). 
 * If you want to swithc the decoder to flushing mode, push a nullptr as data8. Do this again to advance to the next picture in flushing mode.
 * \param decCtx The decoder context that was created with libvvdec_new_decoder
 * \param data8 The raw byte data from the NAL unit starting with the first byte of the NAL unit header. Push nullptr to signal EOF.
 * \param length The length in number of bytes in the data
 * \param checkOutputPictures This bool is set by the function if pictures are available (see libvvdec_get_picture).
 * \return An error code or LIBvvdec_OK if no error occured
 */
vvdecAPI libvvdec_error libvvdec_push_nal_unit(libvvdec_context *decCtx, const unsigned char* data8, int length, bool &checkOutputPictures);

/** The YUV color components
 */
typedef enum
{
  LIBvvdec_LUMA = 0,
  LIBvvdec_CHROMA_U,
  LIBvvdec_CHROMA_V
} libvvdec_ColorComponent;

/** Get the POC of the given picture.
 * By definition, the POC of pictures obtained from libvvdec_get_picture must be strictly increasing.
 * However, the increase may be non-linear.
 * \param pic The libvvdec_picture that was obtained using libvvdec_get_picture
 * \return The picture order count (POC) of the picture.
 */
vvdecAPI uint64_t libvvdec_get_picture_POC(libvvdec_context *decCtx);

/** Get the width/height in pixel of the given picture.
 * This is including internal padding and the conformance window.
 * \param pic The libvvdec_picture that was obtained using libvvdec_get_picture.
 * \param c The color component. Note: The width/height for the luma and chroma components can differ.
 * \return The width/height of the picture in pixel.
 */
vvdecAPI uint32_t libvvdec_get_picture_width(libvvdec_context *decCtx, libvvdec_ColorComponent c);

/** @copydoc libvvdec_get_picture_width(libvvdec_picture *,libvvdec_ColorComponent)
 */
vvdecAPI uint32_t libvvdec_get_picture_height(libvvdec_context *decCtx, libvvdec_ColorComponent c);

/** Get the picture stride (the number of values to reach the next Y-line).
 * Do not confuse the stride and the width of the picture. Internally, the picture buffer may be wider than the output width.
 * \param pic The libvvdec_picture that was obtained using libvvdec_get_picture.
 * \param c The color component. Note: The stride for the luma and chroma components can differ.
 * \return The picture stride
 */
vvdecAPI int32_t libvvdec_get_picture_stride(libvvdec_context *decCtx, libvvdec_ColorComponent c);

/** Get access to the raw image plane.
 * The pointer will point to the top left pixel position. You can read "width" pixels from it.
 * Add "stride" to the pointer to advance to the corresponding pixel in the next line.
 * Internally, there may be padding/a conformance window. The returned pointer may thusly not point
 * to the top left pixel of the internal buffer.
 * \param pic The libvvdec_picture that was obtained using libvvdec_get_picture.
 * \param c The color component to access. Note that the width and stride may be different for the chroma components.
 * \return A pointer to the values as short. For 8-bit output, the upper 8 bit are zero and can be ignored.
 */
vvdecAPI unsigned char *libvvdec_get_picture_plane(libvvdec_context *decCtx, libvvdec_ColorComponent c);

/** The YUV subsampling types
*/
typedef enum
{
  LIBvvdec_CHROMA_400 = 0,  ///< Monochrome (no chroma components)
  LIBvvdec_CHROMA_420,      ///< 4:2:0 - Chroma subsampled by a factor of 2 in horizontal and vertical direction
  LIBvvdec_CHROMA_422,      ///< 4:2:2 - Chroma subsampled by a factor of 2 in horizontal direction
  LIBvvdec_CHROMA_444,      ///< 4:4:4 - No chroma subsampling
  LIBvvdec_CHROMA_UNKNOWN
} libvvdec_ChromaFormat;

/** Get the YUV subsampling type of the given picture.
 * \param pic The libvvdec_picture that was obtained using libvvdec_get_picture.
 * \return The pictures subsampling type
 */
vvdecAPI libvvdec_ChromaFormat libvvdec_get_picture_chroma_format(libvvdec_context *decCtx);

/** Get the bit depth which is used internally for the given color component and the given picture.
 * \param pic The picture to get the internal bit depth from
 * \param c The color component
 * \return The internal bit depth
 */
vvdecAPI uint32_t libvvdec_get_picture_bit_depth(libvvdec_context *decCtx, libvvdec_ColorComponent c);

#ifdef __cplusplus
}
#endif
