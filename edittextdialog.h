#ifndef EditTextDialog_H
#define EditTextDialog_H

#include <QDialog>
#include <QFontDialog>
#include <QColor>
#include <QColorDialog>
#include "playlistitemtext.h"

namespace Ui {
class EditTextDialog;
}

class EditTextDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditTextDialog(QWidget *parent = 0);
    ~EditTextDialog();
    void loadItemStettings(PlaylistItemText* item);
    QFont getFont() {return currentFont;}
    double getDuration() {return currentDuration;}
    QString getText() {return currentText;}
    QColor getColor() {return currentColor;}
public slots:
    void editFont();
    void saveState();
private slots:
    void on_editColor_clicked();

    void on_textEdit_textChanged();

    void on_doubleSpinBox_valueChanged(double arg1);

private:
    Ui::EditTextDialog *ui;
    PlaylistItemText* currentItem;
    QFont currentFont;
    QString currentText;
    QColor currentColor;
    double currentDuration;
};

#endif // EditTextDialog_H
