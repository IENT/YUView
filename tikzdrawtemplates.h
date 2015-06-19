#ifndef TIKZDRAWTEMPLATES_H
#define TIKZDRAWTEMPLATES_H


#include <tikztemplates.h>
#include <statisticsextensions.h>


#include <QString>
#include <QMap>
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
    StatisticsTikzDrawItem(QString statName)
    {
        statTypeName = statName;
        toGrid = 0;
        shiftX = 0;
        shiftY = 0;
        p_value = -1;
    }

    void setType(drawtype_t dtype)
    {
        p_drawType = dtype;
    }

    drawtype_t getDrawType(){return p_drawType;}


    void setStartStop(QPoint start, QPoint stop)
    {
        startPoint = start;
        stopPoint = stop;
    }

    void setValue(int value)
    {
        p_value = value;
    }

    void setToGrid(QPoint shift)
    {
        toGrid = 1;
        shiftX = shift.x();
        shiftY = shift.y();

    }





    QString render()
    {
        if(toGrid)
            applyShift();

        /*Create factory*/
         DrawFactory *drawFactory = new DrawFactory();

         drawElement *el = drawFactory->getElement(p_drawType, startPoint, stopPoint, statTypeName, p_value);


        return el->render();
    }


    QString layerName, statTypeName;
    QPoint startPoint, stopPoint;
    QString stringValue;

    int shiftX, shiftY;

private:
    void applyShift()
    {
        startPoint.setX(startPoint.x() - shiftX);
        startPoint.setY(startPoint.y() - shiftY);

        stopPoint.setX(stopPoint.x() - shiftX);
        stopPoint.setY(stopPoint.y() - shiftY);
    }
    int p_value;
    drawtype_t p_drawType;
    bool toGrid;

};

typedef QList<StatisticsTikzDrawItem> StatisticsTikzDrawItemList;

class StatisticsTikzDrawLayer{
public:
    StatisticsTikzDrawLayer(StatisticsType statItem, bool grid)
    {
        p_layerName = draw_ext::sanitizeString(statItem.typeName);
        p_lineType = "blk";

        if(statItem.visualizationType == vectorType)
        {
            if(grid)
            {
                p_layerName += "Blks";
                p_lineType = "vecBlk";
            }
            else
            {
                p_layerName += "Vecs";
                p_lineType = "vec";
            }
        }
        else
        {
            QString colorCalc;
            QString  colorTmp = "\\set%1Color{%2}{%1%2}{\\alpha%1%3}\n";

            for(int i = statItem.colorRange->rangeMin; i <= statItem.colorRange->rangeMax; ++i){
                colorCalc += colorTmp.arg(p_layerName).arg(i).arg(draw_ext::num2str(i));
            }
            p_drawTemplate = "% Calculation and Definition of all used colors/opacities\n";
            p_drawTemplate += colorCalc;
            p_drawTemplate += "\n";
        }
        PgfonLayerTemplate layerTpl(p_layerName, p_lineType);
        p_drawTemplate += layerTpl.render();
    }

    void addElements(StatisticsTikzDrawItem item){p_elementsList.append(item);}
    void addElements(StatisticsTikzDrawItemList list)
    {
        StatisticsTikzDrawItemList::Iterator it;
        for(it = list.begin(); it != list.end(); it++)
        {
            StatisticsTikzDrawItem dItem = *it;
            addElements(dItem);
        }
    }

    QString layer(){return p_layerName;}
    int size(){return p_elementsList.size();}

    QString render(QPoint shift)
    {
        QString drawSection;

        StatisticsTikzDrawItemList::iterator it;
        for(it = p_elementsList.begin(); it != p_elementsList.end(); it++)
        {
            StatisticsTikzDrawItem dtItem = *it;

            dtItem.setToGrid(shift);

            drawSection += dtItem.render();
        }

        return p_drawTemplate.replace(QString("{{drawsection}}"), drawSection);
    }




private:
    QString p_layerName, p_lineType;

    StatisticsTikzDrawItemList p_elementsList;
    QString p_drawTemplate;
};


typedef QList<StatisticsTikzDrawLayer> StatisticsTikzDraw;

#endif // TIKZDRAWTEMPLATES_H
