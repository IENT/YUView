#include <tikzfile.h>
#include <QDate>
#include <iostream>
#include <tikztemplates.h>

#include <QMap>
#include <QStringList>

TikZFile::TikZFile()
{

}

TikZFile::TikZFile(int frameIdx, QString filename,int x, int y, int picWidth, int picHeight, int scaleFactor)
{

    QString imageTpl = "\\node[anchor=south west,inner sep=0] at (image.south west)"
                        "{\\includegraphics[width=\\figWidth pt]{%1}}; % include background image";
    QString imageFileNameTpl  = "%1_%2,%3_%4x%5.png" ;
    p_tikZFileName = filename;
    p_scaleFactor = scaleFactor;
    p_picWidth = picWidth;
    p_picHeight = picHeight;


    p_docFileName = QString("stat__%1_%2,%3_%4x%5_frm%6.tex")
                        .arg(p_tikZFileName )
                        .arg(x/scaleFactor)
                        .arg(y/scaleFactor)
                        .arg(picWidth/scaleFactor)
                        .arg(picHeight/scaleFactor)
                        .arg(frameIdx);

    p_picFileName = QString("pic__%1_%2,%3_%4x%5_frm%6.tex")
                        .arg(p_tikZFileName )
                        .arg(x/scaleFactor)
                        .arg(y/scaleFactor)
                        .arg(picWidth/scaleFactor)
                        .arg(picHeight/scaleFactor)
                        .arg(frameIdx);

    p_imageFileName = imageFileNameTpl
                        .arg(p_tikZFileName )
                        .arg(x/scaleFactor)
                        .arg(y/scaleFactor)
                        .arg(picWidth/scaleFactor)
                        .arg(picHeight/scaleFactor);

    QString p_docTplFileName = ":tpl/output.tex.tpl";
    QString p_picTplFileName = ":tpl/tikzpicture.tex.tpl";



    p_tikZTemplate = readTplFile(p_docTplFileName);
    p_tikZTemplate.replace(QString("{{frame}}"), QString::number(frameIdx));
    p_tikZTemplate.replace(QString("{{date}}"), QDate::currentDate().toString());
    p_tikZTemplate.replace(QString("{{sequence}}"), p_tikZFileName);    
    p_tikZTemplate.replace(QString("{{tikZpictureName}}"), p_picFileName.left(p_picFileName.size()-4));

    p_tikZPicTemplate = readTplFile(p_picTplFileName);
    p_tikZPicTemplate.replace(QString("{{picname}}"), p_picFileName.left(p_picFileName.size()-4));

    p_imageInput = imageTpl.arg(p_imageFileName);
}


QString TikZFile::readTplFile(QString fileName)
{
    QString s;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cout << "Error!!";
        return s;
    }
    QTextStream in(&file);
    s.append(in.readAll());
    file.close();
    return s;
}


void TikZFile::saveTikZ(QString filename, QString data)
{
    QFile file(filename);
    if (file.open(QIODevice::ReadWrite)) {
        QTextStream stream(&file);
        stream << data;
    }
}

void TikZFile::compileTikz(QRect statRect)
{
    int xShift = 0;
    int yShift = 0;

    if(statRect.width())
    {
        p_picWidth = statRect.width();
        p_picHeight = statRect.height();

        xShift = statRect.x();
        yShift = statRect.y();

    }

    QString drawPart;
    StatisticsTikzDraw::Iterator it;

    for(it = p_drawObj.begin(); it != p_drawObj.end(); it++)
    {
        StatisticsTikzDrawLayer dLayer = *it;
        QString layerName = dLayer.layer();

        p_tikZLayerString += layerName + ",";
        p_tikZLayerList += QString("\\pgfdeclarelayer{%1} \n").arg(layerName);

        drawPart += dLayer.render(QPoint(xShift, yShift));
    }

    p_tikZPicTemplate.replace(QString("{{picWidth}}"),QString::number(p_picWidth));
    p_tikZPicTemplate.replace(QString("{{picHeight}}"),QString::number(p_picHeight));
    p_tikZPicTemplate.replace(QString("{{layerList}}"), p_tikZLayerList);
    p_tikZPicTemplate.replace(QString("{{layerString}}"), p_tikZLayerString);
    p_tikZPicTemplate.replace(QString("{{tikzranges}}"), p_tikZColors);
    p_tikZPicTemplate.replace(QString("{{macroscalc}}"), p_tikZMacrosCalc);
    p_tikZPicTemplate.replace(QString("{{tikzpicture}}"), drawPart);

    if(QFile(p_imageFileName).exists())
    {
        p_tikZPicTemplate.replace(QString("{{image}}"), p_imageInput);

        QImage image(p_imageFileName);
        QImage copy ;
        copy = image.copy(statRect);
        copy.save("cropped_to_grid_"+ p_imageFileName);
    }
    else
        p_tikZPicTemplate.replace(QString("{{image}}"), QString(""));

    saveTikZ(p_picFileName, p_tikZPicTemplate);
    //saveTikZ(p_docFileName, p_tikZTemplate);
}


void TikZFile::addLayer(StatisticsType statItem, StatisticsTikzDrawItemList statTikzlist)
{
    QString statType = draw_ext::sanitizeString(statItem.typeName);
    QString layerName;
    QString lineType;
    QMap<QString, int> params;

    StatisticsTikzDrawLayer mainLayer(statItem, 0);
    StatisticsTikzDrawLayer gridlayer(statItem, 1);

    StatisticsTikzDrawItemList::iterator it;

    if(statItem.visualizationType == vectorType)
    {
        params.insert(QString("red"), statItem.vectorColor.red());
        params.insert(QString("blue"), statItem.vectorColor.blue());
        params.insert(QString("green"), statItem.vectorColor.green());
        params.insert(QString("alphaVec"), (float)statItem.vectorColor.alpha()/255.0);
        params.insert(QString("alphaBlk"), (float)statItem.gridColor.alpha()/255.0);

        VectorTemplate colorCalcTpl(params, statType);
        p_tikZColors += colorCalcTpl.render();
    }
    else
    {
        params.insert(QString("minVal"), statItem.colorRange->rangeMin);
        params.insert(QString("maxVal"), statItem.colorRange->rangeMax);

        params.insert(QString("minRed"), statItem.colorRange->minColor.red());
        params.insert(QString("minBlue"), statItem.colorRange->minColor.blue());
        params.insert(QString("minGreen"), statItem.colorRange->minColor.green());
        params.insert(QString("minAlpha"), (float)statItem.colorRange->minColor.alpha()/255.0);

        params.insert(QString("maxRed"), statItem.colorRange->maxColor.red());
        params.insert(QString("maxBlue"), statItem.colorRange->maxColor.blue());
        params.insert(QString("maxGreen"), statItem.colorRange->maxColor.green());
        params.insert(QString("maxAlpha"), (float)statItem.colorRange->maxColor.alpha()/255.0);

        RangeMinMaxTemplate colorCalcTpl(params, statType);
        MacrosCalcTemplate macrosCalcTpl(statType);

        p_tikZColors += colorCalcTpl.render();
        p_tikZMacrosCalc += macrosCalcTpl.render();

        layerName = statType;
    }
    for(it = statTikzlist.begin(); it != statTikzlist.end(); it++)
    {
        StatisticsTikzDrawItem dtItem = *it;

        if(dtItem.getDrawType() == grid)
            gridlayer.addElements(dtItem);
        if(dtItem.getDrawType() == vector || dtItem.getDrawType() == block)
            mainLayer.addElements(dtItem);
    }

    PgfonLayerTemplate pgfonlayer(layerName, lineType);

    if(gridlayer.size())
        p_drawObj.append(gridlayer);
    p_drawObj.append(mainLayer);

    p_drawLayer += pgfonlayer.render();
}
