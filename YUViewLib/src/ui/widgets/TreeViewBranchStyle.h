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

#pragma once

#include <QProxyStyle>

/* A QProxyStyle that redraws the QTreeView branch indicator (the
 * expand/collapse arrow) using the palette's text color.
 *
 * Problem: on dark themes the platform style (Windows UxTheme / Fusion) draws
 * the branch arrow with a dark color that is nearly invisible on the dark
 * background. Qt style sheets can't fix this without shipping per-theme icon
 * assets, because once a QTreeView::branch rule is present Qt stops drawing the
 * native arrow and requires an image:url() — which can't be tinted at runtime.
 *
 * This proxy style intercepts only PE_IndicatorBranch and paints a simple
 * filled triangle with QPalette::Text, so the arrow automatically follows the
 * active theme color (light on dark themes, dark on light themes). Every other
 * primitive is forwarded to the base style, leaving the rest of the widget
 * rendering untouched.
 *
 * Apply it per QTreeView/QTreeWidget via
 *   setStyle(new TreeViewBranchStyle(widget->style(), widget)).
 */
class TreeViewBranchStyle : public QProxyStyle
{
public:
  explicit TreeViewBranchStyle(QStyle *baseStyle, QObject *parent = nullptr);

  void drawPrimitive(PrimitiveElement                element,
                     const QStyleOption             *option,
                     QPainter                       *painter,
                     const QWidget                  *widget = nullptr) const override;
};
