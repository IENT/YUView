#ifndef TIKZTEMPLATES_H
#define TIKZTEMPLATES_H


#include <QString>
#include <QStringList>
#include <QMap>

class RangeMinMaxTemplate {

public:
    RangeMinMaxTemplate();
    RangeMinMaxTemplate(QMap<QString, int> inputParams, QString statType){

        p_template = "%% %1\n"
                "\\newcommand{\\%1FillOpacity}{1} %% Sets type-specific opacity of the blocks (1->opaque,0->transparent)\n"
                "\\tikzstyle{%1Pattern}=[postaction={pattern=none,pattern color=#1}] %% Sets pattern for blocks of this type (e.g. 'north west lines')\n"
                "\\FPset\\%1MinVal{%minVal}\n"
                "\\FPset\\%1MaxVal{%maxVal}\n"
                "\\FPset\\%1MinRed{%minRed}\n"
                "\\FPset\\%1MaxRed{%maxRed}\n"
                "\\FPset\\%1MinGreen{%minGreen}\n"
                "\\FPset\\%1MaxGreen{%maxGreen}\n"
                "\\FPset\\%1MinBlue{%minBlue}\n"
                "\\FPset\\%1MaxBlue{%maxBlue}\n"
                "\\FPset\\%1MinAlpha{%minAlpha}\n"
                "\\FPset\\%1MaxAlpha{%maxAlpha}\n\n";

        p_template = p_template.arg(statType);

        QMapIterator<QString, int> it(inputParams);

        while (it.hasNext()) {
            it.next();
            p_template.replace(QString("%")+it.key(), QString::number(it.value()));

        }
    }
    QString render(){return p_template;}
private:
    QString p_template;

};

class MacrosCalcTemplate{
public:
    MacrosCalcTemplate();
    MacrosCalcTemplate(QString statType){
        p_template = "%% %1\n"
                "\\newcommand{\\set%1Color}[3]{\n"
                "  \\FPeval\\red{\\%1MinRed+(#1-%1MinVal)*(\\%1MaxRed-\\%1MinRed)/(\\%1MaxVal-\\%1MinVal)}\n"
                "  \\FPeval\\green{\\%1MinGreen+(#1-%1MinVal)*(\\%1MaxGreen-\\%1MinGreen)/(\\%1MaxVal-\\%1MinVal)}\n"
                "  \\FPeval\\blue{\\%1MinBlue+(#1-%1MinVal)*(\\%1MaxBlue-\\%1MinBlue)/(\\%1MaxVal-\\%1MinVal)}\n"
                "  \\FPeval\\alpha{\\%1FillOpacity*(\\%1MinAlpha+#1*(\\%1MaxAlpha-\\%1MinAlpha)/(\\%1MaxVal-\\%1MinVal))}\n"
                "  \\definecolor{#2}{RGB}{\\red,\\green,\\blue}\n"
                "  \\FPset#3{\\alpha} }\n\n";

        p_template = p_template.arg(statType);
    }

    QString render(){return p_template;}
private:
    QString p_template;
};

class PgfonLayerTemplate{
public:
    PgfonLayerTemplate();
    PgfonLayerTemplate(QString layerName, QString lineType){
        p_template = "\n\\begin{pgfonlayer}{%1}\n"
                "\\clip (\\xStartClip pt,\\yStartClip pt) rectangle (\\xEndClip pt,\\yEndClip pt);\n"
                "\\begin{scope}[%2Line,gridOpacity=1,shift={(image.north west)},yscale=-1,scale=\\scaleFac]\n"
                "{{drawsection}}"
                "\\end{scope}\n"
                "\\end{pgfonlayer}\n";

        p_template = p_template.arg(layerName).arg(lineType);

        if(lineType == "vecBlk")
            p_template = QString("% Draw vector-blocks\n\\ifthenelse{\\boolean{bDrawVectorBlock}}{\n%1}\n").arg(p_template);
    }

    QString render(){return p_template;}
private:
    QString p_template;
};

class VectorTemplate{
public:
    VectorTemplate();
    VectorTemplate(QMap<QString, int> inputParams, QString statType){
        p_template = "% %1\n"
                "\\newcommand{\\%1VecOpacity}{%alphaVec} % Sets type-specific opacity of the vectors (1->opaque,0->transparent)\n"
                "\\newcommand{\\%1BlkOpacity}{%alphaBlk} % Sets type-specific opacity of the blocks around the vectors (1->opaque,0->transparent)\n"
                "\\definecolor{%1}{RGB}{%red,%green,%blue} % Color for vectors of type Vector with format (Red,Green,Blue), each between 0 and 255\n";
        p_template = p_template.arg(statType);

        QMapIterator<QString, int> it(inputParams);

        while (it.hasNext()) {
            it.next();
            p_template.replace(QString("%")+it.key(), QString::number(it.value()));

        }
    }
    QString render(){return p_template;}
private:
    QString p_template;
};

/*
class ColorCalcTemplate{
public:
    ColorCalcTemplate();
    ColorCalcTemplate(QMap<QString, int> inputParams, QString statType, int typeID){

    }
};
*/
#endif // TIKZTEMPLATES_H
