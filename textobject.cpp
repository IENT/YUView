#include "textobject.h"

TextObject::TextObject(QString displayString,QObject* parent):DisplayObject(parent)
{
    p_TextString = displayString;
    p_displayImage = QImage(640,480,QImage::Format_ARGB32);
    drawText(0);
    // TO-DO: get these parameters from a user dialog
    p_frameRate = 25;
    p_DurationInS = 3.0;
    p_numFrames = (int)p_frameRate*p_DurationInS;
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
    currentPainter.fillRect(p_displayImage.rect(),Qt::black);
    p_displayImage.fill(qRgba(0, 0, 0, 0));
    //currentPainter.setBrush(Qt::white);
    //currentPainter.setFont(QFont::Helvetica);
    currentPainter.setPen(Qt::white);
    currentPainter.setFont(QFont("Helvectica",20));
    // showing the current idx just for testing purposes, replace by p_TextString
    QString testString = p_TextString + " " + QString::number(idx);
    currentPainter.drawText(p_displayImage.rect(), Qt::AlignCenter, testString);

}
