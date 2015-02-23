#include "frameobjectdialog.h"
#include "ui_frameobjectdialog.h"

FrameObjectDialog::FrameObjectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FrameObjectDialog)
{
    ui->setupUi(this);
}

FrameObjectDialog::~FrameObjectDialog()
{
    delete ui;
}
