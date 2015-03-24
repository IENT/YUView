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

#include <assert.h>
#include <string>
#include "displayobject.h"
#include "statisticsextensions.h"

#include <QMap>
#include <QHash>
#include <QFuture>

typedef QList<StatisticsItem> StatisticsItemList;
typedef QVector<StatisticsType> StatisticsTypeList;

class StatisticsObject : public DisplayObject
{
public:
    StatisticsObject(const QString& srcFileName, QObject* parent = 0);
    ~StatisticsObject();

    QString path() {return p_srcFilePath;}
    QString createdtime() {return p_createdTime;}
    QString modifiedtime() {return p_modifiedTime;}

    void loadImage(int idx);

    ValuePairList getValuesAt(int x, int y);

    void setInternalScaleFactor(int internalScaleFactor);

    StatisticsType* getStatisticsType(int typeID);

    void setStatisticsTypeList(StatisticsTypeList typeList);
    StatisticsTypeList getStatisticsTypeList() { return p_statsTypeList; }

    int numFrames() { return p_numberFrames; }
    int nrBytes() { return p_numBytes; }
    QString status() { return p_backgroundParserFuture.isRunning() ? QString("Parsing...") : QString("OK"); }

private:
    void readHeaderFromFile();
    void readFramePositionsFromFile();
    void readStatisticsFromFile(int frameIdx);

    StatisticsItemList getStatistics(int frameIdx, int type);

    void drawStatisticsImage(int frameIdx);
    void drawStatisticsImage(StatisticsItemList statsList, StatisticsType statsType);

    QStringList parseCSVLine(QString line, char delimiter);

    QHash< int,QHash< int,StatisticsItemList > > p_statsCache; // 2D map of type StatisticsItemList with indexing: [POC][statsTypeID]
    StatisticsTypeList p_statsTypeList;

    QFuture<void> p_backgroundParserFuture;
    bool p_cancelBackgroundParser;

    QMap<int,qint64> p_pocStartList;

    QString p_srcFilePath;
    QString p_createdTime;
    QString p_modifiedTime;
    int     p_numBytes;

    int p_numberFrames;
};

#endif // STATISTICSMODEL_H
