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

#include "yuvfile.h"

void rotateVector(float angle, float vx, float vy, float &nx, float &ny)
{
    float s = sinf(angle);
    float c = cosf(angle);

    nx = c * vx + s * vy;
    ny = -s * vx + c * vy;

     //normalize vector
    float n_abs = sqrtf( nx*nx + ny*ny );
    nx /= n_abs;
    ny /= n_abs;
}

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

    QtConcurrent::run(this, &StatisticsObject::readFramePositionsFromFile);
}
StatisticsObject::~StatisticsObject() {}

void StatisticsObject::setInternalScaleFactor(int internalScaleFactor)
{
    internalScaleFactor = clip(internalScaleFactor, 1, MAX_SCALE_FACTOR);

    if(p_internalScaleFactor!=internalScaleFactor)
    {
        p_internalScaleFactor = internalScaleFactor;
        emit informationChanged();
    }
}

void StatisticsObject::loadImage(int frameIdx)
{
    if (frameIdx==INT_INVALID)
    {
        p_displayImage = QPixmap();
        return;
    }

    // create empty image
    QImage tmpImage(internalScaleFactor()*width(), internalScaleFactor()*height(), QImage::Format_ARGB32);
    tmpImage.fill(qRgba(0, 0, 0, 0));   // clear with transparent color
    p_displayImage.convertFromImage(tmpImage);

    // draw statistics
    drawStatisticsImage(frameIdx);
    p_lastIdx = frameIdx;
}

void StatisticsObject::drawStatisticsImage(unsigned int frameIdx)
{
    // draw statistics (inverse order)
    for(int i=p_statsTypeList.count()-1; i>=0; i--)
    {
        if (!p_statsTypeList[i].render)
            continue;

        StatisticsItemList stats = getStatistics(frameIdx, p_statsTypeList[i].typeID);
        drawStatisticsImage(stats, p_statsTypeList[i]);
    }
}

void StatisticsObject::drawStatisticsImage(StatisticsItemList statsList, StatisticsType statsType)
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
            QRect displayRect = QRect(aRect.left()*internalScaleFactor(), aRect.top()*internalScaleFactor(), aRect.width()*internalScaleFactor(), aRect.height()*internalScaleFactor());

            int x,y;

            // start vector at center of the block
            x = displayRect.left()+displayRect.width()/2;
            y = displayRect.top()+displayRect.height()/2;

            QPoint startPoint = QPoint(x,y);

            int vx = anItem.vector[0];
            int vy = anItem.vector[1];


            QPoint arrowBase = QPoint(x+internalScaleFactor()*vx, y+internalScaleFactor()*vy);
            QColor arrowColor = anItem.color;
            //arrowColor.setAlpha( arrowColor.alpha()*((float)statsType.alphaFactor / 100.0) );

            QPen arrowPen(arrowColor);
            painter.setPen(arrowPen);
            painter.drawLine(startPoint, arrowBase);

            if( vx == 0 && vy == 0 )
            {
                // TODO: draw single point to indicate zero vector
            }
            else
            {
                // draw an arrow
                float nx, ny;

                // TODO: scale arrow head with
                float a = internalScaleFactor()*4;    // length of arrow
                float b = internalScaleFactor()*2;    // base width of arrow

                float n_abs = sqrtf( vx*vx + vy*vy );
                float vxf = (float) vx / n_abs;
                float vyf = (float) vy / n_abs;

                QPoint arrowTip = arrowBase + QPoint(vxf*a+0.5,vyf*a+0.5);

                // arrow head right
                rotateVector(-M_PI_2, -vx, -vy, nx, ny);
                QPoint offsetRight = QPoint(nx*b+0.5, ny*b+0.5);
                QPoint arrowHeadRight = arrowBase + offsetRight;

                // arrow head left
                rotateVector(M_PI_2, -vx, -vy, nx, ny);
                QPoint offsetLeft = QPoint(nx*b+0.5, ny*b+0.5);
                QPoint arrowHeadLeft = arrowBase + offsetLeft;

                // draw arrow head
                QPoint points[3] = {arrowTip, arrowHeadRight, arrowHeadLeft};
                painter.setBrush(arrowColor);
                painter.drawPolygon(points, 3);
            }

            break;
        }
        case blockType:
        {
            //draw a rectangle
            QColor rectColor = anItem.color;
            rectColor.setAlpha( rectColor.alpha()*((float)statsType.alphaFactor / 100.0) );
            painter.setBrush(rectColor);

            QRect aRect = anItem.positionRect;
            QRect displayRect = QRect(aRect.left()*internalScaleFactor(), aRect.top()*internalScaleFactor(), aRect.width()*internalScaleFactor(), aRect.height()*internalScaleFactor());

            painter.fillRect(displayRect, rectColor);

            break;
        }
        }

        // optionally, draw a grid around the region
        if (statsType.renderGrid) {
            //draw a rectangle
            QColor gridColor = anItem.gridColor;
            QPen gridPen(gridColor);
            gridPen.setWidth(1);
            painter.setPen(gridPen);
            painter.setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush));  // no fill color

            QRect aRect = anItem.positionRect;
            QRect displayRect = QRect(aRect.left()*internalScaleFactor(), aRect.top()*internalScaleFactor(), aRect.width()*internalScaleFactor(), aRect.height()*internalScaleFactor());

            painter.drawRect(displayRect);
        }
    }

}

// return raw(!) value of frontmost, active statistic item at given position
ValuePairList StatisticsObject::getValuesAt(int x, int y)
{
    ValuePairList valueList;

    for(int i=0; i<p_statsTypeList.count(); i++)
    {
        if (p_statsTypeList[i].render)  // only show active values
        {
            int typeID = p_statsTypeList[i].typeID;
            StatisticsItemList statsList = getStatistics(p_lastIdx, typeID);

            if( statsList.size() == 0 && typeID == -1 ) // no active statistics
                continue;

            StatisticsType* aType = getStatisticsType(typeID);
            Q_ASSERT(aType->typeID != -1 && aType->typeID == typeID);

            // find item of this type at requested position
            StatisticsItemList::iterator it;
            bool foundStats = false;
            for (it = statsList.begin(); it != statsList.end(); it++)
            {
                StatisticsItem anItem = *it;

                QRect aRect = anItem.positionRect;

                int rawValue1 = anItem.rawValues[0];
                int rawValue2 = anItem.rawValues[1];

                if( aRect.contains(x,y) )
                {
                    if( anItem.type == blockType )
                    {
                        valueList.append( ValuePair(aType->typeName, QString::number(rawValue1)) );
                    }
                    else if( anItem.type == arrowType )
                    {
                        valueList.append( ValuePair(QString("%1[x]").arg(aType->typeName), QString::number(rawValue1)) );
                        valueList.append( ValuePair(QString("%1[y]").arg(aType->typeName), QString::number(rawValue2)) );
                    }

                    foundStats = true;
                    break;
                }
            }

            if(!foundStats)
                valueList.append( ValuePair(aType->typeName, "-") );
        }
    }

    return valueList;
}

StatisticsItemList StatisticsObject::getStatistics(int frameIdx, int type)
{
    // if requested statistics are not in cache, read from file
    if( !(p_statsCache.contains(frameIdx) ) )
    {
        readStatisticsFromFile(frameIdx);
    }

    return p_statsCache[frameIdx][type];
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
        int lastSignalAtFrame = 0;
        while (!inputFile.atEnd())
        {
            qint64 lineStartPos = inputFile.pos();

            // read one line
            QByteArray aLineByteArray = inputFile.readLine();
            QString aLine(aLineByteArray);

            // get components of this line
            QStringList rowItemList = parseCSVLine(aLine, ';');

            // ignore empty stuff
            if (rowItemList[0].isEmpty())
                continue;

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

                    if( p_numFrames > lastSignalAtFrame+10 )
                    {
                        emit informationChanged();
                        lastSignalAtFrame = p_numFrames;
                    }
                }
            }
        }
        p_pocStartList[lastPOC]=lastPOCStart;

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
        QFile inputFile(p_srcFilePath);

        if(inputFile.open(QIODevice::ReadOnly) == false)
            return;

        // cleanup old types
        p_statsTypeList.clear();

        // scan headerlines first
        // also count the lines per Frame for more efficient memory allocation
        // if an ID is used twice, the data of the first gets overwritten
        bool typeParsingActive = false;
        StatisticsType aType;

        while (!inputFile.atEnd())
        {
            // read one line
            QByteArray aLineByteArray = inputFile.readLine();
            QString aLine(aLineByteArray);

            // get components of this line
            QStringList rowItemList = parseCSVLine(aLine, ';');

            if (rowItemList[0].isEmpty())
                continue;

            // either a new type or a line which is not header finishes the last type
            if (((rowItemList[1] == "type") || (rowItemList[0][0] != '%')) && typeParsingActive)
            {
                // last type is complete
                p_statsTypeList.append(aType);

                // start from scratch for next item
                aType = StatisticsType();
                typeParsingActive = false;

                // if we found a non-header line, stop here
                if( rowItemList[0][0] != '%' )
                    return;
            }

            if (rowItemList[1] == "type")   // new type
            {
                aType.typeID = rowItemList[2].toInt();

                aType.readFromRow(rowItemList); // get remaining info from row
                typeParsingActive = true;
            }
            else if (rowItemList[1] == "mapColor")
            {
                int id = rowItemList[2].toInt();

                // assign color
                unsigned char r = (unsigned char)rowItemList[3].toInt();
                unsigned char g = (unsigned char)rowItemList[4].toInt();
                unsigned char b = (unsigned char)rowItemList[5].toInt();
                unsigned char a = (unsigned char)rowItemList[6].toInt();
                aType.colorMap[id] = QColor(r,g,b,a);
            }
            else if (rowItemList[1] == "range")
            {
                aType.colorRange = new ColorRange(rowItemList);
            }
            else if (rowItemList[1] == "defaultRange")
            {
                aType.colorRange = new DefaultColorRange(rowItemList);
            }
            else if (rowItemList[1] == "vectorColor")
            {
                unsigned char r = (unsigned char)rowItemList[2].toInt();
                unsigned char g = (unsigned char)rowItemList[3].toInt();
                unsigned char b = (unsigned char)rowItemList[4].toInt();
                unsigned char a = (unsigned char)rowItemList[5].toInt();
                aType.vectorColor = QColor(r,g,b,a);
            }
            else if (rowItemList[1] == "gridColor")
            {
                unsigned char r = (unsigned char)rowItemList[2].toInt();
                unsigned char g = (unsigned char)rowItemList[3].toInt();
                unsigned char b = (unsigned char)rowItemList[4].toInt();
                unsigned char a = 255;
                aType.gridColor = QColor(r,g,b,a);
            }
            else if (rowItemList[1] == "scaleFactor")
            {
                aType.vectorSampling = rowItemList[2].toInt();
            }
            else if (rowItemList[1] == "scaleToBlockSize")
            {
                aType.scaleToBlockSize = (rowItemList[2] == "1");
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
            QStringList rowItemList = parseCSVLine(aLine, ';');

            if (rowItemList[0].isEmpty())
                continue;

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

            anItem.type = ((p_statsTypeList[typeID].visualizationType == colorMapType) || (p_statsTypeList[typeID].visualizationType == colorRangeType)) ? blockType : arrowType;

            anItem.positionRect = QRect(posX, posY, width, height);

            anItem.rawValues[0] = value1;
            anItem.rawValues[1] = value2;

            StatisticsType statsType = p_statsTypeList[typeID];

            if (statsType.visualizationType == colorMapType)
            {
                ColorMap colorMap = statsType.colorMap;
                anItem.color = colorMap[value1];
            }
            else if (statsType.visualizationType == colorRangeType)
            {
                if (statsType.scaleToBlockSize)
                    anItem.color = statsType.colorRange->getColor((float)value1 / (float)(anItem.positionRect.width() * anItem.positionRect.height()));
                else
                    anItem.color = statsType.colorRange->getColor((float)value1);
            }
            else if (statsType.visualizationType == vectorType)
            {
                // find color
                anItem.color = statsType.vectorColor;

                // calculate the vector size
                anItem.vector[0] = (float)value1 / statsType.vectorSampling;
                anItem.vector[1] = (float)value2 / statsType.vectorSampling;
            }

            // set grid color. if unset for this type, use color of type for grid, too
            if (statsType.gridColor.isValid())
            {
                anItem.gridColor = statsType.gridColor;
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

QStringList StatisticsObject::parseCSVLine(QString line, char delimiter)
{
    // first, trim newline and whitespaces from both ends of line
    line = line.trimmed().replace(" ", "");

    // now split string with delimiter
    return line.split(delimiter);
}

StatisticsType* StatisticsObject::getStatisticsType(int typeID)
{
    for(int i=0; i<p_statsTypeList.count(); i++)
    {
        if( p_statsTypeList[i].typeID == typeID )
            return &p_statsTypeList[i];
    }

    return NULL;
}

void StatisticsObject::setStatisticsTypeList(StatisticsTypeList typeList)
{
    // we do not overwrite our statistics type, we just change their parameters
    foreach(StatisticsType aType, typeList)
    {
        StatisticsType* internalType = getStatisticsType( aType.typeID );

        if( internalType->typeName != aType.typeName )
            continue;

        internalType->render = aType.render;
        internalType->renderGrid = aType.renderGrid;
        internalType->alphaFactor = aType.alphaFactor;
    }
}
