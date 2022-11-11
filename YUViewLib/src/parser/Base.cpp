/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "Base.h"

#include <assert.h>

#define BASE_DEBUG_OUTPUT 0
#if BASE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_PARSER qDebug
#else
#define DEBUG_PARSER(fmt, ...) ((void)0)
#endif

namespace parser
{

/// --------------- Base ---------------------

Base::Base(QObject *parent) : QObject(parent)
{
  this->packetModel.reset(new PacketItemModel(parent));
  this->bitratePlotModel.reset(new BitratePlotModel());
  this->hrdPlotModel.reset(new HRDPlotModel());
  this->streamIndexFilter.reset(new FilterByStreamIndexProxyModel(parent));
  this->streamIndexFilter->setSourceModel(this->packetModel.data());
}

Base::~Base()
{
}

HRDPlotModel *Base::getHRDPlotModel()
{
  if (this->redirectPlotModel == nullptr && !this->hrdPlotModel.isNull())
    return this->hrdPlotModel.data();
  else if (this->redirectPlotModel != nullptr)
    return this->redirectPlotModel;
  return {};
}

void Base::setRedirectPlotModel(HRDPlotModel *plotModel)
{
  Q_ASSERT_X(plotModel != nullptr, Q_FUNC_INFO, "Redirect pointer is NULL");
  this->hrdPlotModel.reset(nullptr);
  this->redirectPlotModel = plotModel;
}

void Base::enableModel()
{
  if (!this->packetModel->rootItem)
  {
    this->packetModel->rootItem = std::make_shared<TreeItem>();
    this->packetModel->rootItem->setProperties("Name", "Value", "Coding", "Code", "Meaning");
  }
}

void Base::updateNumberModelItems()
{
  this->packetModel->updateNumberModelItems();
}

QString Base::convertSliceTypeMapToString(QMap<QString, unsigned int> &sliceTypes)
{
  QString text;
  for (auto key : sliceTypes.keys())
  {
    text += key;
    const auto value = sliceTypes.value(key);
    if (value > 1)
      text += QString("(%1x)").arg(value);
    text += " ";
  }
  return text;
}

QList<QTreeWidgetItem *> Base::StreamInfo::getStreamInfo()
{
  QList<QTreeWidgetItem *> infoList;
  infoList.append(
      new QTreeWidgetItem(QStringList() << "File size" << QString::number(this->fileSize)));
  if (this->parsing)
  {
    infoList.append(new QTreeWidgetItem(QStringList() << "Number units"
                                                      << "Parsing..."));
    infoList.append(new QTreeWidgetItem(QStringList() << "Number Frames"
                                                      << "Parsing..."));
  }
  else
  {
    infoList.append(
        new QTreeWidgetItem(QStringList() << "Number units" << QString::number(this->nrUnits)));
    infoList.append(
        new QTreeWidgetItem(QStringList() << "Number Frames" << QString::number(this->nrFrames)));
  }

  return infoList;
}

} // namespace parser