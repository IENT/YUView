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
#include <QPainter>

TextObject::TextObject(QString displayString, QObject* parent) : DisplayObject(parent)
{
    p_TextString = displayString;
    setDuration(5);
    refreshTextSize();
}

TextObject::~TextObject()
{

}

void TextObject::loadImage(int frameIdx)
{
    if (frameIdx==INT_INVALID || p_TextString.isEmpty() || frameIdx >= numFrames())
    {
        p_displayImage = QPixmap();
        return;
    }
    // else
    drawText();
}

void TextObject::drawText()
{    
    // create new display image
    QImage tmpImage(p_width,p_height,QImage::Format_ARGB32);
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
void TextObject::refreshTextSize()
{
    // estimate size of text to be rendered
    QFontMetrics fm(p_TextFont);
    int width = fm.width(p_TextString);
    int height = fm.height()*(p_TextString.count("\n")+1);
	setSize(width, height);
}

// Get a complete list of all the info we want to show for this file.
QList<fileInfoItem> TextObject::getInfoList()
{
	QList<fileInfoItem> infoList;

	infoList.append(fileInfoItem("Text", p_TextString));
	infoList.append(fileInfoItem("Font Family", p_TextFont.family()));

	// Font size
	QString fontSize = QString("%1 pixels").arg(p_TextFont.pixelSize());
	if (p_TextFont.pixelSize() == -1)
		// Font was set in points
		fontSize = QString("%1 points").arg(p_TextFont.pointSize());
	infoList.append(fileInfoItem("Font Size", fontSize));

	// Font style
	QString fontStyle;
	if (p_TextFont.bold() && p_TextFont.italic())
		fontStyle += "Bold, Italic";
	else {
		if (p_TextFont.bold())
			fontStyle += "Bold";
		if (p_TextFont.italic())
			fontStyle += "Italic";
	}
	infoList.append(fileInfoItem("Font Style", fontStyle));

	QString colorRGBA = QString("%1,%2,%3,%4").arg(p_TextColor.red()).arg(p_TextColor.green()).arg(p_TextColor.blue()).arg(p_TextColor.alpha());
	infoList.append(fileInfoItem("Color (r,g,b,a)", colorRGBA));
	
	return infoList;
}