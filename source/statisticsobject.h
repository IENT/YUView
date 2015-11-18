/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
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

#ifndef STATISTICSMODEL_H
#define STATISTICSMODEL_H

#include "displayobject.h"
#include "statisticSourceFile.h"

class StatisticsObject : public DisplayObject
{
  Q_OBJECT

public:
  StatisticsObject(const QString& srcFileName, QObject* parent = 0);
  StatisticsObject(QSharedPointer<statisticSource> statSrc, QObject* parent = 0);
  ~StatisticsObject();

  QString path() { return p_statisticSource->getPath(); }
  QString createdtime() {return p_statisticSource->getCreatedtime();}
  QString modifiedtime() {return p_statisticSource->getModifiedtime();}

  // Load the image with the given frame index into p_displayImage
  void loadImage(int idx);
  void reloadImage() { loadImage(p_lastIdx); }

  // Get a list of all (rendered) statistics values at a certain position
  ValuePairList getValuesAt(int x, int y) { return p_statisticSource->getValuesAt(x, y); }

  int internalScaleFactor() { return p_statisticSource->internalScaleFactor(); }
  // Set the internal scaling factor and return if it changed
  bool setInternalScaleFactor(int internalScaleFactor) { return p_statisticSource->setInternalScaleFactor(internalScaleFactor); }

  // Set the attributes of the statistics types (rendered, showGrid...)
  bool setStatisticsTypeList(StatisticsTypeList typeList) { return p_statisticSource->setStatisticsTypeList(typeList); }
  // Get a list of all statistics types that the source can provide
  StatisticsTypeList getStatisticsTypeList() { return p_statisticSource->getStatisticsTypeList(); }
  // Return true if any of the statistics are actually rendered
  bool anyStatisticsRendered() { return p_statisticSource->anyStatisticsRendered(); }

  int numFrames() { return p_statisticSource->getNumberFrames(); }

  // Get statistics object info 
  QString getInfoTitle() { return QString("Statistics Info"); };
  QList<fileInfoItem> getInfoList();

public slots:
  void statisticSourceInformationChanced();
      
private:
  QSharedPointer<statisticSource> p_statisticSource;
  
};

#endif // STATISTICSMODEL_H
