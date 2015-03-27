#include "separatewindow.h"
#include "ui_separatewindow.h"
#include "mainwindow.h"


separateWindow::separateWindow(QMainWindow *parent) :
    QMainWindow(parent),
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
        tmp_Widget->show();
        p_height+=tmp_Widget->height();
        if(tmp_Widget->width()>p_width) p_width = tmp_Widget->width();
        p_setsize();
        this->addDockWidget(Qt::LeftDockWidgetArea, tmp_Widget);
        adoptedWidgets.append(tmp_Widget->objectName());
}

void separateWindow::p_setsize(){
    this->setFixedHeight(p_height);
    this->setFixedSize(p_width,p_height);
}


void separateWindow::WidgetGetBack(MainWindow *tmp, Qt::DockWidgetArea position){
    QList<QWidget*> childrenlist;
    QList<QString>::Iterator it;


    for (it =  adoptedWidgets.begin(); it != adoptedWidgets.end(); it++){
        childrenlist.append(this->findChild<QWidget*>(*it));
    }
    int length = childrenlist.length();
    for(int i=0; i<length; ++i){
        childrenlist.at(i)->setParent((QWidget*)tmp);
        childrenlist.at(i)->show();
        tmp->addDockWidget(position,(QDockWidget*)childrenlist.at(i));
    }
}

void separateWindow::reset(){
    this->setFixedHeight(50);
    this->setFixedWidth(50);
    p_height=0;
    p_width=50;
    this->hide();
}

