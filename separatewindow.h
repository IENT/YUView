#ifndef SEPARATEWINDOW_H
#define SEPARATEWINDOW_H

#include <QWidget>
#include <QDockWidget>
#include <QString>
//#include "yuviewapp.h"
//#include "mainwindow.h"

namespace Ui {
class separateWindow;
}

class separateWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit separateWindow(QWidget *parent = 0);
    void moveWidget(QDockWidget* tmp_Widget);
    void WidgetGetBack(QMainWindow *tmp);

    ~separateWindow();
    
private:
    Ui::separateWindow *ui;

protected:
    QString p_name;
    int p_height;
    int p_width;
    void p_setsize();
    QList<QString> adoptedWidgets;


};

#endif // SEPARATEWINDOW_H
