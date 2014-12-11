#ifndef YUVLISTITEMMULTI_H
#define YUVLISTITEMMULTI_H

#include "yuvlistitem.h"
#include "yuvlistitemvid.h"
#include <QDomDocument>

struct lazyItem;

class YUVListItemMulti : public YUVListItem
{
public:
    YUVListItemMulti( const QString &xmlFileName, QTreeWidget* parent = NULL );
    YUVListItemMulti( const QString &xmlFileName, YUVListItem* parentItem = NULL );
    YUVListItemMulti( const QStringList &srcFileNames, QTreeWidget* parent = NULL );
    YUVListItemMulti( const QStringList &srcFileNames, YUVListItem* parentItem = NULL );

    YUVObject* renderObject(int idx = 0);

    YUVListItemType itemType();
    virtual StatisticsParser* getStatisticsParser() { return 0; }
    virtual bool statisticsSupported() { return false; } // no stats for multiviewitems, because of interference with view synthesis

    void switchLR();
    int getNumViews();
    double getEyeDistance();
    void setEyeDistance(double dist);

private:
    QVector<YUVListItemVid*> p_multiviewItems;
    int parseFile(QDomDocument &dom);
    void parseLazyItems(QVector<lazyItem> &items);
    void parseCamera(CameraParameter& cam, QDomElement &e);
    double p_eye_distance;
};

#endif // YUVLISTITEMMULTI_H
