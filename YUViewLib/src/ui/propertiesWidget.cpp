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

#include "propertiesWidget.h"

#include "playlistitem/playlistItem.h"

/* The file info group box can display information on a file (or any other display object).
 * If you provide a list of QString tuples, this class will fill a grid layout with the 
 * corresponding labels.
 */
PropertiesWidget::PropertiesWidget(QWidget *parent) :
  QWidget(parent),
  topLayout(this)
{
  topLayout.setContentsMargins(0, 0, 0, 0);
  topLayout.addWidget(&stack);

  // Create and add the empty widget. This widget is shown when no item is selected.
  stack.addWidget(&emptyWidget);
  stack.setCurrentWidget(&emptyWidget);
}

void PropertiesWidget::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  // The properties are always just shown for the first item
  Q_UNUSED(item2);

  if (parentWidget())
  {
    if (item1)
      parentWidget()->setWindowTitle(item1->getPropertiesTitle());
    else
      parentWidget()->setWindowTitle(PROPERTIESWIDGET_DEFAULT_WINDOW_TITLE);
  }

  if (item1)
  {
    // Show the properties widget of the first selection
    QWidget *propertiesWidget = item1->getPropertiesWidget();

    if (stack.indexOf(propertiesWidget) == -1)
      // The properties widget was just created and is not in the stack yet.
      stack.addWidget(propertiesWidget);

    // Show the properties widget
    stack.setCurrentWidget(propertiesWidget);
  }
  else
  {
    // Show the empty widget
    stack.setCurrentWidget(&emptyWidget);
  }
}

void PropertiesWidget::itemAboutToBeDeleted(playlistItem *item)
{
  if (item->propertiesWidgetCreated())
  {
    // The properties widget for the item was created and it should be in the widget stack.
    // Remove it from the stack but don't delete it. The playlistItem itself will take care of that.
    QWidget *w = item->getPropertiesWidget();
    assert(stack.indexOf(w) != -1);
    stack.removeWidget(w);
  }
}
