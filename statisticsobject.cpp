#include "statisticsobject.h"

#include <QSettings>
#include <QColor>
#include <QPainter>

#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include <sstream>
#include <map>
#include <iostream>
#include <cmath>

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

typedef matrix<unsigned char> ColorMap;

struct ColorRange {
    ColorRange() {}
    ColorRange(std::vector<std::string> &row) {
        min = StringToNumber(row[2]);
        minColorR = StringToNumber(row[4]);
        minColorG = StringToNumber(row[6]);
        minColorB = StringToNumber(row[8]);
        minColorA = StringToNumber(row[10]);
        max = StringToNumber(row[3]);
        maxColorR = StringToNumber(row[5]);
        maxColorG = StringToNumber(row[7]);
        maxColorB = StringToNumber(row[9]);
        maxColorA = StringToNumber(row[11]);
    }
    virtual ~ColorRange() {};

    virtual void getColor(float value, unsigned char& retR, unsigned char& retG, unsigned char& retB, unsigned char& retA) {
        // clamp the value to [min max]
        if (value > max) value = (float)max;
        if (value < min) value = (float)min;

        retR = minColorR + (unsigned char)( floor((float)value / (float)(max-min) * (float)(maxColorR-minColorR) + 0.5f) );
        retG = minColorG + (unsigned char)( floor((float)value / (float)(max-min) * (float)(maxColorG-minColorG) + 0.5f) );
        retB = minColorB + (unsigned char)( floor((float)value / (float)(max-min) * (float)(maxColorB-minColorB) + 0.5f) );
        retA = minColorA + (unsigned char)( floor((float)value / (float)(max-min) * (float)(maxColorA-minColorA) + 0.5f) );
        return;
    }

    int min, max;
    unsigned char minColorR, minColorG, minColorB, minColorA, maxColorR, maxColorG, maxColorB, maxColorA;
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

struct DefaultColorRange : ColorRange {
    DefaultColorRange(std::vector<std::string> &row) {
        min = StringToNumber(row[2]);
        max = StringToNumber(row[3]);
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

    virtual void getColor(float value, unsigned char& retR, unsigned char& retG, unsigned char& retB, unsigned char& retA) {
        float span = (float)(max-min),
                val = (float)value,
                x = val / span,
                r=1,g=1,b=1,a=1;
        float F,N,K;
        int I;

        // clamp the value to [min max]
        if (value > max) value = (float)max;
        if (value < min) value = (float)min;

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
        retR = (unsigned char)( floor(r * 255.0f + 0.5f) );
        retG = (unsigned char)( floor(g * 255.0f + 0.5f) );
        retB = (unsigned char)( floor(b * 255.0f + 0.5f) );
        retA = (unsigned char)( floor(a * 255.0f + 0.5f) );
        return;
    }

    defaultColormaps_t type;
};

enum visualizationType_t { colorMap, colorRange, vectorType };
struct VisualizationType
{
    VisualizationType(std::vector<std::string> &row) {
        id = StringToNumber(row[2]);
        name = row[3];
        if (row[4] == "map") type = colorMap;
        else if (row[4] == "range") type = colorRange;
        else if (row[4] == "vector") type = vectorType;
        map = 0;
        range = 0;
        vectorColor = 0;
        gridColor = 0;
        vectorSampling = 1;
        scaleToBlockSize = false;
    }
    ~VisualizationType() {
        if (map != 0) { delete map; map = 0; }
        if (range != 0) { delete range; range = 0; }
        if (vectorColor != 0) { delete vectorColor; vectorColor = 0; }
        if (gridColor != 0) { delete gridColor; gridColor = 0; }
    }

    int id;
    std::string name;
    visualizationType_t type;
    bool scaleToBlockSize;

    // only for vector type:
    int vectorSampling;

    // Only one of the next should be set, depending on type
    ColorMap* map;
    ColorRange* range;
    unsigned char *vectorColor;
    unsigned char *gridColor;
};

StatisticsItemList StatisticsObject::emptyStats;

StatisticsObject::StatisticsObject(const QString& srcFileName, QObject* parent) : DisplayObject(parent)
{
    p_stats = NULL;

    parseFile(srcFileName.toStdString());

    // nothing to show by default
    p_activeStatsTypes.clear();

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
    drawStatistics(idx);
}

void StatisticsObject::drawStatistics(unsigned int idx)
{
    // create empty image
    p_displayImage = QImage(p_width, p_height, QImage::Format_ARGB32);
    QPainter painter(&p_displayImage);
    painter.fillRect(p_displayImage.rect(), Qt::black);

    unsigned char c[3];
    QSettings settings;
    QColor color = settings.value("Statistics/SimplificationColor").value<QColor>();
    c[0] = color.red();
    c[1] = color.green();
    c[2] = color.blue();

    bool simplifyStats = settings.value("Statistics/Simplify",false).toBool();
    int simplificationThreshold = settings.value("Statistics/SimplificationSize",0).toInt();

    // TODO: respect zoom factor for simplification here...
    float zoomFactor = 1.0;

    // draw statistics
    for(int i=p_activeStatsTypes.size()-1; i>=0; i--)
    {
        if (!p_activeStatsTypes[i].render) continue;
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

            stats = getSimplifiedStatistics(idx, p_activeStatsTypes[i].type_id, threshold, c);
        } else {
            stats = getStatistics(idx, p_activeStatsTypes[i].type_id);
        }

        drawStatistics(stats, p_activeStatsTypes[i], &painter);
    }
}

void StatisticsObject::drawStatistics(StatisticsItemList& statsList, StatisticsRenderItem &item, QPainter* painter)
{
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
            x = (float)anItem.position[0]+(float)anItem.size[0]/2.0;
            y = (float)anItem.position[1]+(float)anItem.size[1]/2.0;

            QPen arrowPen(QColor(anItem.color[0], anItem.color[1], anItem.color[2], anItem.color[3] * ((float)item.alpha / 100.0)));
            painter->setPen(arrowPen);
            painter->drawLine(QPoint(x, p_height - y), QPoint(x + anItem.direction[0], p_height - (y + anItem.direction[1])));

            a = 2.5;
            // arrow head
            QPoint arrowHead = QPoint(x + anItem.direction[0], p_height - (y + anItem.direction[1]));
            // arrow head right
            rotateVector(5.0/6.0*M_PI, anItem.direction[0], anItem.direction[1], nx, ny);
            QPoint arrowHeadRight = QPoint(x + anItem.direction[0] + nx * a, p_height - y - anItem.direction[1] - ny * a);
            // arrow head left
            rotateVector(-5.0/6.0*M_PI, anItem.direction[0], anItem.direction[1], nx, ny);
            QPoint arrowHeadLeft = QPoint(x + anItem.direction[0] + nx * a, p_height - y - anItem.direction[1] - ny * a);

            // draw arrow head
            painter->drawLine(arrowHead, arrowHeadRight);
            painter->drawLine(arrowHead, arrowHeadLeft);
            painter->drawLine(arrowHeadRight, arrowHeadLeft);

            break;
        }
        case blockType:
        {
            //draw a rectangle
            QColor rectColor = QColor(anItem.color[0], anItem.color[1], anItem.color[2], anItem.color[3] * ((float)item.alpha / 100.0));
            QPen rectPen(rectColor);
            painter->setPen(rectPen);

            QPoint topLeft = QPoint(anItem.position[0], p_height - anItem.position[1]);
            QPoint bottomRight = QPoint(anItem.position[0]+anItem.size[0], p_height- anItem.position[1]-anItem.size[1]);

            QRect aRect = QRect(topLeft, bottomRight);

            painter->fillRect(aRect, rectColor);

            break;
        }
        }

        // optionally, draw a grid around the region
        if (item.renderGrid) {
            //draw a rectangle
            QPen gridPen(QColor(anItem.gridColor[0], anItem.gridColor[1], anItem.gridColor[2]));
            painter->setPen(gridPen);

            QPoint topLeft = QPoint(anItem.position[0], p_height - anItem.position[1]);
            QPoint bottomRight = QPoint(anItem.position[0]+anItem.size[0], p_height- anItem.position[1]-anItem.size[1]);

            QRect aRect = QRect(topLeft, bottomRight);

            painter->drawRect(aRect);
        }
    }

}

StatisticsItemList& StatisticsObject::getStatistics(int frameNumber, int type) {
    if ((p_stats->m_columns > frameNumber) && (p_stats->m_rows > type))
        return (*p_stats)[frameNumber][type];
    else
        return StatisticsObject::emptyStats;
}

StatisticsItemList StatisticsObject::getSimplifiedStatistics(int frameNumber, int type, int threshold, unsigned char color[3]) {
    StatisticsItemList stats, tmpStats;
    if ((p_stats->m_columns > frameNumber) && (p_stats->m_rows > type)) {
        stats = (*p_stats)[frameNumber][type];
        StatisticsItemList::iterator it = stats.begin();
        while (it != stats.end()) {
            if ((it->type == arrowType) && ((it->size[0] < threshold) || (it->size[1] < threshold))) {
                tmpStats.push_back(*it); // copy over to tmp Liste of blocks
                it = stats.erase(it); // and erase from original
            } else
                it++;
        }
        while (!tmpStats.empty()) {
            std::list<float> x_val, y_val;
            StatisticsItem newItem;

            it = tmpStats.begin();
            x_val.push_back(it->direction[0]);
            y_val.push_back(it->direction[1]);
            newItem = *it;
            // new items size & position is clamped to a grid
            newItem.position[0] = it->position[0] - (it->position[0] % threshold);
            newItem.position[1] = it->position[1] - (it->position[1] % threshold);
            newItem.size[0] = threshold;
            newItem.size[1] = threshold;
            // combined blocks are always red
            newItem.gridColor[0] = color[0];
            newItem.gridColor[1] = color[1];
            newItem.gridColor[2] = color[2];
            newItem.color[0] = color[0];
            newItem.color[1] = color[1];
            newItem.color[2] = color[2];
            newItem.color[3] = 255;
            it = tmpStats.erase(it);

            while (it != tmpStats.end()) {
                if (
                        (newItem.position[0] < it->position[0]+it->size[0]) && (newItem.position[0]+newItem.size[0] > it->position[0]) &&
                        (newItem.position[1] < it->position[1]+it->size[1]) && (newItem.position[1]+newItem.size[1] > it->position[1]) ) // intersects with newItem's block
                {
                    x_val.push_back(it->direction[0]);
                    y_val.push_back(it->direction[1]);
                    it = tmpStats.erase(it);
                } else ++it;
            }

            // update new Item
            newItem.direction[0] = median(x_val);
            newItem.direction[1] = median(y_val);
            stats.push_back(newItem);
        }
        return stats;
    } else
        return StatisticsObject::emptyStats;
}

bool StatisticsObject::parseFile(std::string filename)
{
    try {

        //TODO: check file version
        std::vector<std::string> row;
        std::string line;
        int i=-1;
        std::ifstream in(filename.c_str());
        if (in.fail()) return false;

        // cleanup old types
        for (int i=0; i<(int)p_types.size(); i++)
            delete p_types[i];
        p_types.clear();

        std::vector<unsigned int> linesPerFrame;

        // scan headerlines first
        // also count the lines per Frame for more efficient memory allocation (which is not yet implemented)
        // if an ID is used twice, the data of the first gets overwritten
        VisualizationType *newType = 0;
        ColorMap *newMap = 0;
        ColorRange *newRange = 0;
        unsigned char *newVector = 0;
        while (getline(in, line)  && in.good())
        {
            parseCSVLine(row, line, ';');
            if (row[0][0] != '%')
            {
                int n = StringToNumber(row[0]);
                if (n >= (int)linesPerFrame.size()) linesPerFrame.resize(n+1, 0);
                linesPerFrame[n]++;
            }

            if (((row[1] == "type") || (row[0][0] != '%')) && (newType != 0))
            { // last type is complete
                if (newType->type == colorMap)
                    newType->map = newMap;
                else if (newType->type == colorRange)
                    newType->range = newRange;
                else if (newType->type == vectorType)
                    newType->vectorColor = newVector;
                if (i >= (int)p_types.size())
                    p_types.resize(i+1, 0);
                p_types[i] = newType;
                newType = 0;
            }

            if (row[1] == "type")
            {
                i = StringToNumber(row[2]);
                newType = new VisualizationType(row);
                if (newType->type == colorMap)
                    newMap = new ColorMap;
            }
            else if (row[1] == "mapColor")
            {
                assert (newMap != 0);

                int id = StringToNumber(row[2]);
                // resize if necessary
                if (newMap->m_columns <= id)
                    newMap->resize(id+1, 4);

                // assign color
                (*newMap)[id][0] = StringToNumber(row[3]);
                (*newMap)[id][1] = StringToNumber(row[4]);
                (*newMap)[id][2] = StringToNumber(row[5]);
                (*newMap)[id][3] = StringToNumber(row[6]);
            }
            else if (row[1] == "range")
            {
                newRange = new ColorRange(row);
            }
            else if (row[1] == "defaultRange")
            {
                newRange = new DefaultColorRange(row);
            } else if (row[1] == "vectorColor")
            {
                newVector = new unsigned char[4];
                newVector[0] = StringToNumber(row[2]);
                newVector[1] = StringToNumber(row[3]);
                newVector[2] = StringToNumber(row[4]);
                newVector[3] = StringToNumber(row[5]);
            } else if (row[1] == "gridColor")
            {
                unsigned char *newColor = new unsigned char[3];
                newColor[0] = StringToNumber(row[2]);
                newColor[1] = StringToNumber(row[3]);
                newColor[2] = StringToNumber(row[4]);
                newType->gridColor = newColor;
            }
            else if (row[1] == "scaleFactor")
            {
                if (newType != 0)
                    newType->vectorSampling = StringToNumber(row[2]);
            }
            else if (row[1] == "scaleToBlockSize")
            {
                if (newType != 0)
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

        // prepare the data structures
        p_stats = new matrix<StatisticsItemList>(linesPerFrame.size(), p_types.size());

        // second pass to get all the data in
        in.clear();
        in.seekg(0);
        int poc, typeID, otherID;
        StatisticsItem item;
        int lastPOC = 0;

        while (std::getline(in, line)  && in.good())
        {
            if (line[0] == '%') continue; // skip header lines
            parseCSVLine(row, line, ';');

            poc = StringToNumber(row[0]);
            typeID = StringToNumber(row[5]);

            if( poc > lastPOC )
                lastPOC = poc;

            item.position[0] = StringToNumber(row[1]);
            item.position[1] = StringToNumber(row[2]);
            item.size[0] = StringToNumber(row[3]);
            item.size[1] = StringToNumber(row[4]);
            item.type = ((p_types[typeID]->type == colorMap) || (p_types[typeID]->type == colorRange)) ? blockType : arrowType;

            otherID = StringToNumber(row[6]);
            if (p_types[typeID]->type == colorMap)
            {
                item.color[0] = (*p_types[typeID]->map)[otherID][0];
                item.color[1] = (*p_types[typeID]->map)[otherID][1];
                item.color[2] = (*p_types[typeID]->map)[otherID][2];
                item.color[3] = (*p_types[typeID]->map)[otherID][3];
            }
            else if (p_types[typeID]->type == colorRange)
            {
                if (p_types[typeID]->scaleToBlockSize)
                    p_types[typeID]->range->getColor((float)otherID / (float)(item.size[0] * item.size[1]), item.color[0], item.color[1], item.color[2], item.color[3]);
                else
                    p_types[typeID]->range->getColor((float)otherID, item.color[0], item.color[1], item.color[2], item.color[3]);
            }
            else if (p_types[typeID]->type == vectorType)
            {
                // find color
                item.color[0] = p_types[typeID]->vectorColor[0];
                item.color[1] = p_types[typeID]->vectorColor[1];
                item.color[2] = p_types[typeID]->vectorColor[2];
                item.color[3] = p_types[typeID]->vectorColor[3];

                // calculate the vector size
                item.direction[0] = (float)otherID / p_types[typeID]->vectorSampling;
                item.direction[1] = (float)StringToNumber(row[7]) / p_types[typeID]->vectorSampling;
            }
            // set grid color. if unset for type, use color of item itself
            if (p_types[typeID]->gridColor != 0)
            {
                item.gridColor[0] = p_types[typeID]->gridColor[0];
                item.gridColor[1] = p_types[typeID]->gridColor[1];
                item.gridColor[2] = p_types[typeID]->gridColor[2];
            }
            else
            {
                item.gridColor[0] = item.color[0];
                item.gridColor[1] = item.color[1];
                item.gridColor[2] = item.color[2];
            }

            (*p_stats)[poc][typeID].push_back(item);
        }
        in.close();

        setNumFrames(lastPOC+1);

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

std::string StatisticsObject::getTypeName(int type) {
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
