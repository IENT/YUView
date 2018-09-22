/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "bitstreamAnalysisDialog.h"

#include "parserAnnexBAVC.h"
#include "parserAnnexBHEVC.h"
#include "parserAnnexBMpeg2.h"
#include "parserAVFormat.h"

bitstreamAnalysisDialog::bitstreamAnalysisDialog(QWidget *parent, QString fileName, inputFormat inputFormatType) :
  QDialog(parent)
{
  ui.setupUi(this);

  // Parse the file using the apropriate parser ...
  if (inputFormatType == inputAnnexBHEVC || inputFormatType == inputAnnexBAVC)
  {
    // Just open and parse the file again
    QScopedPointer<fileSourceAnnexBFile> annexBFile(new fileSourceAnnexBFile(fileName));
    // Create a parser
    QScopedPointer<parserAnnexB> parserAnnexB;
    if (inputFormatType == inputAnnexBHEVC)
      parserAnnexB.reset(new parserAnnexBHEVC());
    else if (inputFormatType == inputAnnexBAVC)
      parserAnnexB.reset(new parserAnnexBAVC());

    // Parse the file
    parserAnnexB->enableModel();
    if (!parserAnnexB->parseAnnexBFile(annexBFile))
      return;

    parser.reset(parserAnnexB.take());
  }
  else if (inputFormatType == inputLibavformat)
  {
    // Just open and parse the file again
    QScopedPointer<fileSourceFFmpegFile> ffmpegFile(new fileSourceFFmpegFile(fileName));
    AVCodecSpecfier codec = ffmpegFile->getCodecSpecifier();
    QScopedPointer<parserAVFormat> p(new parserAVFormat(codec));
    p->enableModel();
    if (!p->parseFFMpegFile(ffmpegFile))
      return;

    parser.reset(p.take());
  }
  else
    return;

  ui.dataTreeView->setModel(parser->getNALUnitModel());

  ui.dataTreeView->setColumnWidth(0, 300);
  ui.dataTreeView->setColumnWidth(1, 50);
  ui.dataTreeView->setColumnWidth(2, 70);
}