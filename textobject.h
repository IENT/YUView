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

#ifndef TEXTOBJECT_H
#define TEXTOBJECT_H

#include <QString>
#include <QImage>
#include <QPainter>
#include <QFontMetrics>
#include <QColor>
#include "displayobject.h"

class TextObject : public DisplayObject
{
public:
    TextObject(QString displayString, QObject* parent=0);
    ~TextObject();

    void loadImage(unsigned int idx);
    QColor getPixelValue(int x, int y) { return QColor(); }

    void setDuration(int durationSeconds)
    {
        setFrameRate(1.0);
        setNumFrames(durationSeconds);
    }
    int duration() { return p_numFrames; }

    void setText(QString text) {p_TextString=text;}
    void setFont(QFont font) {p_TextFont=font;}
    void setColor(QColor color) {p_TextColor=color;}

    QString text() {return p_TextString;}
    QFont font() {return p_TextFont;}
    QColor color() {return p_TextColor;}

public slots:

    void refreshDisplayImage() {return;}
private:

    void drawText();

    QString p_TextString;
    QFont p_TextFont;
    QColor p_TextColor;

};

#endif // TEXTOBJECT_H
