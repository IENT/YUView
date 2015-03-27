#ifndef SEPARATEWINDOW_H
#define SEPARATEWINDOW_H

#include <QWidget>
#include <QDockWidget>
#include <QString>
#include <QMainWindow>

class MainWindow;

namespace Ui {
class separateWindow;
}

class separateWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit separateWindow(QMainWindow *parent = 0);
    void moveWidget(QDockWidget* tmp_Widget);
    void WidgetGetBack(MainWindow *tmp, Qt::DockWidgetArea position);
    void reset();
    void setTitle(QString title) {this->setWindowTitle(title);}

    ~separateWindow();

private:
    Ui::separateWindow *ui;

protected:
    QString p_name;
    int p_height;
    int p_width;
    void p_setsize();
    QList<QString> adoptedWidgets;
    MainWindow* p_mother;


};

#endif // SEPARATEWINDOW_H

