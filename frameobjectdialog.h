#ifndef FRAMEOBJECTDIALOG_H
#define FRAMEOBJECTDIALOG_H

#include <QDialog>

namespace Ui {
class FrameObjectDialog;
}

class FrameObjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FrameObjectDialog(QWidget *parent = 0);
    ~FrameObjectDialog();

private:
    Ui::FrameObjectDialog *ui;
};

#endif // FRAMEOBJECTDIALOG_H
