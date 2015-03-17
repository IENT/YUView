/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

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
