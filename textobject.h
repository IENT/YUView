#ifndef TEXTOBJECT_H
#define TEXTOBJECT_H

#include <QString>
#include <QImage>
#include <QPainter>
#include "displayobject.h"

class TextObject : public DisplayObject
{
private:
    QString p_TextString;
    QFont p_TextFont;
    QColor p_TextFontColor;
    // TODO: implement setter function for these member variables
public:
    TextObject(QString displayString, QObject* parent=0);
    ~TextObject();
    void loadImage(unsigned int idx);
    void drawText(unsigned int idx);
    int getPixelValue(int x, int y) { return -1; }

    void setDuration(float durationSeconds)
    {
        setFrameRate(1.0/durationSeconds);
        setNumFrames(1);
    }

public slots:

    void refreshDisplayImage() {return;}

};

#endif // TEXTOBJECT_H
