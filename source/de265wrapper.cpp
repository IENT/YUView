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

#include "de265wrapper.h"
#include <cstring>
#include <QDir>
#include <QCoreApplication>

de265Functions::de265Functions() { memset(this, 0, sizeof(*this)); }

de265Wrapper::de265Wrapper() :
  error(false),
  internalsSupported(false)
{
}

void de265Wrapper::setError(const QString &reason)
{
  error = true;
  errorString = reason;
}

QFunctionPointer de265Wrapper::resolve(const char *symbol)
{
  QFunctionPointer ptr = library.resolve(symbol);
  if (!ptr) setError(QStringLiteral("Error loading the libde265 library: Can't find function %1.").arg(symbol));
  return ptr;
}

template <typename T> T de265Wrapper::resolve(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(resolve(symbol));
}

template <typename T> T de265Wrapper::resolveInternals(T &fun, const char *symbol)
{
  return fun = reinterpret_cast<T>(library.resolve(symbol));
}

void de265Wrapper::loadDecoderLibrary()
{
  // Try to load the libde265 library from the current working directory
  // Unfortunately relative paths like this do not work: (at least on windows)
  // library.setFileName(".\\libde265");

#ifdef Q_OS_MAC
  // If the file name is not set explicitly, QLibrary will try to open
  // the libde265.so file first. Since this has been compiled for linux
  // it will fail and not even try to open the libde265.dylib
  QStringList libNames = QStringList() << "libde265-internals.dylib" << "libde265.dylib";
#else
  // On windows and linux ommitting the extension works
  QStringList libNames = QStringList() << "libde265-internals" << "libde265";
#endif
  QStringList libPaths = QStringList()
      << QDir::currentPath() + "/%1"
      << QDir::currentPath() + "/libde265/%1"
      << QCoreApplication::applicationDirPath() + "/%1"
      << QCoreApplication::applicationDirPath() + "/libde265/%1"
      << "%1"; // Try the system directories.

  bool libLoaded = false;
  Q_FOREACH(QString libName, libNames)
  {
    Q_FOREACH(QString libPath, libPaths)
    {
      library.setFileName(libPath.arg(libName));
      libLoaded = library.load();
      if (libLoaded)
        break;
    }
    if (libLoaded)
      break;
  }

  if (!libLoaded)
    return setError(library.errorString());

  // Get/check function pointers
  if (!resolve(de265_new_decoder, "de265_new_decoder")) return;
  if (!resolve(de265_set_parameter_bool, "de265_set_parameter_bool")) return;
  if (!resolve(de265_set_parameter_int, "de265_set_parameter_int")) return;
  if (!resolve(de265_disable_logging, "de265_disable_logging")) return;
  if (!resolve(de265_set_verbosity, "de265_set_verbosity")) return;
  if (!resolve(de265_start_worker_threads, "de265_start_worker_threads")) return;
  if (!resolve(de265_set_limit_TID, "de265_set_limit_TID")) return;
  if (!resolve(de265_get_error_text, "de265_get_error_text")) return;
  if (!resolve(de265_get_chroma_format, "de265_get_chroma_format")) return;
  if (!resolve(de265_get_image_width, "de265_get_image_width")) return;
  if (!resolve(de265_get_image_height, "de265_get_image_height")) return;
  if (!resolve(de265_get_image_plane, "de265_get_image_plane")) return;
  if (!resolve(de265_get_bits_per_pixel,"de265_get_bits_per_pixel")) return;
  if (!resolve(de265_decode, "de265_decode")) return;
  if (!resolve(de265_push_data, "de265_push_data")) return;
  if (!resolve(de265_flush_data, "de265_flush_data")) return;
  if (!resolve(de265_get_next_picture, "de265_get_next_picture")) return;
  if (!resolve(de265_free_decoder, "de265_free_decoder")) return;

  // Get pointers to the internals/statistics functions (if present)
  // If not, disable the statistics extraction. Normal decoding of the video will still work.

  if (!resolveInternals(de265_internals_get_CTB_Info_Layout, "de265_internals_get_CTB_Info_Layout")) return;
  if (!resolveInternals(de265_internals_get_CTB_sliceIdx, "de265_internals_get_CTB_sliceIdx")) return;
  if (!resolveInternals(de265_internals_get_CB_Info_Layout, "de265_internals_get_CB_Info_Layout")) return;
  if (!resolveInternals(de265_internals_get_CB_info, "de265_internals_get_CB_info")) return;
  if (!resolveInternals(de265_internals_get_PB_Info_layout, "de265_internals_get_PB_Info_layout")) return;
  if (!resolveInternals(de265_internals_get_PB_info, "de265_internals_get_PB_info")) return;
  if (!resolveInternals(de265_internals_get_IntraDir_Info_layout, "de265_internals_get_IntraDir_Info_layout")) return;
  if (!resolveInternals(de265_internals_get_intraDir_info, "de265_internals_get_intraDir_info")) return;
  if (!resolveInternals(de265_internals_get_TUInfo_Info_layout, "de265_internals_get_TUInfo_Info_layout")) return;
  if (!resolveInternals(de265_internals_get_TUInfo_info, "de265_internals_get_TUInfo_info")) return;

  internalsSupported = true;
}
