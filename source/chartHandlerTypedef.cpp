/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2017  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "chartHandlerTypedef.h"


/*-------------------- Enum ChartOrderBy functions  --------------------*/
QString EnumAuxiliary::asString(chartOrderBy aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case cobPerFrameGrpByValueNrmNone:
      return "each frame, group by value, not normalized";

    case cobPerFrameGrpByValueNrmByArea:
      return "each frame, group by value, normalized";

    case cobPerFrameGrpByBlocksizeNrmNone:
      return "each frame, group by blocksize, not normalized";

    case cobPerFrameGrpByBlocksizeNrmByArea:
      return "each frame, group by blocksize, normalized";

    case cobRangeGrpByValueNrmNone:
      return "frame range, group by value, not normalized";

    case cobRangeGrpByValueNrmByArea:
      return "frame range, group by value, normalized";

    case cobRangeGrpByBlocksizeNrmNone:
      return "frame range, group by blocksize, not normalized";

    case cobRangeGrpByBlocksizeNrmByArea:
      return "frame range, group by blocksize, normalized";

    case cobAllFramesGrpByValueNrmNone:
      return "all frames, group by value, not normalized";

    case cobAllFramesGrpByValueNrmByArea:
      return "all frames, group by value, normalized";

    case cobAllFramesGrpByBlocksizeNrmNone:
      return "all frames, group by blocksize, not normalized";

    case cobAllFramesGrpByBlocksizeNrmByArea:
      return "all frames, group by blocksize, normalized";

    default: // case of cobUnknown
      return "no sort available";
  }
}

QString EnumAuxiliary::asTooltip(chartOrderBy aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case cobPerFrameGrpByValueNrmNone:
      return "the chart will show the value-data not normalized for each frame";

    case cobPerFrameGrpByValueNrmByArea:
      return "the chart will show the value-data normalized for each frame";

    case cobPerFrameGrpByBlocksizeNrmNone:
      return "the chart will show the value for the blocksize not normalized for each frame";

    case cobPerFrameGrpByBlocksizeNrmByArea:
      return "the chart will show the value for the blocksize normalized for each frame";

    case cobRangeGrpByValueNrmNone:
      return "the chart will show the value-data not normalized for the defined range of frames";

    case cobRangeGrpByValueNrmByArea:
      return "the chart will show the value-data normalized for the defined range of frames";

    case cobRangeGrpByBlocksizeNrmNone:
      return "the chart will show the value for the blocksize not normalized for the defined range of frames";

    case cobRangeGrpByBlocksizeNrmByArea:
      return "the chart will show the value for the blocksize normalized for the defined range of frames";

    case cobAllFramesGrpByValueNrmNone:
      return "the chart will show the value-data not normalized for all frames";

    case cobAllFramesGrpByValueNrmByArea:
      return "the chart will show the value-data normalized for all frames";

    case cobAllFramesGrpByBlocksizeNrmNone:
      return "the chart will show the value for the blocksize not normalized for all frames";

    case cobAllFramesGrpByBlocksizeNrmByArea:
      return "the chart will show the value for the blocksize normalized for all frames";

    default: // case of cobUnknown
      return "no sort available";
  }
}

/*-------------------- Enum ChartShow functions  --------------------*/
QString EnumAuxiliary::asString(chartShow aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case csPerFrame:
      return "current frame";

    case csRange:
      return "select range";

    case csAllFrames:
      return "all frames";

    default: // case of csUnknown
      return "no sort avaible";
  }
}

QString EnumAuxiliary::asTooltip(chartShow aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case csPerFrame:
      return "the order will be displayed for current selected frame from the playback";

    case csRange:
      return "the order will be displayed for the specified range";

    case csAllFrames:
      return "the order will display the data for all frames in one chart";

    default: // case of csUnknown
      return "no frame-display selection avaible";
  }
}

/*-------------------- Enum ChartGroupBy functions  --------------------*/
QString EnumAuxiliary::asString(chartGroupBy aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case cgbByValue:
      return "value";

    case cgbByBlocksize:
      return "blocksize";

    default: // case of cgbUnknown
      return "no sort avaible";
  }
}

QString EnumAuxiliary::asTooltip(chartGroupBy aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case cgbByValue:
      return "the order will be grouped by the value";

    case cgbByBlocksize:
      return "the order will be grouped by the blocksize";

    default: // case of cgbUnknown
      return "no sort avaible";
  }
}

/*-------------------- Enum ChartNormalize functions  --------------------*/
QString EnumAuxiliary::asString(chartNormalize aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case cnNone:
      return "no normalize";

    case cgbByBlocksize:
      return "normalize";

    default: // case of cnUnknown
      return "no sort avaible";
  }
}

QString EnumAuxiliary::asTooltip(chartNormalize aEnum)
{
  // check which enum we have and return correct information
  switch (aEnum) {
    case cnNone:
      return "the chart will not normalize";

    case cgbByBlocksize:
      return "the chart will be normalized. It shows the relation of values to summed values";

    default: // case of cnUnknown
      return "no sort avaible";
  }
}

chartOrderBy EnumAuxiliary::makeChartOrderBy(chartShow aShow, chartGroupBy aGroup, chartNormalize aNormalize)
{ // TODO -oCH: check if better solution as the big if-else-cascade (switch case is not better :D )
  // check if one of the parameter is unknown
  if(aShow == csUnknown || aGroup == cgbUnknown || aNormalize == cnUnknown)
    return cobUnknown;

  // lets check if Show option is per frame or all frames
  if(aShow == csPerFrame) // case of per frame
  {
    // lets check if group by option is by value or by blocksize
    if(aGroup == cgbByValue) // case of group by value
    {
      // lets check if normalize the data
      if(aNormalize == cnNone) // case of no normalization
        return cobPerFrameGrpByValueNrmNone;
      else if(aNormalize == cnByArea) // case of normalize by area (aNormalize == cnByArea)
        return cobPerFrameGrpByValueNrmByArea;
      else
        return cobUnknown;
    }
    else if(aGroup == cgbByBlocksize) // case of group by value (aGroup == cgbByBlocksize)
    {
      if(aNormalize == cnNone)
        return cobPerFrameGrpByBlocksizeNrmNone;
      else if(aNormalize == cnByArea)
        return cobPerFrameGrpByBlocksizeNrmByArea;
      else
        return cobUnknown;
    }
    else // case of unknown type
      return cobUnknown;
  }
  else if (aShow == csRange) // case of selected range
  {
    if(aGroup == cgbByValue)
    {
      if( aNormalize == cnNone)
        return cobRangeGrpByValueNrmNone;
      else // aNormalize == cnByArea
        return cobRangeGrpByValueNrmByArea;
    }
    else if(aGroup == cgbByBlocksize)// case of aGroup == cgbByBlocksize
    {
      if(aNormalize == cnNone)
        return cobRangeGrpByBlocksizeNrmNone;
      else if(aNormalize == cnByArea) // aNormalize == cnByArea
        return cobRangeGrpByBlocksizeNrmByArea;
      else
        return cobUnknown;
    }
    else
      return cobUnknown;
  }
  else if(aShow == csAllFrames) // case of all frames
  {
    if(aGroup == cgbByValue)
    {
      if( aNormalize == cnNone)
        return cobAllFramesGrpByValueNrmNone;
      else // aNormalize == cnByArea
        return cobAllFramesGrpByValueNrmByArea;
    }
    else if (aGroup == cgbByBlocksize)// case of aGroup == cgbByBlocksize
    {
      if( aNormalize == cnNone)
        return cobAllFramesGrpByBlocksizeNrmNone;
      else if (aNormalize == cnByArea)// aNormalize == cnByArea
        return cobAllFramesGrpByBlocksizeNrmByArea;
      else
        return cobUnknown;
    }
    else
      return cobUnknown;
  }
  else
    return cobUnknown;
}
