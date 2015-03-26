#include "separatewindow.h"
#include "ui_separatewindow.h"


separateWindow::separateWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::separateWindow)
{

    p_width = 50;
    p_height = 0;
    adoptedWidgets.clear();
    ui->setupUi(this);
}

separateWindow::~separateWindow()
{
    delete ui;
}

void separateWindow::moveWidget(QDockWidget* tmp_Widget){

        tmp_Widget->setParent(this);
        tmp_Widget->move(0,p_height);
        tmp_Widget->show();
        p_height+=tmp_Widget->height();
        if(tmp_Widget->width()>p_width) p_width = tmp_Widget->width();
        p_setsize();
        adoptedWidgets.append(tmp_Widget->objectName());
}

void separateWindow::p_setsize(){
    this->setFixedHeight(p_height);
    this->setFixedSize(p_width,p_height);
}

void separateWindow::WidgetGetBack(QMainWindow *tmp){
    QList<QWidget*> childrenlist;
    QList<QString>::Iterator it;

    for (it =  adoptedWidgets.begin(); it != adoptedWidgets.end(); it++){
        childrenlist.append(this->findChild<QWidget*>(*it));
    }

    int length = childrenlist.length();
    for(int i=0; i<length; ++i){
        childrenlist.at(i)->setParent((QWidget*)tmp);
    }
}
