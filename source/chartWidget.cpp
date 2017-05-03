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

#include "chartWidget.h"

#include "playlistItem.h"

ChartWidget::ChartWidget(QWidget *parent) :QWidget(parent),
  topLayout(this)
{
  topLayout.setContentsMargins(0, 0, 0, 0);
  topLayout.addWidget(&stack);

  // Create and add the empty widget. This widget is shown when no item is selected.
  stack.addWidget(&emptyWidget);
  stack.setCurrentWidget(&emptyWidget);
}

ChartWidget::~ChartWidget()
{
}

void ChartWidget::drawChart()
{
  this->mChart = this->mChartHandler.makeDummyChart();
  this->layout()->addWidget(this->mChart);
}


void ChartWidget::currentSelectedItemsChanged(playlistItem *aItem1, playlistItem *aItem2)
{
  Q_UNUSED(aItem2)

  if (this->parentWidget())
    // get and set title
    this->parentWidget()->setWindowTitle(this->mChartHandler.getStatisticTitle(aItem1));

  if (aItem1)
  {
    // Show the widget of the first selection
    this->mWidget = this->mChartHandler.createChartWidget(aItem1);

    if (stack.indexOf(this->mWidget) == -1)
      // The widget was just created and is not in the stack yet.
      stack.addWidget(this->mWidget);

    // Show the chart widget
    stack.setCurrentWidget(this->mWidget);
  }
  else
  {
    // Show the empty widget
    stack.setCurrentWidget(&emptyWidget);
  }
}

void ChartWidget::itemAboutToBeDeleted(playlistItem *aItem)
{
  if (this->mWidget)
  {
    // Remove it from the stack but don't delete it. The ChartHandler itself will take care of that.
    assert( stack.indexOf(this->mWidget) != -1 );
    stack.removeWidget(this->mWidget);
    this->mChartHandler.removeWidgetFromList(aItem);
  }
}

