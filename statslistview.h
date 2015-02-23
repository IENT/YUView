#ifndef STATSLISTVIEW_H
#define STATSLISTVIEW_H

#include <QListView>

class StatsListView : public QListView
{
    Q_OBJECT
public:
    explicit StatsListView(QWidget *parent = 0);

    void dragEnterEvent(QDragEnterEvent *event);
    //void dropEvent(QDropEvent *event);
    
signals:
    
public slots:
    
};

#endif // STATSLISTVIEW_H
