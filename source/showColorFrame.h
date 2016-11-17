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

#ifndef SHOWCOLORFRAME_H
#define SHOWCOLORFRAME_H

#include <QFrame>
#include "statisticsExtensions.h"

class showColorWidget : public QFrame
{
  Q_OBJECT

public:
  showColorWidget(QWidget *parent) : QFrame(parent) { renderRange = false; renderRangeValues = false; }
  virtual void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
  void setColorMapper(const colorMapper &mapper) { renderRange = true; colMapper = mapper; update(); }
  void setPlainColor(const QColor &color) { renderRange = false; plainColor = color; update(); }
  QColor getPlainColor() { return plainColor; }
  void setRenderRangeValues(bool render) { renderRangeValues = render; }

signals:
  // Emitted if the user clicked this widget.
  void clicked();

protected:
  // If the mouse is released, emit a clicked() event.
  virtual void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE { Q_UNUSED(event); emit clicked(); }
  bool        renderRange;
  bool        renderRangeValues;
  colorMapper colMapper;
  QColor      plainColor;
};

#endif
