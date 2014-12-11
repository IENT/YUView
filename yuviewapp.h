#ifndef YUVIEWAPP_H
#define YUVIEWAPP_H

#include <QApplication>

#include "mainwindow.h"

class YUViewApp : public QApplication
{
    Q_OBJECT

public:
    YUViewApp(int & argc, char **argv);

protected:
    bool event(QEvent *);

private:

    MainWindow *w;

signals:

public slots:

};

#endif // YUVIEWAPP_H
