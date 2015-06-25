#ifndef TIKZFILE_H
#define TIKZFILE_H

#include <QVector>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QColor>
#include <QPainter>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QFuture>
#include <QtConcurrent>
#include <QStringList>

#include <statisticsextensions.h>
#include <tikzextensions.h>
#include <tikzdrawtemplates.h>

//#include <statisticsobject.h>



class TikZFile
{
public:
    TikZFile();
    TikZFile(int frameIdx, QString fileName, int x, int y, int picWidth, int picHeight, int scaleFactor);

    void compileTikz();
    void addLayer(StatisticsType statItem, StatisticsTikzDrawItemList statTikzlist);
    void setSettings(TikzDrawSettings settings){p_settings = settings;}
    void setStatRect(QRect statRect){p_statRect = statRect;}
    
private:
    QString readTplFile(QString filename);
    void saveTikZ(QString filaname, QString data);
    void addColorCalculation(TikzDrawLayerSettings settings, drawtype_t drawType, QString statType, int minVal, int maxVal);

    int p_scaleFactor;
    int p_picWidth, p_picHeight;

    QString p_tikZTemplate;
    QString p_tikZPicTemplate;

    QString p_tikZFileName;
    QString p_imageFileName;

    QString p_tikZLayerList;
    QString p_tikZLayerString;
    QString p_tikZColors;
    QString p_tikZMacrosCalc;
    QString p_tikzColorCalc;
    QStringList p_tikzColorCalcList;

    QString p_draw;
    QString p_drawLayer;
    QString p_blkSection;
    QString p_drawSection;
    QString p_imageInput;

    QString p_docFileName;
    QString p_picFileName;

    QRect p_statRect;

    StatisticsTikzDraw p_drawObj;
    TikzDrawSettings p_settings;

};

#endif // TIKZFILE_H
