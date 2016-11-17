/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2016  Institut für Nachrichtentechnik
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

#ifndef DE265WRAPPER_H
#define DE265WRAPPER_H

#include "de265.h"
#include <QLibrary>

struct de265Functions
{
  de265Functions();

  de265_decoder_context *(*de265_new_decoder)          ();
  void                   (*de265_set_parameter_bool)   (de265_decoder_context*, de265_param, int);
  void                   (*de265_set_parameter_int)    (de265_decoder_context*, de265_param, int);
  void                   (*de265_disable_logging)      ();
  void                   (*de265_set_verbosity)        (int);
  de265_error            (*de265_start_worker_threads) (de265_decoder_context*, int);
  void                   (*de265_set_limit_TID)        (de265_decoder_context*, int);
  const char*            (*de265_get_error_text)       (de265_error);
  de265_chroma           (*de265_get_chroma_format)    (const de265_image*);
  int                    (*de265_get_image_width)      (const de265_image*, int);
  int                    (*de265_get_image_height)     (const de265_image*, int);
  const uint8_t*         (*de265_get_image_plane)      (const de265_image*, int, int*);
  int                    (*de265_get_bits_per_pixel)   (const de265_image*, int);
  de265_error            (*de265_decode)               (de265_decoder_context*, int*);
  de265_error            (*de265_push_data)            (de265_decoder_context*, const void*, int, de265_PTS, void*);
  de265_error            (*de265_flush_data)           (de265_decoder_context*);
  const de265_image*     (*de265_get_next_picture)     (de265_decoder_context*);
  de265_error            (*de265_free_decoder)         (de265_decoder_context*);

  // libde265 decoder library function pointers for internals
  void (*de265_internals_get_CTB_Info_Layout)		   (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_CTB_sliceIdx)			   (const de265_image*, uint16_t*);
  void (*de265_internals_get_CB_Info_Layout)		   (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_CB_info)				       (const de265_image*, uint16_t*);
  void (*de265_internals_get_PB_Info_layout)		   (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_PB_info)				       (const de265_image*, int16_t*, int16_t*, int16_t*, int16_t*, int16_t*, int16_t*);
  void (*de265_internals_get_IntraDir_Info_layout) (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_intraDir_info)			   (const de265_image*, uint8_t*, uint8_t*);
  void (*de265_internals_get_TUInfo_Info_layout)	 (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_TUInfo_info)			     (const de265_image*, uint8_t*);

  static const int abc = 5;
};

// This class wraps the de265 library in a demand-load fashion.
// To easily access the functions, one can use protected inheritance:
// class de265User : ..., protected de265Wrapper
// This API is similar to the QOpenGLFunctions API family.
class de265Wrapper : public de265Functions {
public:
  de265Wrapper();

  void loadDecoderLibrary();
  bool wrapperError() const { return error; }
  QString wrapperErrorString() const { return errorString; }
  bool wrapperInternalsSupported() const { return internalsSupported; } ///< does the loaded library support the extraction of internals/statistics?

private:
  void setError(const QString &reason);
  QFunctionPointer resolve(const char *symbol);
  template <typename T> T resolve(T &ptr, const char *symbol);
  template <typename T> T resolveInternals(T &ptr, const char *symbol);

  QLibrary library;
  bool error;
  bool internalsSupported;
  QString errorString;
};

#endif // DE265WRAPPER_H
