#ifndef TIKZDRAWTEMPLATES_H
#define TIKZDRAWTEMPLATES_H


#include <tikztemplates.h>
#include <statisticsextensions.h>
#include <tikzextensions.h>

#include <QString>
#include <QMap>
#include <QPoint>
#include <QColor>
#include <QPoint>

namespace draw_ext {
    QString num2str(int num);
    QString sanitizeString(QString input);
}

enum drawtype_t{
    block=0,
    vector,
    grid
};
class drawElement{
public:
    virtual QString render()=0;
private:
    QString p_template;
};


class drawBlock : public drawElement{
public:
    drawBlock();
    drawBlock(QPoint start, QPoint stop, QString typeName, int value);
    QString render(){return p_template;}
private:
    QString p_template, p_typeName;


};

class drawVector : public drawElement{
public:
    drawVector();
    drawVector(QPoint start, QPoint stop, QString typeName);
    QString render(){return p_template;}
private:
    QString p_template;
};

class drawGrid :public drawElement{
public:
    drawGrid();
    drawGrid(QPoint start, QPoint stop, QString typeName);
    QString render(){return p_template;}
private:
    QString p_template;


};

class DrawFactory{
public:
    drawElement *getElement(drawtype_t drawtype, QPoint start, QPoint stop, QString typeName, int value)
    {
        typeName = draw_ext::sanitizeString(typeName);
        drawElement *el = NULL;
        switch(drawtype)
        {
        case grid:
        el = new drawGrid(start, stop, typeName);
        break;
        case block:
        {
        el = new drawBlock(start, stop, typeName, value);
        break;
        }
        case vector:
        el = new drawVector(start, stop, typeName);
        break;
        default:
        el = NULL;
        break;
        }
        return el;
    }
};


class StatisticsTikzDrawItem{
public:
    StatisticsTikzDrawItem(QString statName);

    void setType(drawtype_t dtype){p_drawType = dtype;}
    void setValue(int value){p_value = value;}
    void setStartStop(QPoint start, QPoint stop);
    void setToGrid(QPoint shift);

    drawtype_t getDrawType(){return p_drawType;}
    QString render();
private:
    void applyShift();

    QPoint startPoint, stopPoint;
    QString layerName, statTypeName;
    QString stringValue;
    int p_value;
    int shiftX, shiftY;
    drawtype_t p_drawType;
    bool toGrid;

};

typedef QList<StatisticsTikzDrawItem> StatisticsTikzDrawItemList;

class StatisticsTikzDrawLayer{
public:
    StatisticsTikzDrawLayer(StatisticsType statItem, bool gridb);

    void addElements(StatisticsTikzDrawItem item){p_elementsList.append(item);}
    void addElements(StatisticsTikzDrawItemList list);

    QString layer(){return p_layerName;}
    QString statType(){return p_statType;}
    int size(){return p_elementsList.size();}
    QColor minColor(){return p_settings.minColor;}
    QColor maxColor(){return p_settings.maxColor;}
    int minValue(){return p_minValue;}
    int maxValue(){return p_maxValue;}
    bool show(){return p_render;}
    drawtype_t drawType(){return p_drawType;}
    QString render(QPoint shift);
    TikzDrawLayerSettings settings(){return p_settings;}
private:
    int p_minValue, p_maxValue;
    bool p_render;
    QPoint p_topLeft, p_bottomRight;
    QString p_layerName, p_lineType, p_statType;
    StatisticsTikzDrawItemList p_elementsList;
    QString p_drawTemplate;
    drawtype_t p_drawType;
    TikzDrawLayerSettings p_settings;
};


typedef QList<StatisticsTikzDrawLayer> StatisticsTikzDraw;

#endif // TIKZDRAWTEMPLATES_H
