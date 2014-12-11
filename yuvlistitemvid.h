#ifndef YUVLISTITEMVID_H
#define YUVLISTITEMVID_H

#include <QTreeWidgetItem>

#include "yuvlistitem.h"
#include "yuvobject.h"

class YUVListItemVid : public YUVListItem
{
public:
    YUVListItemVid(const QString &srcFileName, QTreeWidget* parent = NULL);
    YUVListItemVid(const QString &srcFileName, YUVListItem* parentItem = NULL);

    ~YUVListItemVid();

    YUVObject* renderObject(int idx = 0);

    YUVListItemType itemType();

private:
    YUVObject *p_renderObject;
};

#endif // YUVLISTITEMVID_H
