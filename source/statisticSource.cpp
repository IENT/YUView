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

#include "statisticSource.h"

#include <QPainter>

void rotateVector(float angle, float vx, float vy, float &nx, float &ny)
{
	float s = sinf(angle);
	float c = cosf(angle);

	nx = c * vx + s * vy;
	ny = -s * vx + c * vy;

	//normalize vector
	float n_abs = sqrtf(nx*nx + ny*ny);
	nx /= n_abs;
	ny /= n_abs;
}

statisticSource::statisticSource()
{
	p_internalScaleFactor = 1;
	p_lastFrameIdx = -1;
}

statisticSource::~statisticSource()
{
}

bool statisticSource::setInternalScaleFactor(int internalScaleFactor)
{
	internalScaleFactor = clip(internalScaleFactor, 1, MAX_SCALE_FACTOR);

	if (p_internalScaleFactor != internalScaleFactor)
	{
		p_internalScaleFactor = internalScaleFactor;
		return true;
	}
	return false;
}

void statisticSource::drawStatistics(QPixmap *img, int frameIdx)
{
	// draw statistics (inverse order)
	for (int i = p_statsTypeList.count() - 1; i >= 0; i--)
	{
		if (!p_statsTypeList[i].render)
			continue;

		StatisticsItemList stat = getStatistics(frameIdx, p_statsTypeList[i].typeID);
		drawStatisticsImage(img, stat, p_statsTypeList[i]);
	}

	p_lastFrameIdx = frameIdx;
}

void statisticSource::drawStatisticsImage(QPixmap *img, StatisticsItemList statsList, StatisticsType statsType)
{
	QPainter painter(img);

	StatisticsItemList::iterator it;
	for (it = statsList.begin(); it != statsList.end(); ++it)
	{
		StatisticsItem anItem = *it;

		switch (anItem.type)
		{
		case arrowType:
		{
			QRect aRect = anItem.positionRect;
			QRect displayRect = QRect(aRect.left()*p_internalScaleFactor, aRect.top()*p_internalScaleFactor, aRect.width()*p_internalScaleFactor, aRect.height()*p_internalScaleFactor);

			int x, y;

			// start vector at center of the block
			x = displayRect.left() + displayRect.width() / 2;
			y = displayRect.top() + displayRect.height() / 2;

			QPoint startPoint = QPoint(x, y);

			float vx = anItem.vector[0];
			float vy = anItem.vector[1];

			QPoint arrowBase = QPoint(x + p_internalScaleFactor*vx, y + p_internalScaleFactor*vy);
			QColor arrowColor = anItem.color;
			//arrowColor.setAlpha( arrowColor.alpha()*((float)statsType.alphaFactor / 100.0) );

			QPen arrowPen(arrowColor);
			painter.setPen(arrowPen);
			painter.drawLine(startPoint, arrowBase);

			if (vx == 0 && vy == 0)
			{
				// nothing to draw...
			}
			else
			{
				// draw an arrow
				float nx, ny;

				// TODO: scale arrow head with
				float a = p_internalScaleFactor * 4;    // length of arrow
				float b = p_internalScaleFactor * 2;    // base width of arrow

				float n_abs = sqrtf(vx*vx + vy*vy);
				float vxf = (float)vx / n_abs;
				float vyf = (float)vy / n_abs;

				QPoint arrowTip = arrowBase + QPoint(vxf*a + 0.5, vyf*a + 0.5);

				// arrow head right
				rotateVector((float)-M_PI_2, -vx, -vy, nx, ny);
				QPoint offsetRight = QPoint(nx*b + 0.5, ny*b + 0.5);
				QPoint arrowHeadRight = arrowBase + offsetRight;

				// arrow head left
				rotateVector((float)M_PI_2, -vx, -vy, nx, ny);
				QPoint offsetLeft = QPoint(nx*b + 0.5, ny*b + 0.5);
				QPoint arrowHeadLeft = arrowBase + offsetLeft;

				// draw arrow head
				QPoint points[3] = { arrowTip, arrowHeadRight, arrowHeadLeft };
				painter.setBrush(arrowColor);
				painter.drawPolygon(points, 3);
			}

			break;
		}
		case blockType:
		{
			//draw a rectangle
			QColor rectColor = anItem.color;
			rectColor.setAlpha(rectColor.alpha()*((float)statsType.alphaFactor / 100.0));
			painter.setBrush(rectColor);

			QRect aRect = anItem.positionRect;
			QRect displayRect = QRect(aRect.left()*p_internalScaleFactor, aRect.top()*p_internalScaleFactor, aRect.width()*p_internalScaleFactor, aRect.height()*p_internalScaleFactor);

			painter.fillRect(displayRect, rectColor);

			break;
		}
		}

		// optionally, draw a grid around the region
		if (statsType.renderGrid) {
			//draw a rectangle
			QColor gridColor = anItem.gridColor;
			QPen gridPen(gridColor);
			gridPen.setWidth(1);
			painter.setPen(gridPen);
			painter.setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush));  // no fill color

			QRect aRect = anItem.positionRect;
			QRect displayRect = QRect(aRect.left()*p_internalScaleFactor, aRect.top()*p_internalScaleFactor, aRect.width()*p_internalScaleFactor, aRect.height()*p_internalScaleFactor);

			painter.drawRect(displayRect);
		}
	}
}

StatisticsItemList statisticSource::getStatistics(int frameIdx, int typeIdx)
{
	// if requested statistics are not in cache, read from file
	if (!p_statsCache.contains(frameIdx) || !p_statsCache[frameIdx].contains(typeIdx))
	{
		loadStatisticToCache(frameIdx, typeIdx);
	}

	return p_statsCache[frameIdx][typeIdx];
}

StatisticsType* statisticSource::getStatisticsType(int typeID)
{
	for (int i = 0; i<p_statsTypeList.count(); i++)
    {
        if( p_statsTypeList[i].typeID == typeID )
            return &p_statsTypeList[i];
    }

    return NULL;
}

// return raw(!) value of frontmost, active statistic item at given position
ValuePairList statisticSource::getValuesAt(int x, int y)
{
	ValuePairList valueList;

	for (int i = 0; i<p_statsTypeList.count(); i++)
	{
		if (p_statsTypeList[i].render)  // only show active values
		{
			int typeID = p_statsTypeList[i].typeID;
			StatisticsItemList statsList = getStatistics(p_lastFrameIdx, typeID);

			if (statsList.size() == 0 && typeID == INT_INVALID) // no active statistics
				continue;

			StatisticsType* aType = getStatisticsType(typeID);
			Q_ASSERT(aType->typeID != INT_INVALID && aType->typeID == typeID);

			// find item of this type at requested position
			StatisticsItemList::iterator it;
			bool foundStats = false;
			for (it = statsList.begin(); it != statsList.end(); ++it)
			{
				StatisticsItem anItem = *it;
				QRect aRect = anItem.positionRect;
								
				if (aRect.contains(x, y))
				{
					if (anItem.type == blockType)
					{
						QString sValTxt = aType->getValueTxt(anItem.rawValues[0]);
						valueList.append(ValuePair(aType->typeName, sValTxt));
					}
					else if (anItem.type == arrowType)
					{
						// TODO: do we also want to show the raw values?
						float vectorValue1 = anItem.vector[0];
						float vectorValue2 = anItem.vector[1];
						valueList.append(ValuePair(QString("%1[x]").arg(aType->typeName), QString::number(vectorValue1)));
						valueList.append(ValuePair(QString("%1[y]").arg(aType->typeName), QString::number(vectorValue2)));
					}

					foundStats = true;
				}
			}

			if (!foundStats)
				valueList.append(ValuePair(aType->typeName, "-"));
		}
	}

	return valueList;
}

/* Set the statistics Type list.
 * we do not overwrite our statistics type, we just change their parameters
 * return if something has changed where a redraw would be necessary
*/
bool statisticSource::setStatisticsTypeList(StatisticsTypeList typeList)
{
	bool bChanged = false;
	foreach(StatisticsType aType, typeList)
	{
		StatisticsType* internalType = getStatisticsType(aType.typeID);

		if (internalType->typeName != aType.typeName)
			continue;

		if (internalType->render != aType.render) {
			internalType->render = aType.render;
			bChanged = true;
		}
		if (internalType->renderGrid != aType.renderGrid) {
			internalType->renderGrid = aType.renderGrid;
			bChanged = true;
		}

		if (internalType->alphaFactor != aType.alphaFactor) {
			internalType->alphaFactor = aType.alphaFactor;
			bChanged = true;
		}
	}

	return bChanged;
}

/* Check if at least one of the statistics is actually displayed.
*/
bool statisticSource::anyStatisticsRendered()
{
	for (int i = 0; i<p_statsTypeList.count(); i++)
    {
        if( p_statsTypeList[i].render )
            return true;
    }
	return false;
}