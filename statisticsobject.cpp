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

#include "statisticsobject.h"

#include <QSettings>
#include <QColor>
#include <QPainter>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include <sstream>
#include <map>
#include <iostream>
#if _WIN32 && !__MINGW32__
#define _USE_MATH_DEFINES 1
#include "math.h"
#else
#include <cmath>
#endif

void rotateVector(float angle, float vx, float vy, float &nx, float &ny)
{
    float s = sinf(angle);
    float c = cosf(angle);

    nx = c * vx - s * vy;
    ny = s * vx + c * vy;

    // normalize vector
    float n_abs = sqrtf( nx*nx + ny*ny );
    nx /= n_abs;
    ny /= n_abs;
}

int StringToNumber ( std::string &Text )//Text not by const reference so that the function can be used with a
{                               //character array as argument
    std::stringstream ss(Text);
    int result;
    return ss >> result ? result : 0;
}

float median(std::list<float> &l)
{
    l.sort();
    std::list<float>::iterator it = l.begin();
    std::advance(it, l.size() / 2);
    return *it;
}

// Type, Map and Range are data structures

typedef std::map<int,QColor> ColorMap;

struct ColorRange {
    ColorRange() {}
    ColorRange(std::vector<std::string> &row) {
        rangeMin = StringToNumber(row[2]);
        unsigned char minColorR = StringToNumber(row[4]);
        unsigned char minColorG = StringToNumber(row[6]);
        unsigned char minColorB = StringToNumber(row[8]);
        unsigned char minColorA = StringToNumber(row[10]);
        minColor = QColor( minColorR, minColorG, minColorB, minColorA );

        rangeMax = StringToNumber(row[3]);
        unsigned char maxColorR = StringToNumber(row[5]);
        unsigned char maxColorG = StringToNumber(row[7]);
        unsigned char maxColorB = StringToNumber(row[9]);
        unsigned char maxColorA = StringToNumber(row[11]);
        minColor = QColor( maxColorR, maxColorG, maxColorB, maxColorA );
    }
    virtual ~ColorRange() {}

    virtual QColor getColor(float value) {
        // clamp the value to [min max]
        if (value > rangeMax) value = (float)rangeMax;
        if (value < rangeMin) value = (float)rangeMin;

        unsigned char retR = minColor.red() + (unsigned char)( floor((float)value / (float)(rangeMax-rangeMin) * (float)(maxColor.red()-minColor.red()) + 0.5f) );
        unsigned char retG = minColor.green() + (unsigned char)( floor((float)value / (float)(rangeMax-rangeMin) * (float)(maxColor.green()-minColor.green()) + 0.5f) );
        unsigned char retB = minColor.blue() + (unsigned char)( floor((float)value / (float)(rangeMax-rangeMin) * (float)(maxColor.blue()-minColor.blue()) + 0.5f) );
        unsigned char retA = minColor.alpha() + (unsigned char)( floor((float)value / (float)(rangeMax-rangeMin) * (float)(maxColor.alpha()-minColor.alpha()) + 0.5f) );

        return QColor(retR, retG, retB, retA);
    }

    int rangeMin, rangeMax;
    QColor minColor;
    QColor maxColor;
};

enum defaultColormaps_t {
    jetColormap = 0,
    heatColormap,
    hsvColormap,
    hotColormap,
    coolColormap,
    springColormap,
    summerColormap,
    autumnColormap,
    winterColormap,
    grayColormap,
    boneColormap,
    copperColormap,
    pinkColormap,
    linesColormap
};

struct DefaultColorRange : ColorRange
{
    DefaultColorRange(std::vector<std::string> &row)
    {
        rangeMin = StringToNumber(row[2]);
        rangeMax = StringToNumber(row[3]);
        std::string str = row[4];
        if (str == "jet")
            type = jetColormap;
        else if (str == "heat")
            type = heatColormap;
        else if (str == "hsv")
            type = hsvColormap;
        else if (str == "hot")
            type = hotColormap;
        else if (str == "cool")
            type = coolColormap;
        else if (str == "spring")
            type = springColormap;
        else if (str == "summer")
            type = summerColormap;
        else if (str == "autumn")
            type = autumnColormap;
        else if (str == "winter")
            type = winterColormap;
        else if (str == "gray")
            type = grayColormap;
        else if (str == "bone")
            type = boneColormap;
        else if (str == "copper")
            type = copperColormap;
        else if (str == "pink")
            type = pinkColormap;
        else if (str == "lines")
            type = linesColormap;
    }

    virtual QColor getColor(float value)
    {
        float span = (float)(rangeMax-rangeMin),
                val = (float)value,
                x = val / span,
                r=1,g=1,b=1,a=1;
        float F,N,K;
        int I;

        // clamp the value to [min max]
        if (value > rangeMax) value = (float)rangeMax;
        if (value < rangeMin) value = (float)rangeMin;

        switch (type) {
        case jetColormap:
            if ((x >= 3.0/8.0) && (x < 5.0/8.0)) r = (4.0f * x - 3.0f/2.0f); else
                if ((x >= 5.0/8.0) && (x < 7.0/8.0)) r = 1.0; else
                if (x >= 7.0/8.0) r = (-4.0f * x + 9.0f/2.0f); else
                    r = 0.0f;
            if ((x >= 1.0/8.0) && (x < 3.0/8.0)) g = (4.0f * x - 1.0f/2.0f); else
                if ((x >= 3.0/8.0) && (x < 5.0/8.0)) g = 1.0; else
                if ((x >= 5.0/8.0) && (x < 7.0/8.0)) g = (-4.0f * x + 7.0f/2.0f); else
                    g = 0.0f;
            if (x < 1.0/8.0) b = (4.0f * x + 1.0f/2.0f); else
                if ((x >= 1.0/8.0) && (x < 3.0/8.0)) b = 1.0f; else
                if ((x >= 3.0/8.0) & (x < 5.0/8.0)) b = (-4.0f * x + 5.0f/2.0f); else
                    b = 0.0f;
            break;
        case heatColormap:
            r = 1;
            g = 0;
            b = 0;
            a = x;
            break;
        case hsvColormap:  // h = x, s = 1, v = 1
            if (x >= 1.0) x = 0.0;
            x = x * 6.0f;
            I = (int) x;   /* should be in the range 0..5 */
            F = x - I;     /* fractional part */

            N = (1.0f - 1.0f * F);
            K = (1.0f - 1.0f * (1 - F));

            if (I == 0) { r = 1; g = K; b = 0; }
            if (I == 1) { r = N; g = 1.0; b = 0; }
            if (I == 2) { r = 0; g = 1.0; b = K; }
            if (I == 3) { r = 0; g = N; b = 1.0; }
            if (I == 4) { r = K; g = 0; b = 1.0; }
            if (I == 5) { r = 1.0; g = 0; b = N; }
            break;
        case hotColormap:
            if (x < 2.0/5.0) r = (5.0f/2.0f * x); else r = 1.0f;
            if ((x >= 2.0/5.0) && (x < 4.0/5.0)) g = (5.0f/2.0f * x - 1); else if (x >= 4.0/5.0) g = 1.0f; else g = 0.0f;
            if (x >= 4.0/5.0) b = (5.0f*x - 4.0f); else b = 0.0f;
            break;
        case coolColormap:
            r = x;
            g = 1.0f - x;
            b = 1.0;
            break;
        case springColormap:
            r = 1.0f;
            g = x;
            b = 1.0f - x;
            break;
        case summerColormap:
            r = x;
            g = 0.5f + x / 2.0f;
            b = 0.4f;
            break;
        case autumnColormap:
            r = 1.0;
            g = x;
            b = 0.0;
            break;
        case winterColormap:
            r = 0;
            g = x;
            b = 1.0f - x / 2.0f;
            break;
        case grayColormap:
            r = x;
            g = x;
            b = x;
            break;
        case boneColormap:
            if (x < 3.0/4.0) r = (7.0f/8.0f * x); else if (x >= 3.0/4.0) r = (11.0f/8.0f * x - 3.0f/8.0f);
            if (x < 3.0/8.0) g = (7.0f/8.0f * x); else
                if ((x >= 3.0/8.0) && (x < 3.0/4.0)) g = (29.0f/24.0f * x - 1.0f/8.0f); else
                if (x >= 3.0/4.0) g = (7.0f/8.0f * x + 1.0f/8.0f);
            if (x < 3.0/8.0) b = (29.0f/24.0f * x); else if (x >= 3.0/8.0) b = (7.0f/8.0f * x + 1.0f/8.0f);
            break;
        case copperColormap:
            if (x < 4.0/5.0) r = (5.0f/4.0f * x); else r = 1.0f;
            g = 4.0f/5.0f * x;
            b = 1.0f/2.0f * x;
            break;
        case pinkColormap:
            if (x < 3.0/8.0) r = (14.0f/9.0f * x); else
                if (x >= 3.0/8.0) r = (2.0f/3.0f * x + 1.0f/3.0f);
            if (x < 3.0/8.0) g = (2.0f/3.0f * x); else
                if ((x >= 3.0/8.0) && (x < 3.0/4.0)) g = (14.0f/9.0f * x - 1.0f/3.0f); else
                if (x >= 3.0/4.0) g = (2.0f/3.0f * x + 1.0f/3.0f);
            if (x < 3.0/4.0)b = (2.0f/3.0f * x); else
                if (x >= 3.0/4.0) b = (2.0f * x - 1.0f);
            break;
        case linesColormap:
            if (x >= 1.0) x = 0.0f;
            x = x * 7.0f;
            I = (int) x;

            if (I == 0) { r = 0.0; g = 0.0; b = 1.0; }
            if (I == 1) { r = 0.0; g = 0.5; b = 0.0; }
            if (I == 2) { r = 1.0; g = 0.0; b = 0.0; }
            if (I == 3) { r = 0.0; g = 0.75; b = 0.75; }
            if (I == 4) { r = 0.75; g = 0.0; b = 0.75; }
            if (I == 5) { r = 0.75; g = 0.75; b = 0.0; }
            if (I == 6) { r = 0.25; g = 0.25; b = 0.25; }
            break;
        }

        //TODO: proper rounding
        unsigned char retR = (unsigned char)( floor(r * 255.0f + 0.5f) );
        unsigned char retG = (unsigned char)( floor(g * 255.0f + 0.5f) );
        unsigned char retB = (unsigned char)( floor(b * 255.0f + 0.5f) );
        unsigned char retA = (unsigned char)( floor(a * 255.0f + 0.5f) );

        return QColor(retR, retG, retB, retA);
    }

    defaultColormaps_t type;
};

enum visualizationType_t { colorMapType, colorRangeType, vectorType };
struct VisualizationType
{
    VisualizationType(std::vector<std::string> &row) {
        id = StringToNumber(row[2]);
        name = QString::fromStdString(row[3]);
        if (row[4] == "map") type = colorMapType;
        else if (row[4] == "range") type = colorRangeType;
        else if (row[4] == "vector") type = vectorType;

        colorRange = NULL;

        vectorSampling = 1;
        scaleToBlockSize = false;
    }
    ~VisualizationType()
    {
        if( colorRange == NULL ) { delete colorRange; colorRange = NULL; }
    }

    int id;
    QString name;
    visualizationType_t type;
    bool scaleToBlockSize;

    // only for vector type:
    int vectorSampling;

    // only one of the next should be set, depending on type
    ColorMap colorMap;
    ColorRange* colorRange; // can either be a ColorRange or a DefaultColorRange
    QColor vectorColor;
    QColor gridColor;
};

StatisticsItemList StatisticsObject::emptyStats;

StatisticsObject::StatisticsObject(const QString& srcFileName, QObject* parent) : DisplayObject(parent)
{
    p_stats = NULL;

    QStringList components = srcFileName.split(QDir::separator());
    QString fileName = components.last();
    int lastPoint = fileName.lastIndexOf(".");
    p_name = fileName.left(lastPoint);

    // TODO: try to get width, height, framerate from filename, just like with YUVFile?
    parseFile(srcFileName.toStdString());

    // get some more information from file
    QFileInfo fileInfo(srcFileName);
    p_srcFilePath = srcFileName;
    p_createdTime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
    p_modifiedTime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");

    // nothing to show by default
    p_activeStatsTypes.clear();

    // TODO: might be reasonable to have a map instead of a vector to allow not ascending type identifiers
    std::vector<int> types = getTypeIDs();
    StatisticsRenderItem item;

    item.renderGrid = false;
    item.render = false;
    item.alpha = 50;
    for (int i=0; i<(int)types.size(); i++) {
        if (types[i] == -1)
            continue;
        item.type_id = types[i];
        p_activeStatsTypes.push_back(item);
    }
}
StatisticsObject::~StatisticsObject() {
    // clean up
    for (int i=0; i<(int)p_types.size(); i++)
        delete p_types[i];
}

void StatisticsObject::loadImage(unsigned int idx)
{
    // create empty image
    QImage tmpImage(p_width,p_height,QImage::Format_ARGB32);
    tmpImage.fill(qRgba(0, 0, 0, 0));   // clear with transparent color
    p_displayImage.convertFromImage(tmpImage);

    if( idx < (unsigned int)p_stats->m_columns )
    {
        drawStatisticsImage(idx);
        p_lastIdx = idx;
    }
}

void StatisticsObject::drawStatisticsImage(unsigned int idx)
{
    QSettings settings;
    QColor simplifiedColor = settings.value("Statistics/SimplificationColor").value<QColor>();

    bool simplifyStats = settings.value("Statistics/Simplify",false).toBool();
    int simplificationThreshold = settings.value("Statistics/SimplificationSize",0).toInt();

    // TODO: respect zoom factor of display widget for simplification here...
    float zoomFactor = 1.0;

    // draw statistics (inverse order)
    for(int i=p_activeStatsTypes.size()-1; i>=0; i--)
    {
        if (!p_activeStatsTypes[i].render)
            continue;

        StatisticsItemList stats;
        if (simplifyStats) {
            //calculate threshold
            int threshold=0;
            unsigned int v = (zoomFactor < 1) ? 1/zoomFactor : zoomFactor;
            // calc next power of two
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            v++;
            threshold = (zoomFactor < 1) ? v * simplificationThreshold : simplificationThreshold / v;

            stats = getSimplifiedStatistics(idx, p_activeStatsTypes[i].type_id, threshold, simplifiedColor);
        } else {
            stats = getStatistics(idx, p_activeStatsTypes[i].type_id);
        }

        drawStatisticsImage(stats, p_activeStatsTypes[i]);
    }
}

void StatisticsObject::drawStatisticsImage(StatisticsItemList& statsList, StatisticsRenderItem &item)
{
    QPainter painter(&p_displayImage);

    StatisticsItemList::iterator it;
    for (it = statsList.begin(); it != statsList.end(); it++)
    {
        StatisticsItem anItem = *it;

        switch (anItem.type)
        {
        case arrowType:
        {
            //draw an arrow
            float x,y, nx, ny, a;

            // start vector at center of the block
            x = (float)anItem.positionRect.left()+(float)anItem.positionRect.width()/2.0;
            y = (float)anItem.positionRect.top()+(float)anItem.positionRect.height()/2.0;

            QColor arrowColor = anItem.color;
            arrowColor.setAlpha( arrowColor.alpha()*((float)item.alpha / 100.0) );

            QPen arrowPen(arrowColor);
            painter.setPen(arrowPen);
            painter.drawLine(QPoint(x, p_height - y), QPoint(x + anItem.vector[0], p_height - (y + anItem.vector[1])));

            a = 2.5;
            // arrow head
            QPoint arrowHead = QPoint(x + anItem.vector[0], y + anItem.vector[1]);
            // arrow head right
            rotateVector(5.0/6.0*M_PI, anItem.vector[0], anItem.vector[1], nx, ny);
            // check if rotation is nan
            if (nx!=nx)
                nx=0;
            if (ny!=ny)
                ny=0;
            QPoint arrowHeadRight = QPoint(x + anItem.vector[0] + nx * a, y + anItem.vector[1] + ny * a);
            // arrow head left
            rotateVector(-5.0/6.0*M_PI, anItem.vector[0], anItem.vector[1], nx, ny);
            if (nx!=nx)
                nx=0;
            if (ny!=ny)
                ny=0;
            QPoint arrowHeadLeft = QPoint(x + anItem.vector[0] + nx * a, y + anItem.vector[1] + ny * a);

            // draw arrow head
            painter.drawLine(arrowHead, arrowHeadRight);
            painter.drawLine(arrowHead, arrowHeadLeft);
            painter.drawLine(arrowHeadRight, arrowHeadLeft);

            break;
        }
        case blockType:
        {
            //draw a rectangle
            QColor rectColor = anItem.color;
            rectColor.setAlpha( rectColor.alpha()*((float)item.alpha / 100.0) );
            painter.setBrush(rectColor);

            QRect aRect = anItem.positionRect;

            painter.fillRect(aRect, rectColor);

            break;
        }
        }

        // optionally, draw a grid around the region
        if (item.renderGrid) {
            //draw a rectangle
            QColor gridColor = anItem.gridColor;
            QPen gridPen(gridColor);
            gridPen.setWidth(1);
            painter.setPen(gridPen);
            painter.setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush));  // no fill color

            QRect aRect = anItem.positionRect;

            painter.drawRect(aRect);
        }
    }

}

StatisticsItemList& StatisticsObject::getFrontmostActiveStatisticsItem(unsigned int idx, int& type)
{
    for(int i=0; i<p_activeStatsTypes.size(); i++)
    {
        if (p_activeStatsTypes[i].render)
        {
            type = p_activeStatsTypes[i].type_id;
            return getStatistics(idx, p_activeStatsTypes[i].type_id);
        }
    }

    // if no item is enabled
    return StatisticsObject::emptyStats;
}

// return raw(!) value of frontmost, active statistic item at given position
ValuePairList StatisticsObject::getValuesAt(int x, int y)
{
    int typeID = -1;
    StatisticsItemList statsList = getFrontmostActiveStatisticsItem(p_lastIdx, typeID);

    if( statsList.size() == 0 && typeID == -1 ) // no active statistics
        return ValuePairList();

    StatisticsItemList::iterator it;
    for (it = statsList.begin(); it != statsList.end(); it++)
    {
        StatisticsItem anItem = *it;

        QRect aRect = anItem.positionRect;

        int rawValue1 = anItem.rawValues[0];
        int rawValue2 = anItem.rawValues[1];

        if( aRect.contains(x,y) )
        {
            ValuePairList values;

            if( anItem.type == blockType )
            {
                values.append( ValuePair(getTypeName(typeID), QString::number(rawValue1)) );
            }
            else if( anItem.type == arrowType )
            {
                values.append( ValuePair(QString("%1[x]").arg(getTypeName(typeID)), QString::number(rawValue1)) );
                values.append( ValuePair(QString("%1[y]").arg(getTypeName(typeID)), QString::number(rawValue2)) );
            }

            return values;
        }
    }

    ValuePairList defaultValueList;
    defaultValueList.append( ValuePair(getTypeName(typeID), "-") );

    return defaultValueList;
}

StatisticsItemList& StatisticsObject::getStatistics(int frameNumber, int type) {
    if ((p_stats->m_columns > frameNumber) && (p_stats->m_rows > type))
        return (*p_stats)[frameNumber][type];
    else
        return StatisticsObject::emptyStats;
}

StatisticsItemList StatisticsObject::getSimplifiedStatistics(int frameNumber, int type, int threshold, QColor color) {
    StatisticsItemList stats, tmpStats;
    if ((p_stats->m_columns > frameNumber) && (p_stats->m_rows > type)) {
        stats = (*p_stats)[frameNumber][type];
        StatisticsItemList::iterator it = stats.begin();
        while (it != stats.end()) {
            if ((it->type == arrowType) && ((it->positionRect.width() < threshold) || (it->positionRect.height() < threshold))) {
                tmpStats.push_back(*it); // copy over to tmp list of blocks
                it = stats.erase(it); // and erase from original
            } else
                it++;
        }
        while (!tmpStats.empty()) {
            std::list<float> x_val, y_val;
            StatisticsItem newItem;

            it = tmpStats.begin();
            x_val.push_back(it->vector[0]);
            y_val.push_back(it->vector[1]);
            newItem = *it;

            // new items size & position is clamped to a grid
            int posX = it->positionRect.left() - (it->positionRect.left() % threshold);
            int posY = it->positionRect.top() - (it->positionRect.top() % threshold);
            unsigned int width = threshold;
            unsigned int height = threshold;
            newItem.positionRect = QRect(posX, posY, width, height);

            // combined blocks are always red?!
            newItem.gridColor = color;
            newItem.color = color;
            newItem.color.setAlpha(255);
            it = tmpStats.erase(it);

            while (it != tmpStats.end()) {
                if ( newItem.positionRect.contains( it->positionRect ) ) // intersects with newItem's block
                {
                    x_val.push_back(it->vector[0]);
                    y_val.push_back(it->vector[1]);
                    it = tmpStats.erase(it);
                } else ++it;
            }

            // update new Item
            newItem.vector[0] = median(x_val);
            newItem.vector[1] = median(y_val);

            stats.push_back(newItem);
        }
        return stats;
    } else
        return StatisticsObject::emptyStats;
}

bool StatisticsObject::parseFile(std::string filename)
{
    try {
        std::vector<std::string> row;
        std::string line;
        int i=-1;
        std::ifstream in(filename.c_str());

        if (in.fail()) return false;

        // cleanup old types
        for (int i=0; i<(int)p_types.size(); i++)
            delete p_types[i];

        p_types.clear();

        int numFrames = 0;

        // scan headerlines first
        // also count the lines per Frame for more efficient memory allocation (which is not yet implemented)
        // if an ID is used twice, the data of the first gets overwritten
        VisualizationType* newType = NULL;

        while(getline(in, line) && in.good())
        {
            parseCSVLine(row, line, ';');

            // check for max POC
            if (row[0][0] != '%')
            {
                int poc = StringToNumber(row[0]);
                if( poc+1 > numFrames )
                    numFrames = poc+1;

                if(newType == NULL)
                    continue;   // for now, we are only interested in headers
            }

            if (((row[1] == "type") || (row[0][0] != '%')) && (newType != 0))
            {
                // last type is complete
                if (i >= (int)p_types.size())
                    p_types.resize(i+1);

                p_types[i] = newType;

                newType = NULL; // start from scratch
            }

            if (row[1] == "type")
            {
                i = StringToNumber(row[2]);

                newType = new VisualizationType(row);   // gets its type from row info
            }
            else if (row[1] == "mapColor")
            {
                int id = StringToNumber(row[2]);

                // assign color
                unsigned char r = StringToNumber(row[3]);
                unsigned char g = StringToNumber(row[4]);
                unsigned char b = StringToNumber(row[5]);
                unsigned char a = StringToNumber(row[6]);
                newType->colorMap[id] = QColor(r,g,b,a);
            }
            else if (row[1] == "range")
            {
                newType->colorRange = new ColorRange(row);
            }
            else if (row[1] == "defaultRange")
            {
                newType->colorRange = new DefaultColorRange(row);
            }
            else if (row[1] == "vectorColor")
            {
                unsigned char r = StringToNumber(row[2]);
                unsigned char g = StringToNumber(row[3]);
                unsigned char b = StringToNumber(row[4]);
                unsigned char a = StringToNumber(row[5]);
                newType->vectorColor = QColor(r,g,b,a);
            }
            else if (row[1] == "gridColor")
            {
                unsigned char r = StringToNumber(row[2]);
                unsigned char g = StringToNumber(row[3]);
                unsigned char b = StringToNumber(row[4]);
                unsigned char a = 255;
                newType->gridColor = QColor(r,g,b,a);
            }
            else if (row[1] == "scaleFactor")
            {
                if (newType != NULL)
                    newType->vectorSampling = StringToNumber(row[2]);
            }
            else if (row[1] == "scaleToBlockSize")
            {
                if (newType != NULL)
                    newType->scaleToBlockSize = (row[2] == "1") ? true : false;
            }
            else if (row[1] == "syntax-version")
            {
                // TODO: check syntax version for compatibility!
                //if (row[2] != "v1.01") throw "Wrong syntax version (should be v1.01).";
            }
            else if (row[1] == "seq-specs")
            {
                QString seqName = QString::fromStdString(row[2]);
                QString layerId = QString::fromStdString(row[3]);
                QString fullName = seqName + "_" + layerId;
                setName( fullName );
                setWidth(StringToNumber(row[4]));
                setHeight(StringToNumber(row[5]));
                setFrameRate(StringToNumber(row[6]));
            }
        }

        setNumFrames(numFrames);

        // prepare the data structures
        p_stats = new Matrix<StatisticsItemList>(numFrames, p_types.size());

        // second pass to get all the data in
        in.clear();
        in.seekg(0);
        int poc, typeID, value1, value2;
        StatisticsItem item;

        while (std::getline(in, line) && in.good())
        {
            if (line[0] == '%')
                continue; // skip header lines

            parseCSVLine(row, line, ';');

            poc = StringToNumber(row[0]);
            typeID = StringToNumber(row[5]);
            value1 = StringToNumber(row[6]);
            value2 = (row.size()>=8)?StringToNumber(row[7]):-1;

            int posX = StringToNumber(row[1]);
            int posY = StringToNumber(row[2]);
            unsigned int width = StringToNumber(row[3]);
            unsigned int height = StringToNumber(row[4]);

            item.type = ((p_types[typeID]->type == colorMapType) || (p_types[typeID]->type == colorRangeType)) ? blockType : arrowType;

            item.positionRect = QRect(posX, posY, width, height);

            item.rawValues[0] = value1;
            item.rawValues[1] = value2;

            if (p_types[typeID]->type == colorMapType)
            {
                ColorMap colorMap = p_types[typeID]->colorMap;
                item.color = colorMap[value1];
            }
            else if (p_types[typeID]->type == colorRangeType)
            {
                if (p_types[typeID]->scaleToBlockSize)
                    item.color = p_types[typeID]->colorRange->getColor((float)value1 / (float)(item.positionRect.width() * item.positionRect.height()));
                else
                    item.color = p_types[typeID]->colorRange->getColor((float)value1);
            }
            else if (p_types[typeID]->type == vectorType)
            {
                // find color
                item.color = p_types[typeID]->vectorColor;

                // calculate the vector size
                item.vector[0] = (float)value1 / p_types[typeID]->vectorSampling;
                item.vector[1] = (float)value2 / p_types[typeID]->vectorSampling;
            }

            // set grid color. if unset for this type, use color of type for grid, too
            if (p_types[typeID]->gridColor.isValid())
            {
                item.gridColor = p_types[typeID]->gridColor;
            }
            else
            {
                item.gridColor = item.color;
            }

            (*p_stats)[poc][typeID].push_back(item);
        }
        in.close();

    } // try
    catch ( const char * str ) {
        std::cerr << "Error while parsing: " << str << '\n';
        return false;
    }
    catch (...) {
        std::cerr << "Error while parsing.";
        return false;
    }

    return true;
}

void StatisticsObject::parseCSVLine(std::vector<std::string> &record, const std::string& line, char delimiter)
{
    int linepos=0;
    int inquotes=false;
    char c;
    int linemax=line.length();
    std::string curstring;
    record.clear();

    while(line[linepos]!=0 && linepos < linemax)
    {

        c = line[linepos];

        if (!inquotes && curstring.length()==0 && c=='"')
        {
            //beginquotechar
            inquotes=true;
        }
        else if (inquotes && c=='"')
        {
            //quotechar
            if ( (linepos+1 <linemax) && (line[linepos+1]=='"') )
            {
                //encountered 2 double quotes in a row (resolves to 1 double quote)
                curstring.push_back(c);
                linepos++;
            }
            else
            {
                //endquotechar
                inquotes=false;
            }
        }
        else if (!inquotes && c==delimiter)
        {
            //end of field
            record.push_back( curstring );
            curstring="";
        }
        else if (!inquotes && (c=='\r' || c=='\n') )
        {
            record.push_back( curstring );
            return;
        }
        else
        {
            curstring.push_back(c);
        }
        linepos++;
    }
    record.push_back( curstring );
    return;
}

QString StatisticsObject::getTypeName(int type) {
    return p_types[type]->name;
}

std::vector<int> StatisticsObject::getTypeIDs() {
    std::vector<int> tmp(p_types.size());
    for (int i=0; i<(int)p_types.size(); i++) {
        if (p_types[i] == 0)
            tmp[i] = -1;
        else
            tmp[i] = p_types[i]->id;
    }
    return tmp;
}
