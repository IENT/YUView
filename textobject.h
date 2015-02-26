#ifndef TEXTOBJECT_H
#define TEXTOBJECT_H

#include <QString>
#include <QImage>
#include <QPainter>
#include <QFontMetrics>
#include "displayobject.h"

class TextObject : public DisplayObject
{
private:

    // TODO: implement setter function for these member variables
public:
    TextObject(QString displayString,QFont font, double duration, QObject* parent=0);
    ~TextObject();
    void loadImage(unsigned int idx);
    void drawText(unsigned int idx);
    int getPixelValue(int x, int y) { return -1; }

    void setDuration(float durationSeconds)
    {
        setFrameRate(1.0/durationSeconds);
        setNumFrames(1);
    }
    void setText(QString text) {p_TextString=text;};
    void setFont(QFont font) {p_TextFont=font;}
    QString getText() {return p_TextString;};
    QFont getFont() {return p_TextFont;};

public slots:

    void refreshDisplayImage() {return;}
private:
    QString p_TextString;
    QFont p_TextFont;

};

#endif // TEXTOBJECT_H
