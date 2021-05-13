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

#include "LoggingModel.h"

#include <common/Logging.h>

using namespace logging;

LoggingModel::LoggingModel(QObject *parent) : QAbstractTableModel(parent) {}

int LoggingModel::rowCount(const QModelIndex &) const
{
  return int(this->nrRows);
}

int LoggingModel::columnCount(const QModelIndex &) const { return 4; }

QVariant LoggingModel::data(const QModelIndex &index, int role) const
{
  const auto col = index.column();
  const auto row = index.row();
  if (role == Qt::DisplayRole)
  {
    auto e = Logger::instance().getEntry(row);
    if (col == 0)
      return QString::fromStdString(e.time);
    if (col == 1)
      return QString::fromStdString(LogLevelMapper.getName(e.level));
    if (col == 2)
      return QString::fromStdString(e.component);
    if (col == 3)
      return QString::fromStdString(e.message);
  }
  if (role == Qt::BackgroundRole)
  {
    auto e = Logger::instance().getEntry(row);
    if (e.level == LogLevel::Error)
      return QBrush(Qt::red);
  }

  return QVariant();
}

QVariant LoggingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
  {
    if (section == 0)
      return QString("Time");
    if (section == 1)
      return QString("Level");
    if (section == 2)
      return QString("Component");
    if (section == 3)
      return QString("Message");
  }
  return QVariant();
}

void LoggingModel::updateNumberModelItems()
{
  auto n = Logger::instance().getNrEntries();
  Q_ASSERT_X(n >= this->nrRows, Q_FUNC_INFO, "Setting a smaller number of rows");
  auto nrAddItems = int(n - this->nrRows);
  if (nrAddItems == 0)
    return;

  auto lastIndex = int(this->nrRows);
  beginInsertRows(QModelIndex(), lastIndex, lastIndex + nrAddItems - 1);
  this->nrRows = unsigned(n);
  endInsertRows();
}
