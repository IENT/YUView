#include "textobject.h"

TextObject::TextObject(QString displayString,QFont font, double duration, QObject* parent) : DisplayObject(parent)
{
    p_TextString = displayString;
    p_TextFont = font;
    setDuration((float)duration);
    //TODO: use fontmetric to measure size of QImage needed
    p_displayImage = QImage(640,480,QImage::Format_ARGB32);
    drawText(0);
}

TextObject::~TextObject()
{

}

void TextObject::loadImage(unsigned int idx)
{
    drawText(idx);
}

void TextObject::drawText(unsigned int idx)
{
    QPainter currentPainter(&p_displayImage);
    currentPainter.setRenderHint(QPainter::TextAntialiasing);
    currentPainter.setRenderHint(QPainter::Antialiasing);
    //currentPainter.fillRect(p_displayImage.rect(),Qt::black);
    p_displayImage.fill(qRgba(0, 0, 0, 0));
    currentPainter.setPen(Qt::white);
    currentPainter.setFont(p_TextFont);
    // showing the current idx just for testing purposes, replace by p_TextString
    //QString testString = p_TextString + " " + QString::number(idx);
    currentPainter.drawText(p_displayImage.rect(), Qt::AlignCenter, p_TextString);

}
