#include "yuvlistitemmulti.h"
#include <QDomDocument>
#include <QDir>
#include "yuvlistitemvid.h"

struct lazyItem {
    QString filename;
    bool isDepth;
    bool processed;
    CameraParameter* camera;
    QString statsFilename;
    lazyItem():camera(0){};
};

YUVListItemMulti::YUVListItemMulti(const QString &xmlFileName, QTreeWidget* parent ) : YUVListItem ( xmlFileName, parent ) {
    QDomDocument dom;
    if (!dom.isNull())
        return;
    QFile file( xmlFileName );
    QFileInfo fileInfo(file);
    QDir::setCurrent( fileInfo.absoluteDir().path() );
    if( !file.open( QIODevice::ReadOnly ) )
      return;
    if( !dom.setContent( &file ) )
    {
      file.close();
      return;
    }
    file.close();
    parseFile(dom);
}

YUVListItemMulti::YUVListItemMulti(const QString &xmlFileName, YUVListItem* parentItem ) : YUVListItem ( xmlFileName, parentItem ) {
    QDomDocument dom;
    if (!dom.isNull())
        return;
    QFile file( xmlFileName );
    QFileInfo fileInfo(file);
    QDir::setCurrent( fileInfo.absoluteDir().path() );
    if( !file.open( QIODevice::ReadOnly ) )
      return;
    if( !dom.setContent( &file ) )
    {
      file.close();
      return;
    }
    file.close();
    parseFile(dom);
}

int YUVListItemMulti::parseFile(QDomDocument &dom) {
    QRegExp isDepthRX("/depth_");
    QRegExp viewRX("([0-9]+)_([0-9]+)x([0-9]+)_([0-9]+)");
    CameraParameter defaultCamera;

    QVector<lazyItem> items;

    if (dom.isNull())
        return -1;
    QDomElement root = dom.documentElement();
    if( root.tagName() != "XMLFile" )
        return -2;

    QDomNode n = root.firstChild();
    while( !n.isNull() )
    {
        QDomElement e = n.toElement();
        if( !e.isNull() )
        {
            if (e.tagName() == "CameraParameters") {
                parseCamera(defaultCamera, e);
            } else
            if( e.tagName() == "View" )
            {
                // new item for video file
                lazyItem itVideo;
                CameraParameter* camVideo = new CameraParameter(defaultCamera), *camDepth = new CameraParameter;

                // Do RegEx evaluations from filename
                if (viewRX.indexIn(e.attribute( "videofile", QString())) > -1)
                    camVideo->viewNumber = viewRX.cap(1).toInt();

                itVideo.filename = e.attribute( "videofile", QString());
                itVideo.statsFilename = e.attribute( "videostatsfile", QString());
                itVideo.isDepth = false;

                // now parse parameters from XML
                parseCamera(*camVideo, e);

                itVideo.processed = false;
                itVideo.camera = camVideo;
                items.append(itVideo);

                lazyItem itDepth;
                itDepth.filename = e.attribute( "depthfile", QString());
                itVideo.statsFilename = e.attribute( "depthstatsfile", QString());
                itDepth.isDepth = true;
                itDepth.processed = false;
                camDepth->viewNumber = camVideo->viewNumber;
                itDepth.camera = camDepth;
                items.append(itDepth);
            }
        }

        n = n.nextSibling();
    }

    parseLazyItems(items);


    // update item name to short name
    QString multiName = p_multiviewItems[0]->text(0) + " (Multi)";
    setText(0, multiName);

    // update icon
    setIcon(0, QIcon(":images/img_multi.png"));

    // make sure drag&drop is not possible for multi videos
    //setFlags(flags() & ~Qt::ItemIsDropEnabled);
    return 1;
}


YUVListItemMulti::YUVListItemMulti(const QStringList &srcFileNames, QTreeWidget* parent ) : YUVListItem ( srcFileNames[0], parent )
{
    QRegExp isDepthRX("/depth_");
    QRegExp viewRX("([0-9]+)_([0-9]+)x([0-9]+)_([0-9]+)");

    QVector<lazyItem> items;

    // match all filenames before adding them in right order
    for (int i = 0; i<srcFileNames.length(); i++) {
        lazyItem it;
        CameraParameter cam;
        it.camera = &cam;

        // Do RegEx evaluations
        it.isDepth = (isDepthRX.indexIn(srcFileNames[i])>-1);
        if (viewRX.indexIn(srcFileNames[i]) > -1)
            it.camera->viewNumber = viewRX.cap(1).toInt();
        it.filename = srcFileNames[i];
        it.processed = false;
        items.append(it);
    }

    parseLazyItems(items);

    // update item name to short name
    QString multiName = p_multiviewItems[0]->text(0) + " (Multi)";
    setText(0, multiName);

    // update icon
    setIcon(0, QIcon(":images/img_multi.png"));

    // make sure drag&drop is possible for multiview items
    setFlags(flags() | Qt::ItemIsDropEnabled);
}

YUVListItemMulti::YUVListItemMulti(const QStringList &srcFileNames, YUVListItem* parentItem ) : YUVListItem ( srcFileNames[0], parentItem )
{
    // create new object for this video file
    foreach (const QString &srcFileName, srcFileNames) {
        // create Video objects
        YUVListItemVid *leftVidItem = new YUVListItemVid(srcFileName, this);
        leftVidItem->setIcon(0, QIcon(":images/img_L.png"));
        //leftVidItem->setFlags(leftVidItem->flags() & ~Qt::ItemIsDragEnabled);
        p_multiviewItems.append(leftVidItem);
    }

    // update item name to short name
    QString multiName = /*leftVidItem->text(0) +*/ " (Multi)";
    setText(0, multiName);

    // update icon
    setIcon(0, QIcon(":images/img_multi.png"));

    // make sure drag&drop is not possible for multi videos
    //setFlags(flags() & ~Qt::ItemIsDropEnabled);
}

void YUVListItemMulti::parseLazyItems(QVector<lazyItem> &items) {
    // iterate over all items
    for (int i=0; i<items.size(); i++) {
        int rgbID=-1, depthID=-1, view=INT_MAX; // state vars

        // iterate over all items
        // CAVE: totally inefficient search method...
        for (int j=0; j<items.size(); j++) {
            if ((items[j].processed) || (items[j].camera->viewNumber > view))
                continue;// find the items with highest view property that has not been processed before
            if (items[j].isDepth) depthID = j;
            else {
                rgbID = j;
                view = items[j].camera->viewNumber;
            }
        }
        if (rgbID > -1) {
            YUVListItemVid *newVidItem = new YUVListItemVid(items[rgbID].filename, this);
            //newVidItem->setFlags(newVidItem->flags() & ~Qt::ItemIsDragEnabled);
            //newVidItem->setIcon(0, QIcon(":images/img_L.png"));

            newVidItem->renderObject()->setCameraParameter(*items[rgbID].camera);

            // set stats
            if (!items[rgbID].statsFilename.isEmpty()) {
                StatisticsParser *parser = new StatisticsParser;
                parser->parseFile(items[rgbID].statsFilename.toStdString());
                newVidItem->setStatisticsParser(parser);
            }

            p_multiviewItems.append(newVidItem);
            items[rgbID].processed = true;
            if (depthID > -1) {
                YUVListItemVid *depthVidItem = new YUVListItemVid(items[depthID].filename, newVidItem);
                depthVidItem->setFlags(depthVidItem->flags() & ~Qt::ItemIsDragEnabled);

                // set stats
                if (!items[depthID].statsFilename.isEmpty()) {
                    StatisticsParser *parser = new StatisticsParser;
                    parser->parseFile(items[depthID].statsFilename.toStdString());
                    newVidItem->setStatisticsParser(parser);
                }

                //newVidItem->setIcon(0, QIcon(":images/img_L.png"));
                p_multiviewItems.append(depthVidItem);
                items[depthID].processed = true;
            }
        }
    }
}

void YUVListItemMulti::parseCamera(CameraParameter& cam, QDomElement &e) {
    // camera/view parameters
    bool ok=true;
    float f;
    int i;
    f = e.attribute( "fx", "not found").toFloat(&ok);
    if (ok) cam.fx = f;

    f = e.attribute( "fy", "not found").toFloat(&ok);
    if (ok) cam.fy = f;

    f = e.attribute( "z_near", "not found").toFloat(&ok);
    if (ok) cam.Z_near = f;

    f = e.attribute( "z_far", "not found").toFloat(&ok);
    if (ok) cam.Z_far = f;

    i = e.attribute("width", "not found").toInt(&ok);
    if (ok) cam.width = i;

    i = e.attribute("height", "not found").toInt(&ok);
    if (ok) cam.height = i;

    i = e.attribute("view_number", "not found").toInt(&ok);
    if (ok) cam.viewNumber = i;

    f = e.attribute( "default_eye_distance", "not found").toFloat(&ok);
    if (ok) p_eye_distance = f;

    //position
    QString str = e.attribute("position", "0,0,0");
    QStringList lst = str.split(",");
    for (int i=0; i<3; i++)
        cam.position[i] = lst[i].toFloat();

    //rotation
    str = e.attribute("rotation", "0,0,0,0,0,0,0,0,0");
    lst = str.split(",");
    for (int i=0; i<9; i++)
        cam.rotation[i] = lst[i].toFloat();

    //TODO: default_eye_distance
}

YUVObject* YUVListItemMulti::renderObject(int idx)
{
    if(idx > getNumViews()-1)
        return NULL;
    else
    {
        YUVListItemVid* vidItem = (YUVListItemVid*)child(idx);
        return vidItem->renderObject();
    }
}

YUVListItemType YUVListItemMulti::itemType()
{
    return MultiviewItem;
}

int YUVListItemMulti::getNumViews() {
    return childCount();
}

void YUVListItemMulti::switchLR()
{
    // reorder child items
    YUVListItemVid* oldLeft = (YUVListItemVid*)takeChild(0);
    YUVListItemVid* oldRight = (YUVListItemVid*)takeChild(0);

    oldRight->setIcon(0, QIcon(":images/img_L.png"));
    oldLeft->setIcon(0, QIcon(":images/img_R.png"));

    addChild(oldRight);
    addChild(oldLeft);
}


double YUVListItemMulti::getEyeDistance() {
    return p_eye_distance;
}

void YUVListItemMulti::setEyeDistance(double dist) {
    p_eye_distance = dist;
}
