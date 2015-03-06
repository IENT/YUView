#include "textobject.h"

TextObject::TextObject(QString displayString, QObject* parent) : DisplayObject(parent)
{
    p_TextString = displayString;
    p_TextFont = QFont("Arial", 12);
    p_TextColor = QColor(Qt::white);
    setDuration(5);
}

TextObject::~TextObject()
{

}

void TextObject::loadImage(unsigned int idx)
{
    drawText();
}

void TextObject::drawText()
{
    // estimate size of text to be rendered
    QFontMetrics fm(p_TextFont);
    int width = fm.width(p_TextString);
    int height = fm.height()*(p_TextString.count("\n")+1);

    // create new display image
    QImage tmpImage(width,height,QImage::Format_ARGB32);
    tmpImage.fill(qRgba(0, 0, 0, 0));   // clear with transparent color
    p_displayImage.convertFromImage(tmpImage);

    // draw text
    QPainter currentPainter(&p_displayImage);
    currentPainter.setRenderHint(QPainter::TextAntialiasing);
    currentPainter.setRenderHint(QPainter::Antialiasing);
    currentPainter.setPen(p_TextColor);
    currentPainter.setFont(p_TextFont);
    currentPainter.drawText(p_displayImage.rect(), Qt::AlignCenter, p_TextString);

}
