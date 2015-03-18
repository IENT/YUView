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
#include <QTextStream>
#include <QFuture>
#include <QtConcurrent>

#include <iostream>
#include <algorithm>
#if _WIN32 && !__MINGW32__
#define _USE_MATH_DEFINES 1
#include "math.h"
#else
#include <cmath>
#endif

#include "yuvfile.h"

void rotateVector(float angle, float vx, float vy, float &nx, float &ny)
{
    float s = sinf(angle);
    float c = cosf(angle);

    nx = c * vx - s * vy;
    ny = s * vx + c * vy;

    // normalize vector
//    float n_abs = sqrtf( nx*nx + ny*ny );
//    nx /= n_abs;
//    ny /= n_abs;
}

// Type, Map and Range are data structures

typedef QMap<int,QColor> ColorMap;

class ColorRange {
public:
    ColorRange() {}
    ColorRange(QList<QString> row) {
        rangeMin = row[2].toInt();
        unsigned char minColorR = row[4].toInt();
        unsigned char minColorG = row[6].toInt();
        unsigned char minColorB = row[8].toInt();
        unsigned char minColorA = row[10].toInt();
        minColor = QColor( minColorR, minColorG, minColorB, minColorA );

        rangeMax = row[3].toInt();
        unsigned char maxColorR = row[5].toInt();
        unsigned char maxColorG = row[6].toInt();
        unsigned char maxColorB = row[9].toInt();
        unsigned char maxColorA = row[11].toInt();
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

class DefaultColorRange : public ColorRange
{
public:
    DefaultColorRange(QList<QString> &row)
    {
        rangeMin = row[2].toInt();
        rangeMax = row[3].toInt();
        QString str = row[4];
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

private:
    defaultColormaps_t type;
};

enum visualizationType_t { colorMapType, colorRangeType, vectorType };
class VisualizationType
{
public:
    VisualizationType(QList<QString> &row) {
        id = row[2].toInt();
        name = row[3];
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

    // only for vector type
    int vectorSampling;

    // only one of the next should be set, depending on type
    ColorMap colorMap;
    ColorRange* colorRange; // can either be a ColorRange or a DefaultColorRange
    QColor vectorColor;
    QColor gridColor;
};

StatisticsObject::StatisticsObject(const QString& srcFileName, QObject* parent) : DisplayObject(parent)
{
    // get some more information from file
    QFileInfo fileInfo(srcFileName);
    p_srcFilePath = srcFileName;
    p_createdTime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
    p_modifiedTime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");

    QStringList components = srcFileName.split(QDir::separator());
    QString fileName = components.last();
    int lastPoint = fileName.lastIndexOf(".");
    p_name = fileName.left(lastPoint);

    // try to get width, height, framerate from filename
    YUVFile::formatFromFilename(srcFileName, &p_width, &p_height, &p_frameRate, &p_numFrames, false);
    readHeaderFromFile();

    QFuture<void> future = QtConcurrent::run(this, &StatisticsObject::readFramePositionsFromFile);

    // nothing to show by default
    p_activeStatsTypes.clear();

    // TODO: might be reasonable to have a map instead of a vector to allow not ascending type identifiers
    QList<int> types = getTypeIDs();
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
    for (int i=0; i<p_typeList.count(); i++)
        delete p_typeList[i];
}

void StatisticsObject::loadImage(int idx)
{
    if (idx==INT_INVALID)
    {
        p_displayImage = QPixmap();
        return;
    }
    // create empty image
    QImage tmpImage(scaleFactor()*width(), scaleFactor()*height(), QImage::Format_ARGB32);
    tmpImage.fill(qRgba(0, 0, 0, 0));   // clear with transparent color
    p_displayImage.convertFromImage(tmpImage);

    // TODO: load corresponding statistics from file

    // draw statistics
    drawStatisticsImage(idx);
    p_lastIdx = idx;
}

void StatisticsObject::drawStatisticsImage(unsigned int idx)
{
    // draw statistics (inverse order)
    for(int i=p_activeStatsTypes.size()-1; i>=0; i--)
    {
        if (!p_activeStatsTypes[i].render)
            continue;

        StatisticsItemList stats = getStatistics(idx, p_activeStatsTypes[i].type_id);
        drawStatisticsImage(stats, p_activeStatsTypes[i]);
    }
}

void StatisticsObject::drawStatisticsImage(StatisticsItemList statsList, StatisticsRenderItem item)
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
            QRect aRect = anItem.positionRect;
            QRect displayRect = QRect(aRect.left()*scaleFactor(), aRect.top()*scaleFactor(), aRect.width()*scaleFactor(), aRect.height()*scaleFactor());

            int x,y;

            // start vector at center of the block
            x = displayRect.left()+displayRect.width()/2;
            y = displayRect.top()+displayRect.height()/2;

            QPoint startPoint = QPoint(x,y);

            int vx = anItem.vector[0];
            int vy = anItem.vector[1];

            QPoint arrowHeadPoint = QPoint(x+scaleFactor()*vx, y+scaleFactor()*vy);

            QColor arrowColor = anItem.color;
            arrowColor.setAlpha( arrowColor.alpha()*((float)item.alpha / 100.0) );

            QPen arrowPen(arrowColor);
            painter.setPen(arrowPen);
            painter.drawLine(startPoint, arrowHeadPoint);

            if( vx == 0 && vy == 0 )
            {
                // TODO: draw single point to indicate zero vector
            }
            else
            {
                // draw an arrow
                float nx, ny;

                // TODO: scale arrow head with
                float a = scaleFactor()*0.25;    // length of arrow head wings

                // arrow head right
                rotateVector(5.0/6.0*M_PI, vx, vy, nx, ny);
                QPoint offsetRight = QPoint(nx*a+0.5, ny*a+0.5);
                QPoint arrowHeadRight = arrowHeadPoint + offsetRight;

                // arrow head left
                rotateVector(-5.0/6.0*M_PI, vx, vy, nx, ny);
                QPoint offsetLeft = QPoint(nx*a+0.5, ny*a+0.5);
                QPoint arrowHeadLeft = arrowHeadPoint + offsetLeft;

                // draw arrow head
                static const QPointF points[3] = {arrowHeadPoint, arrowHeadRight, arrowHeadLeft};
                painter.setBrush(arrowColor);
                painter.drawPolygon(points, 4);
            }

            break;
        }
        case blockType:
        {
            //draw a rectangle
            QColor rectColor = anItem.color;
            rectColor.setAlpha( rectColor.alpha()*((float)item.alpha / 100.0) );
            painter.setBrush(rectColor);

            QRect aRect = anItem.positionRect;
            QRect displayRect = QRect(aRect.left()*scaleFactor(), aRect.top()*scaleFactor(), aRect.width()*scaleFactor(), aRect.height()*scaleFactor());

            painter.fillRect(displayRect, rectColor);

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
            QRect displayRect = QRect(aRect.left()*scaleFactor(), aRect.top()*scaleFactor(), aRect.width()*scaleFactor(), aRect.height()*scaleFactor());

            painter.drawRect(displayRect);
        }
    }

}

StatisticsItemList StatisticsObject::getFrontmostActiveStatisticsItem(unsigned int idx, int& type)
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
    return StatisticsItemList();
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

StatisticsItemList StatisticsObject::getStatistics(int frameNumber, int type)
{
    // if requested statistics are not in cache, read from file
    if( !(p_statsCache.contains(frameNumber) && p_statsCache[frameNumber].contains(type) && p_statsCache[frameNumber][type].count() > 0 ) )
    {
        readStatisticsFromFile(frameNumber);
    }

    return p_statsCache[frameNumber][type];
}

void StatisticsObject::readFramePositionsFromFile()
{
    try {
        QFile inputFile(p_srcFilePath);

        if(inputFile.open(QIODevice::ReadOnly) == false)
            return;

        int lastPOC = -1;
        qint64 lastPOCStart = -1;
        int numFrames = 0;
        while (!inputFile.atEnd())
        {
            qint64 lineStartPos = inputFile.pos();

            // read one line
            QByteArray aLineByteArray = inputFile.readLine();
            QString aLine(aLineByteArray);

            // get components of this line
            QList<QString> rowItemList = parseCSVLine(aLine, ';');

            // ignore headers
            if (rowItemList[0][0] == '%')
                continue;

            // check for POC information
            int poc = rowItemList[0].toInt();

            // check if we found a new POC
            if( poc != lastPOC )
            {
                // finalize last POC
                if( lastPOC != -1 )
                    p_pocStartList[lastPOC] = lastPOCStart;

                // start with new POC
                lastPOC = poc;
                lastPOCStart = lineStartPos;

                // update number of frames
                if( poc+1 > numFrames )
                {
                    numFrames = poc+1;
                    p_numFrames = numFrames;
                }
            }
        }

        emit informationChanged();
        inputFile.close();

    } // try
    catch ( const char * str ) {
        std::cerr << "Error while parsing meta data: " << str << '\n';
        return;
    }
    catch (...) {
        std::cerr << "Error while parsing meta data.";
        return;
    }

    return;
}

void StatisticsObject::readHeaderFromFile()
{
    try {
        int typeID = -1;
        QFile inputFile(p_srcFilePath);

        if(inputFile.open(QIODevice::ReadOnly) == false)
            return;

        // cleanup old types
        for (int i=0; i<p_typeList.count(); i++)
            delete p_typeList[i];
        p_typeList.clear();

        // scan headerlines first
        // also count the lines per Frame for more efficient memory allocation
        // if an ID is used twice, the data of the first gets overwritten
        VisualizationType* newType = NULL;

        while (!inputFile.atEnd())
        {
            // read one line
            QByteArray aLineByteArray = inputFile.readLine();
            QString aLine(aLineByteArray);

            // get components of this line
            QList<QString> rowItemList = parseCSVLine(aLine, ';');

            // either a new type or a line which is not header finishes the last type
            if (((rowItemList[1] == "type") || (rowItemList[0][0] != '%')) && (newType != NULL))
            {
                // last type is complete
                p_typeList[typeID] = newType;

                // start from scratch for next item
                newType = NULL;

                // if we found a non-header line, stop here
                if( rowItemList[0][0] != '%' )
                    return;
            }

            if (rowItemList[1] == "type")
            {
                typeID = rowItemList[2].toInt();

                newType = new VisualizationType(rowItemList);   // gets its type from row info
            }
            else if (rowItemList[1] == "mapColor")
            {
                int id = rowItemList[2].toInt();

                // assign color
                unsigned char r = (unsigned char)rowItemList[3].toInt();
                unsigned char g = (unsigned char)rowItemList[4].toInt();
                unsigned char b = (unsigned char)rowItemList[5].toInt();
                unsigned char a = (unsigned char)rowItemList[6].toInt();
                newType->colorMap[id] = QColor(r,g,b,a);
            }
            else if (rowItemList[1] == "range")
            {
                newType->colorRange = new ColorRange(rowItemList);
            }
            else if (rowItemList[1] == "defaultRange")
            {
                newType->colorRange = new DefaultColorRange(rowItemList);
            }
            else if (rowItemList[1] == "vectorColor")
            {
                unsigned char r = (unsigned char)rowItemList[2].toInt();
                unsigned char g = (unsigned char)rowItemList[3].toInt();
                unsigned char b = (unsigned char)rowItemList[4].toInt();
                unsigned char a = (unsigned char)rowItemList[5].toInt();
                newType->vectorColor = QColor(r,g,b,a);
            }
            else if (rowItemList[1] == "gridColor")
            {
                unsigned char r = (unsigned char)rowItemList[2].toInt();
                unsigned char g = (unsigned char)rowItemList[3].toInt();
                unsigned char b = (unsigned char)rowItemList[4].toInt();
                unsigned char a = 255;
                newType->gridColor = QColor(r,g,b,a);
            }
            else if (rowItemList[1] == "scaleFactor")
            {
                if (newType != NULL)
                    newType->vectorSampling = rowItemList[2].toInt();
            }
            else if (rowItemList[1] == "scaleToBlockSize")
            {
                if (newType != NULL)
                    newType->scaleToBlockSize = (rowItemList[2] == "1");
            }
            else if (rowItemList[1] == "syntax-version")
            {
                // TODO: check syntax version for compatibility!
                //if (row[2] != "v1.01") throw "Wrong syntax version (should be v1.01).";
            }
            else if (rowItemList[1] == "seq-specs")
            {
                QString seqName = rowItemList[2];
                QString layerId = rowItemList[3];
                QString fullName = seqName + "_" + layerId;
                if(!seqName.isEmpty())
                    setName( fullName );
                if (rowItemList[4].toInt()>0)
                    setWidth(rowItemList[4].toInt());
                if (rowItemList[5].toInt()>0)
                    setHeight(rowItemList[5].toInt());
                if (rowItemList[6].toDouble()>0.0)
                    setFrameRate(rowItemList[6].toDouble());
            }
        }

        inputFile.close();

    } // try
    catch ( const char * str ) {
        std::cerr << "Error while parsing meta data: " << str << '\n';
        return;
    }
    catch (...) {
        std::cerr << "Error while parsing meta data.";
        return;
    }

    return;
}

void StatisticsObject::readStatisticsFromFile(int frameIdx)
{
    try {
        QFile inputFile(p_srcFilePath);

        if(inputFile.open(QIODevice::ReadOnly) == false)
            return;

        QTextStream in(&inputFile);

        qint64 startPos = p_pocStartList[frameIdx];

        // fast forward
        in.seek(startPos);

        StatisticsItem anItem;

        while (!in.atEnd())
        {
            // read one line
            QString aLine = in.readLine();

            // get components of this line
            QList<QString> rowItemList = parseCSVLine(aLine, ';');

            int poc = rowItemList[0].toInt();

            // if there is a new poc, we are done here!
            if( poc != frameIdx )
                break;

            int typeID = rowItemList[5].toInt();
            int value1 = rowItemList[6].toInt();
            int value2 = (rowItemList.count()>=8)?rowItemList[7].toInt():0;

            int posX = rowItemList[1].toInt();
            int posY = rowItemList[2].toInt();
            unsigned int width = rowItemList[3].toUInt();
            unsigned int height = rowItemList[4].toUInt();

            anItem.type = ((p_typeList[typeID]->type == colorMapType) || (p_typeList[typeID]->type == colorRangeType)) ? blockType : arrowType;

            anItem.positionRect = QRect(posX, posY, width, height);

            anItem.rawValues[0] = value1;
            anItem.rawValues[1] = value2;

            if (p_typeList[typeID]->type == colorMapType)
            {
                ColorMap colorMap = p_typeList[typeID]->colorMap;
                anItem.color = colorMap[value1];
            }
            else if (p_typeList[typeID]->type == colorRangeType)
            {
                if (p_typeList[typeID]->scaleToBlockSize)
                    anItem.color = p_typeList[typeID]->colorRange->getColor((float)value1 / (float)(anItem.positionRect.width() * anItem.positionRect.height()));
                else
                    anItem.color = p_typeList[typeID]->colorRange->getColor((float)value1);
            }
            else if (p_typeList[typeID]->type == vectorType)
            {
                // find color
                anItem.color = p_typeList[typeID]->vectorColor;

                // calculate the vector size
                anItem.vector[0] = (float)value1 / p_typeList[typeID]->vectorSampling;
                anItem.vector[1] = (float)value2 / p_typeList[typeID]->vectorSampling;
            }

            // set grid color. if unset for this type, use color of type for grid, too
            if (p_typeList[typeID]->gridColor.isValid())
            {
                anItem.gridColor = p_typeList[typeID]->gridColor;
            }
            else
            {
                anItem.gridColor = anItem.color;
            }

            p_statsCache[poc][typeID].append(anItem);
        }
        inputFile.close();

    } // try
    catch ( const char * str ) {
        std::cerr << "Error while parsing: " << str << '\n';
        return;
    }
    catch (...) {
        std::cerr << "Error while parsing.";
        return;
    }

    return;
}

QList<QString> StatisticsObject::parseCSVLine(QString line, char delimiter)
{
    // first, trim newline and whitespaces from both ends of line
    line = line.trimmed().replace(" ", "");

    // now split string with delimiter
    return line.split(delimiter);
}

QString StatisticsObject::getTypeName(int type) {
    return p_typeList[type]->name;
}

QList<int> StatisticsObject::getTypeIDs() {
    QList<int> tmpList;
    QMapIterator<int, VisualizationType*> it(p_typeList);
    while (it.hasNext()) {
        it.next();
        tmpList.append(it.value()->id);
    }
    return tmpList;
}
