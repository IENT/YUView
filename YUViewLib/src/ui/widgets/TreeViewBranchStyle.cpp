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
 *   also delete this file here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TreeViewBranchStyle.h"

#include <QPainter>
#include <QStyleOption>

TreeViewBranchStyle::TreeViewBranchStyle(QStyle *baseStyle, QObject *parent)
  : QProxyStyle(baseStyle)
{
  setParent(parent);
}

void TreeViewBranchStyle::drawPrimitive(PrimitiveElement    element,
                                        const QStyleOption *option,
                                        QPainter           *painter,
                                        const QWidget      *widget) const
{
  if (element != PE_IndicatorBranch)
  {
    QProxyStyle::drawPrimitive(element, option, painter, widget);
    return;
  }

  // Only draw the expand/collapse arrow when the row actually has children.
  // For empty branch cells (guide-line-only positions) we paint nothing — the
  // clean look matches modern UIs (Windows 11 / Fusion without guide lines).
  // State_Children / State_Open are flags on QStyle::State, accessible from the
  // base QStyleOption without casting to QStyleOptionBranch.
  if (!(option->state & QStyle::State_Children))
    return;

  const QRect rect = option->rect;
  const bool  isOpen = option->state & QStyle::State_Open;

  // Use the palette text color so the arrow follows the active theme
  // (typically light on dark themes, dark on light themes).
  const QColor arrowColor = option->palette.color(QPalette::Text);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(arrowColor);
  painter->setPen(Qt::NoPen);

  // Draw a centered filled triangle. Size scales with the cell but is capped
  // so it stays proportional on very tall rows.
  const int side = qBound(6, qMin(rect.width(), rect.height()) - 4, 10);
  const int cx = rect.center().x();
  const int cy = rect.center().y();
  const int half = side / 2;

  QPolygonF triangle;
  if (isOpen)
  {
    // Pointing down (expanded).
    triangle << QPointF(cx - half, cy - half / 2) << QPointF(cx + half, cy - half / 2)
             << QPointF(cx, cy + half);
  }
  else
  {
    // Pointing right (collapsed).
    triangle << QPointF(cx - half / 2, cy - half) << QPointF(cx - half / 2, cy + half)
             << QPointF(cx + half, cy);
  }
  painter->drawPolygon(triangle);

  painter->restore();
}
