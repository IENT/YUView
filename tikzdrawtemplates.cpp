#include <tikzdrawtemplates.h>


QString draw_ext::num2str(int num)
{
    if (num < 10 && num>=0){
        QStringList nums;
        nums<<"Zero"<<"One"<<"Two"<<"Three"<<"Four";
        nums<<"Five"<<"Six"<<"Seven"<<"Eight"<<"Nine";
        return nums[num];
    } else {
        return "Void";
    }
}

QString draw_ext::sanitizeString(QString input){
    QStringList inputList= input.split("");
    for(int i=0; i<inputList.length(); ++i){
        bool isNum;
        int value = inputList[i].toInt(&isNum);
        if (isNum)
            inputList[i] = draw_ext::num2str(value);
    }
    return inputList.join("");
}


drawBlock::drawBlock(QPoint start, QPoint stop, QString typeName, int value)
{
    p_typeName = typeName;
    QString colorName = p_typeName + QString::number(value);
    QString alphaName = p_typeName + draw_ext::num2str(value);

    p_template = "\\filldraw[fillStyle=%6,fillOpacity=\\alpha%7*\\%5FillOpacity,%5Pattern=%6] (%1 pt,%2 pt) rectangle (%3 pt,%4 pt);\n";
    p_template = p_template
                    .arg(start.x())
                    .arg(start.y())
                    .arg(stop.x())
                    .arg(stop.y())
                    .arg(typeName)
                    .arg(colorName)
                    .arg(alphaName);
}


drawVector::drawVector(QPoint start, QPoint stop, QString typeName)
{
    p_template = "\\filldraw[color=%1] (%2 pt,%3 pt) -- (%4 pt,%5 pt);\n";
    p_template = p_template
                    .arg(typeName)
                    .arg(start.x())
                    .arg(start.y())
                    .arg(stop.x())
                    .arg(stop.y());
}


drawGrid::drawGrid(QPoint start, QPoint stop, QString typeName)
{
    p_template = "\\draw[color = %5] (%1 pt,%2 pt) rectangle (%3 pt,%4 pt);\n";
    p_template = p_template
                    .arg(start.x())
                    .arg(start.y())
                    .arg(stop.x())
                    .arg(stop.y())
                    .arg(typeName);
}


StatisticsTikzDrawItem::StatisticsTikzDrawItem(QString statName)
{
    statTypeName = statName;
    toGrid = 0;
    shiftX = 0;
    shiftY = 0;
    p_value = -1;
}

QString StatisticsTikzDrawItem::render()
{
    if(toGrid)
        applyShift();

    /*Create factory*/
     DrawFactory *drawFactory = new DrawFactory();

     drawElement *el = drawFactory->getElement(p_drawType, startPoint, stopPoint, statTypeName, p_value);

    return el->render();
}


void StatisticsTikzDrawItem::applyShift()
{
    startPoint.setX(startPoint.x() - shiftX);
    startPoint.setY(startPoint.y() - shiftY);

    stopPoint.setX(stopPoint.x() - shiftX);
    stopPoint.setY(stopPoint.y() - shiftY);
}

void StatisticsTikzDrawItem::setStartStop(QPoint start, QPoint stop)
{
    startPoint = start;
    stopPoint = stop;
}



void StatisticsTikzDrawItem::setToGrid(QPoint shift)
{
    toGrid = 1;
    shiftX = shift.x();
    shiftY = shift.y();

}


StatisticsTikzDrawLayer::StatisticsTikzDrawLayer(StatisticsType statItem, bool grid)
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


void StatisticsTikzDrawLayer::addElements(StatisticsTikzDrawItemList list)
{
    StatisticsTikzDrawItemList::Iterator it;
    for(it = list.begin(); it != list.end(); it++)
    {
        StatisticsTikzDrawItem dItem = *it;
        addElements(dItem);
    }
}



QString StatisticsTikzDrawLayer::render(QPoint shift)
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
