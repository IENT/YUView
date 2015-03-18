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

#include <QMap>
#include <QHash>

enum statistics_t {
    arrowType = 0,
    blockType
};

struct StatisticsItem {
    statistics_t type;
    QColor color;
    QColor gridColor;
    QRect positionRect;
    float vector[2];
    int rawValues[2];
};

typedef QList<StatisticsItem> StatisticsItemList;

struct StatisticsRenderItem {
    int type_id;
    bool renderGrid;
    bool render;
    int alpha;
};

class VisualizationType;

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

    QString getTypeName(int type);
    QList<int> getTypeIDs();

    void setActiveStatsTypes(QVector<StatisticsRenderItem> types) { p_activeStatsTypes = types; }
    QVector<StatisticsRenderItem>& getActiveStatsTypes() { return p_activeStatsTypes; }

private:
    void readHeaderFromFile();
    void readFramePositionsFromFile();
    void readStatisticsFromFile(int frameIdx);

    StatisticsItemList getStatistics(int frameNumber, int type=0);

    void drawStatisticsImage(unsigned int idx);
    void drawStatisticsImage(StatisticsItemList statsList, StatisticsRenderItem item);

    StatisticsItemList getFrontmostActiveStatisticsItem(unsigned int idx, int& type);

    QVector<StatisticsRenderItem> p_activeStatsTypes; // contains all type-IDs of stats and whether they should be rendered (in order)

    QList<QString> parseCSVLine(QString line, char delimiter);

    QHash< int,QHash< int,StatisticsItemList > > p_statsCache; // 2D map of type StatisticsItemList with indexing: [POC][statsTypeID]
    QMap<int,VisualizationType*> p_typeList;

    QMap<int,qint64> p_pocStartList;

    QString p_srcFilePath;
    QString p_createdTime;
    QString p_modifiedTime;
};

#endif // STATISTICSMODEL_H
