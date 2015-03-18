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
#include <vector>
#include <list>
#include <string>
#include "displayobject.h"

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

typedef std::list<StatisticsItem> StatisticsItemList;

struct StatisticsRenderItem {
    int type_id;
    bool renderGrid;
    bool render;
    int alpha;
};

// minimal matrix declaration
template <typename w_elem_type> class Matrix
{
public:
    typedef int t_Size;

    t_Size m_columns;
    t_Size m_rows;

    std::vector<w_elem_type> m_data;

    Matrix( t_Size i_columns = 0, t_Size i_rows = 0 )
        : m_columns( i_columns ),
          m_rows( i_rows ),
          m_data( i_columns * i_rows )
    {
    }

    void resize( t_Size i_columns, t_Size i_rows) {
        m_data.resize( i_rows * i_columns );
        m_rows = i_rows;
        m_columns = i_columns;
    }

    w_elem_type* operator[]( t_Size i_index )
    {
        return & ( m_data[ i_index * m_rows ] );
    }
/*
    template <typename w_Type, int w_columns, int w_rows>
    matrix( const w_Type (&i_array)[w_columns][w_rows] )
        : m_columns( w_columns ),
          m_rows( w_rows ),
          m_data( & (i_array[0][0]), & (i_array[w_columns-1][w_rows]) )
    {
    }*/
};

struct VisualizationType;

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
    std::vector<int> getTypeIDs();

    static StatisticsItemList emptyStats;

    void setActiveStatsTypes(QVector<StatisticsRenderItem> types) { p_activeStatsTypes = types; }
    QVector<StatisticsRenderItem>& getActiveStatsTypes() { return p_activeStatsTypes; }

private:
    bool parseFile(std::string filename);

    StatisticsItemList& getStatistics(int frameNumber, int type=0);
    StatisticsItemList getSimplifiedStatistics(int frameNumber, int type, int theshold, QColor color);

    void drawStatisticsImage(unsigned int idx);
    void drawStatisticsImage(StatisticsItemList& statsList, StatisticsRenderItem &item);

    StatisticsItemList& getFrontmostActiveStatisticsItem(unsigned int idx, int& type);

    QVector<StatisticsRenderItem> p_activeStatsTypes; // contains all type-IDs of stats and whether they should be rendered (in order)

    void reset();
    void parseCSVLine(std::vector<std::string> &record, const std::string& line, char delimiter);
    Matrix<StatisticsItemList> *p_stats; // 2D array of type StatisticsItemList
    std::vector<VisualizationType*> p_types;

    QString p_srcFilePath;
    QString p_createdTime;
    QString p_modifiedTime;
};

#endif // STATISTICSMODEL_H
