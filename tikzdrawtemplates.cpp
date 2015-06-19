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
