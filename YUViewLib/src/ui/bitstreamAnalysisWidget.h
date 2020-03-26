/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef BITSTREAMANALYSISDIALOG_H
#define BITSTREAMANALYSISDIALOG_H

#include <QWidget>
#include <QtConcurrent>

#include "ui_bitstreamAnalysisWidget.h"

#include "parser/parserBase.h"
#include "playlistitem/playlistItem.h"
#include "playlistitem/playlistItemCompressedVideo.h"
#include "common/typedef.h"

class BitstreamAnalysisWidget : public QWidget
{
  Q_OBJECT

public:
  BitstreamAnalysisWidget(QWidget *parent = nullptr);
  ~BitstreamAnalysisWidget() { stopAndDeleteParserBlocking(); }

public slots:
  void currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2, bool chageByPlayback);

private slots:
  void updateParserItemModel();
  void updateStreamInfo();
  void backgroundParsingDone(QString error);

  void showOnlyStreamComboBoxIndexChanged(int index);
  void colorCodeStreamsCheckBoxToggled(bool state) { this->parser->setStreamColorCoding(state); }
  void parseEntireBitstreamCheckBoxToggled(bool state) { Q_UNUSED(state); this->restartParsingOfCurrentItem(); }
  void bitratePlotOrderComboBoxIndexChanged(int index);

protected:
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;
  
private:
  
  Ui::bitstreamAnalysisWidget ui;

  // -1: No Item selected, 0-99: parsing in progress, 100: parsing done
  void updateParsingStatusText(int progressValue);

  void stopAndDeleteParserBlocking();

  void restartParsingOfCurrentItem();
  void createAndConnectNewParser(YUView::inputFormat inputFormatType);

  QScopedPointer<parserBase> parser;
  QFuture<void> backgroundParserFuture;
  void backgroundParsingFunction();

  QPointer<playlistItemCompressedVideo> currentCompressedVideo;

  // -1: Show all streams. Otherwise only show the given stream index.
  int showOnlyStream {-1};
};

#endif // BITSTREAMANALYSISDIALOG_H
