/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

/**
 * @file PathConversion.h
 * @brief Unicode-safe conversions between QString and std::filesystem::path.
 *
 * On Windows, std::filesystem::path constructed from std::string interprets
 * bytes as the ANSI code page. QString::toStdString() produces UTF-8, so
 * Chinese / CJK paths become corrupted or throw filesystem_error. Always use
 * these helpers when crossing the Qt ↔ filesystem boundary.
 */

#include <QString>
#include <QtGlobal>

#include <filesystem>
#include <string>

namespace functions
{

/**
 * @brief Convert a Qt path string to a native std::filesystem::path.
 * @param path UTF-16 QString path (as produced by Qt file dialogs).
 * @return filesystem::path using wide strings on Windows, UTF-8 elsewhere.
 */
inline std::filesystem::path qStringToFsPath(const QString &path)
{
#ifdef Q_OS_WIN
  return std::filesystem::path(path.toStdWString());
#else
  return std::filesystem::path(path.toStdString());
#endif
}

/**
 * @brief Convert a std::filesystem::path back to a Qt QString.
 * @param path Native filesystem path.
 * @return QString preserving Unicode characters on all platforms.
 */
inline QString fsPathToQString(const std::filesystem::path &path)
{
#ifdef Q_OS_WIN
  return QString::fromStdWString(path.wstring());
#else
  // C++20: path.u8string() returns std::u8string (char8_t), not std::string.
  // QString::fromStdString cannot accept it; decode UTF-8 bytes explicitly.
  const std::u8string u8 = path.u8string();
  return QString::fromUtf8(reinterpret_cast<const char *>(u8.data()),
                           static_cast<int>(u8.size()));
#endif
}

/**
 * @brief Convert a filesystem path to a UTF-8 std::string for display / logging.
 * @param path Native filesystem path.
 * @return UTF-8 encoded path string (safe for QString::fromStdString round-trip).
 *
 * Do not pass the result to std::filesystem::path on Windows; use the path
 * object (or qStringToFsPath) instead.
 */
inline std::string fsPathToUtf8String(const std::filesystem::path &path)
{
  return fsPathToQString(path).toStdString();
}

} // namespace functions
