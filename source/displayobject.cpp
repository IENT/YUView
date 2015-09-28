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

#include "displayobject.h"

DisplayObject::DisplayObject(QObject *parent) : QObject(parent)
{
    // preset internal values
    p_startFrame = 0;
    p_endFrame = -1;
    p_sampling = 1;
    p_frameRate = 1;

    p_width = 1;
    p_height = 1;

    p_lastIdx = INT_MAX;    // initialize with magic number ;)

    p_status = "OK";
    p_info = "";
}

DisplayObject::~DisplayObject()
{

}

QString DisplayObject::getStatusAndInfo()
{
  QString ret = p_status;
  // Append temporaray info
  if (!p_info.isEmpty()) {
    ret += QString("\n") + p_info;
  }
  // Append permanent info
  if (!p_info_permanent.isEmpty()) {
    for (int i = 0; i < p_info_permanent.count(); i++) {
      ret += QString("\n") + p_info_permanent[i];
    }
  }
  return ret;
}

bool DisplayObject::setInfo(QString s, bool permament) 
{ 
  if (permament) {
    // Do not append permanent warnings more than once
    for (int i = 0; i < p_info_permanent.count(); i++) {
      if (p_info_permanent[i] == s)
        return false; // Already in list. Do not append.
    }
    p_info_permanent.append(s);
  }
  else {
    p_info = s;
  }
  return true;
}

void DisplayObject::frameIndexLimits(int &minIdx, int &maxIdx)
{
	minIdx = (p_startFrame > 0) ? p_startFrame : 0;
	maxIdx = numFrames() - 1;
	if (p_endFrame >= 0 && p_endFrame < maxIdx)
		// End Frame active and smaller
		maxIdx = p_endFrame;
	if (maxIdx < minIdx)
		maxIdx = minIdx;
}