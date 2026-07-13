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
 *   file(s), but you are not obligated to do so. If you do not wish to
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

// ---------------------------------------------------------------------------
// Unified logging macros
//
// Replace the dozens of per-file `#define XXX_DEBUG_OUTPUT 0` switches with
// QLoggingCategory-based macros that can be toggled at runtime.
//
// Categories are organised by module. Each category can be enabled/disabled
// individually via:
//   * Environment variable:  QT_LOGGING_RULES="yuv.decoder.debug=true"
//   * Code:  QLoggingCategory::setFilterRules("yuv.*.debug=true")
//   * Multiple rules: "yuv.decoder.debug=true;yuv.cache.debug=false"
//
// File / line / function context is captured automatically by Qt's
// QMessageLogContext and forwarded to Logger::writeEntry, so the macros
// themselves stay minimal.
//
// Usage:
//   LOG_DEBUG(logDecoder) << "frame" << idx << "decoded";
//   LOG_WARNING(logVideo) << "out of memory, retrying";
//   LOG_ERROR(logParser) << "parse failed at offset" << offset;
// ---------------------------------------------------------------------------

#include <QLoggingCategory>

// Module categories. Names are prefixed with "yuv." so that logging rules
// can target the whole application ("yuv.*.debug=true") or a single module
// ("yuv.decoder.debug=true").
Q_DECLARE_LOGGING_CATEGORY(logApp)        // application / startup / misc
Q_DECLARE_LOGGING_CATEGORY(logParser)     // bitstream parsers (HEVC/AVC/VVC/AV1/MPEG2)
Q_DECLARE_LOGGING_CATEGORY(logDecoder)    // decoder base + all decoder backends
Q_DECLARE_LOGGING_CATEGORY(logVideo)      // video handlers (RGB/YUV/resample/diff)
Q_DECLARE_LOGGING_CATEGORY(logCache)      // frame caching and worker threads
Q_DECLARE_LOGGING_CATEGORY(logUI)         // widgets, views, playback, main window
Q_DECLARE_LOGGING_CATEGORY(logFFmpeg)     // FFmpeg library bridging
Q_DECLARE_LOGGING_CATEGORY(logStats)      // statistics file parsing & overlay
Q_DECLARE_LOGGING_CATEGORY(logFileSource) // file source / data source
Q_DECLARE_LOGGING_CATEGORY(logUpdater)    // update handler

// Convenience macros. Using .noquote() so that string values appear in the
// log without surrounding double quotes, keeping the output readable.
#define LOG_DEBUG(cat)   qCDebug(cat).noquote()
#define LOG_INFO(cat)    qCInfo(cat).noquote()
#define LOG_WARNING(cat) qCWarning(cat).noquote()
#define LOG_ERROR(cat)   qCCritical(cat).noquote()
#define LOG_FATAL(cat)   qCFatal(cat).noquote()
