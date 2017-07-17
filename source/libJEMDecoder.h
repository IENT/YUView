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

/** \file libJEMDecoder.h
 * Decoder library public API definition.
 * This public API defines the functions that are exported by the dynamic library libJEMDecoder.
 * The API can be used like this:
 * \code{.cpp}
 * libJEMDec_context *decCtx = libJEMDec_new_decoder();
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
 *   if (libJEMDec_push_nal_unit(decCtx, data, length, eof, new_picture, check_output) != LIBJEMDEC_OK)
 *     handle_the_error();
 *   if (!new_picture)
 *   {
 *     // The NAL was consumed and the next NAL can be pushed in the next call to libJEMDec_push_nal_unit
 *     data = NULL;
 *     legth = -1;
 *   }
 *
 *   // Next, check the output
 *   if (check_output)
 *   {
 *     libJEMDec_picture *pic = libJEMDec_get_picture(decCtx);
 *     while(pic != NULL)
 *     {
 *       // Do something with the picture...
 *       int width = libJEMDEC_get_picture_width(pic, LIBJEMDEC_LUMA);
 *       // Go to the next picture until no more pictures are present.
 *       pic = libJEMDec_get_picture(decCtx);
 *     }
 *   }
 * }
 * libJEMDec_free_decoder(decCtx);
 * \endcode
 */

#ifndef LIBJEMDECODER_H
#define LIBJEMDECODER_H

#ifdef __cplusplus
extern "C" {
#endif

//#include <stdint.h>

#if defined(_MSC_VER)
#define JEM_DEC_API __declspec(dllexport)
#else
#define JEM_DEC_API
#endif

//! \ingroup libJEMDecoder
//! \{

/** The error codes that are returned if something goes wrong
 */
typedef enum
{
  LIBJEMDEC_OK = 0,            ///< No error occured
  LIBJEMDEC_ERROR,             ///< There was an unspecified error
  LIBJEMDEC_ERROR_READ_ERROR   ///< There was an error reading the provided data
} libJEMDec_error;

/** The YUV subsampling types
*/
typedef enum
{
  LIBJEMDEC_CHROMA_400 = 0,  ///< Monochrome (no chroma components)
  LIBJEMDEC_CHROMA_420,      ///< 4:2:0 - Chroma subsampled by a factor of 2 in horizontal and vertical direction
  LIBJEMDEC_CHROMA_422,      ///< 4:2:2 - Chroma subsampled by a factor of 2 in horizontal direction
  LIBJEMDEC_CHROMA_444,      ///< 4:4:4 - No chroma subsampling
  LIBJEMDEC_CHROMA_UNKNOWN
} libJEMDec_ChromaFormat;

/** The YUV color components
 */
typedef enum
{
  LIBJEMDEC_LUMA = 0,
  LIBJEMDEC_CHROMA_U,
  LIBJEMDEC_CHROMA_V
} libJEMDec_ColorComponent;

/** Get info about the JEM decoder version (e.g. "16.0")
 */
JEM_DEC_API const char *libJEMDec_get_version(void);

/** This private structure is the decoder.
 * You can save a pointer to it and use all the following functions to access it
 * but it is not further defined as part of the public API.
 */
typedef void libJEMDec_context;

/** Allocate and initialize a new decoder.
  * \return Returns a pointer to the new decoder or NULL if an error occurred.
  */
JEM_DEC_API libJEMDec_context* libJEMDec_new_decoder(void);

/** Destroy an existing decoder.
 * \param decCtx The decoder context to destroy that was created with libJEMDec_new_decoder
 * \return Return an error code or LIBJEMDEC_OK if no error occured.
 */
JEM_DEC_API libJEMDec_error libJEMDec_free_decoder(libJEMDec_context* decCtx);

/** Enable/disable checking the SEI hash (default enabled).
 * This should be set before the first NAL unit is pushed to the decoder.
 * \param decCtx The decoder context that was created with libJEMDec_new_decoder
 * \param check_hash If set, the hash in the bitstream (if present) will be checked against the reconstruction.
 */
JEM_DEC_API void libJEMDec_set_SEI_Check(libJEMDec_context* decCtx, bool check_hash);
/** Set the maximum temporal layer to decode.
 * By default, this is set to -1 (no limit).
 * This should be set before the first NAL unit is pushed to the decoder.
 * \param decCtx The decoder context that was created with libJEMDec_new_decoder
 * \param max_layer Maximum layer id. -1 means no limit (all layers).
 */
JEM_DEC_API void libJEMDec_set_max_temporal_layer(libJEMDec_context* decCtx, int max_layer);

/** Push a single NAL unit into the decoder (excluding the start code but including the NAL unit header).
 * This will perform decoding of the NAL unit. It must be exactly one NAL unit and the data array must
 * not be empty.
 * \param decCtx The decoder context that was created with libJEMDec_new_decoder
 * \param data8 The raw byte data from the NAL unit starting with the first byte of the NAL unit header.
 * \param length The length in number of bytes in the data
 * \param eof Is this NAL the last one in the bitstream?
 * \param bNewPicture This bool is set by the function if the NAL unit must be pushed to the decoder again after reading frames.
 * \param checkOutputPictures This bool is set by the function if pictures might be available (see libJEMDec_get_picture).
 * \return An error code or LIBJEMDEC_OK if no error occured
 */
JEM_DEC_API libJEMDec_error libJEMDec_push_nal_unit(libJEMDec_context *decCtx, const void* data8, int length, bool eof, bool &bNewPicture, bool &checkOutputPictures);

/** Parse the given NAL unit and return the POC of the NAL, or -1 if the NAL does not produce an output picture.
 * The NAL must be excluding the start code but including the NAL unit header.
 * No actual decoding of the NAL unit is performed.
 * \param decCtx The decoder context that was created with libJEMDec_new_decoder
 * \param data8 The raw byte data from the NAL unit starting with the first byte of the NAL unit header.
 * \param length The length in number of bytes in the data
 * \param eof Is this NAL the last one in the bitstream?
 * \param bNewPicture This bool is set by the function if the NAL unit must be pushed to the decoder again after reading frames.
 * \param checkOutputPictures This bool is set by the function if pictures might be available (see libJEMDec_get_picture).
 * \param poc Returns the POC of the given NAL unit
 * \param isRAP is set if this NAL us a random access point
 * \param isParameterSet is set if this NAL is a parameter set
 * \param picWidthLumaSamples The picture width in luma samples (with application of the conformance window)
 * \param picHeightLumaSamples The picture height in luma samples (with application of the conformance window)
 * \param bitDepthLuma Bit depth for luma
 * \param bitDepthChroma Bit depth for chroma
 * \param chromaFormat The chroma format
 * \return An error code or LIBJEMDEC_OK if no error occured
 */
JEM_DEC_API libJEMDec_error libJEMDec_get_nal_unit_info(libJEMDec_context *decCtx, const void* data8, int length, bool eof, int &poc, bool &isRAP, bool &isParameterSet, int &picWidthLumaSamples, int &picHeightLumaSamples, int &bitDepthLuma, int &bitDepthChroma, libJEMDec_ChromaFormat &chromaFormat);

/** This private structure represents a picture.
 * You can save a pointer to it and use all the following functions to access it
 * but it is not further defined as part of the public API.
 */
typedef void libJEMDec_picture;

/** Get the next output picture from the decoder if one is read for output.
 * When the checkOutputPictures flag was set in the last call of libJEMDec_push_nal_unit, call this
 * function until no more pictures are available.
 * \param decCtx The decoder context that was created with libJEMDec_new_decoder
 * \return Returns a pointer to a libJEMDec_picture or NULL if no picture is ready for output.
*/
JEM_DEC_API libJEMDec_picture *libJEMDec_get_picture(libJEMDec_context* decCtx);

/** Get the POC of the given picture.
 * By definition, the POC of pictures obtained from libJEMDec_get_picture must be strictly increasing.
 * However, the increase may be non-linear.
 * \param pic The libJEMDec_picture that was obtained using libJEMDec_get_picture
 * \return The picture order count (POC) of the picture.
 */
JEM_DEC_API int libJEMDEC_get_POC(libJEMDec_picture *pic);

/** Get the width/height in pixel of the given picture.
 * This is including internal padding and the conformance window.
 * \param pic The libJEMDec_picture that was obtained using libJEMDec_get_picture.
 * \param c The color component. Note: The width/height for the luma and chroma components can differ.
 * \return The width/height of the picture in pixel.
 */
JEM_DEC_API int libJEMDEC_get_picture_width(libJEMDec_picture *pic, libJEMDec_ColorComponent c);

/** @copydoc libJEMDEC_get_picture_width(libJEMDec_picture *,libJEMDec_ColorComponent)
 */
JEM_DEC_API int libJEMDEC_get_picture_height(libJEMDec_picture *pic, libJEMDec_ColorComponent c);

/** Get the picture stride (the number of values to reach the next Y-line).
 * Do not confuse the stride and the width of the picture. Internally, the picture buffer may be wider than the output width.
 * \param pic The libJEMDec_picture that was obtained using libJEMDec_get_picture.
 * \param c The color component. Note: The stride for the luma and chroma components can differ.
 * \return The picture stride
 */
JEM_DEC_API int libJEMDEC_get_picture_stride(libJEMDec_picture *pic, libJEMDec_ColorComponent c);

/** Get access to the raw image plane.
 * The pointer will point to the top left pixel position. You can read "width" pixels from it.
 * Add "stride" to the pointer to advance to the corresponding pixel in the next line.
 * Internally, there may be padding/a conformance window. The returned pointer may thusly not point
 * to the top left pixel of the internal buffer.
 * \param pic The libJEMDec_picture that was obtained using libJEMDec_get_picture.
 * \param c The color component to access. Note that the width and stride may be different for the chroma components.
 * \return A pointer to the values as short. For 8-bit output, the upper 8 bit are zero and can be ignored.
 */
JEM_DEC_API short *libJEMDEC_get_image_plane(libJEMDec_picture *pic, libJEMDec_ColorComponent c);

/** Get the YUV subsampling type of the given picture.
 * \param pic The libJEMDec_picture that was obtained using libJEMDec_get_picture.
 * \return The pictures subsampling type
 */
JEM_DEC_API libJEMDec_ChromaFormat libJEMDEC_get_chroma_format(libJEMDec_picture *pic);

/** Get the bit depth which is used internally for the given color component and the given picture.
 * \param pic The picture to get the internal bit depth from
 * \param c The color component
 * \return The internal bit depth
 */
JEM_DEC_API int libJEMDEC_get_internal_bit_depth(libJEMDec_picture *pic, libJEMDec_ColorComponent c);

/** This struct is used to retrive internal coding data for a picture.
 * A block is defined by its position within an image (x,y) and its size (w,h).
 * The associated value is saved in 'value'. Its meaning depends on the libJEMDec_info_type.
 * Some values (vectors) may have two values.
 */
typedef struct
{
  unsigned short x, y, w, h;
  int value;
  int value2;
} libJEMDec_BlockValue;

// -------- Internals -----------
// TODO: Internals -> How to handle data compression? (pred mode and motion information)?

/** How many different types of internals can this decoder provide?
  * \param decCtx The decoder context that was created with libJEMDec_new_decoder
  * \return The number of supported internal types. Can be 0.
  */
JEM_DEC_API unsigned int libJEMDEC_get_internal_type_number();

/** Get the name of ther internals type with the given index.
  * \param decCtx The decoder context that was created with libJEMDec_new_decoder
  * \param idx The index of the internals type (can range from 0 to the value provided by libJEMDEC_get_internal_type_number)
  * \return The name of the type
  */
JEM_DEC_API const char *libJEMDEC_get_internal_type_name(unsigned int idx);

/** Each type can provide different types of data. These are the supported types.
*/
typedef enum
{
  LIBJEMDEC_TYPE_FLAG = 0,         ///< A flag that is either 0 or 1
  LIBJEMDEC_TYPE_RANGE,            ///< A range from 0 to some maximum value (max)
  LIBJEMDEC_TYPE_RANGE_ZEROCENTER, ///< A range from -max to +max
  LIBJEMDEC_TYPE_VECTOR,           ///< A vector
  LIBJEMDEC_TYPE_INTRA_DIR,        ///< A HEVC intra direction (0 to 35)
  LIBJEMDEC_TYPE_UNKNOWN
} libJEMDec_InternalsType;

/** Get the type of the internals type (does it represent a flag, a range, a vector ...)
  * \param decCtx The decoder context that was created with libJEMDec_new_decoder
  * \param idx The index of the internals type (can range from 0 to the value provided by libJEMDEC_get_internal_type_number)
  * \return The type of the internals type
  */
JEM_DEC_API libJEMDec_InternalsType libJEMDEC_get_internal_type(unsigned int idx);

/** If the type is LIBJEMDEC_TYPE_RANGE or LIBJEMDEC_TYPE_RANGE_ZEROCENTER, this function will provide the max value.
* \param decCtx The decoder context that was created with libJEMDec_new_decoder
* \param idx The index of the internals type (can range from 0 to the value provided by libJEMDEC_get_internal_type_number)
* \return The maximum value of this range or zero centered range
*/
JEM_DEC_API unsigned int libJEMDEC_get_internal_type_max(unsigned int idx);

/** If the type is LIBJEMDEC_TYPE_VECTOR, this function will provide the internal scaling of the vector values.
  * For example: For quarter precision motion vectors this value is 4. If there is no scaling, this values is 1.
* \param decCtx The decoder context that was created with libJEMDec_new_decoder
* \param idx The index of the internals type (can range from 0 to the value provided by libJEMDEC_get_internal_type_number)
* \return The scaling of the value.
*/
JEM_DEC_API unsigned int libJEMDEC_get_internal_type_vector_scaling(unsigned int idx);

/** Get a description of ther internals type with the given index.
* \param decCtx The decoder context that was created with libJEMDec_new_decoder
* \param idx The index of the internals type (can range from 0 to the value provided by libJEMDEC_get_internal_type_number)
* \return A desctiption of the interlas type
*/
JEM_DEC_API const char *libJEMDEC_get_internal_type_description(unsigned int idx);

/** Get the internal coding information from the picture.
 * The pointer to the returned array is always valid. However, the array data is changed if libJEMDEC_get_internal_info
 * or libJEMDEC_clear_internal_info is called. The internal cache has a fixed size and may overflow. In this case, callAgain
 * will be set and you will have to call this function again (after reading all the data from the buffer of course).
 * \param decCtx The decoder context that was created with libJEMDec_new_decoder
 * \param pic The libJEMDec_picture that was obtained using libJEMDec_get_picture.
 * \param typeIdx The index of the internals type (can range from 0 to the value provided by libJEMDEC_get_internal_type_number)
 * \param nrValues This will return the number of valid values in the returned array
 * \param callAgain This bool is set if the function returned because the cache was full. Call the function again (after processing all data from the array).
 * \return A pointer to the vector of block data
 */
JEM_DEC_API libJEMDec_BlockValue *libJEMDEC_get_internal_info(libJEMDec_context *decCtx, libJEMDec_picture *pic, unsigned int typeIdx, unsigned int &nrValues, bool &callAgain);

/** Clear the internal storage for the internal info (the pointer returned by libJEMDEC_get_internal_info).
 * If you no longer need the info in the internals vector, you can call this to free some space.
 * \param decCtx The decoder context that was created with libJEMDec_new_decoder
 * \return An error code or LIBJEMDEC_OK if no error occured
 */
JEM_DEC_API libJEMDec_error libJEMDEC_clear_internal_info(libJEMDec_context *decCtx);

#ifdef __cplusplus
}
#endif

//! \}

#endif // !LIBJEMDECODER_H
