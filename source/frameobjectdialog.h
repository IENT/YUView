#ifndef FRAMEOBJECTDIALOG_H
#define FRAMEOBJECTDIALOG_H

#include <QDialog>
#include <QFontDialog>
#include "textobject.h"

namespace Ui {
class FrameObjectDialog;
}

class FrameObjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FrameObjectDialog(QWidget *parent = 0);
    ~FrameObjectDialog();
    void loadItemStettings(TextObject* item);
    QFont getFont() {return currentFont;};
    double getDuration() {return currentDuration;};
    QString getText() {return currentText;}
public slots:
    void editFont();
    void saveState();
private:
    Ui::FrameObjectDialog *ui;
    QFont currentFont;
    QString currentText;
    double currentDuration;
};

#endif // FRAMEOBJECTDIALOG_H
