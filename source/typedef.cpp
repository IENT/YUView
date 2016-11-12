/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2016  Institut für Nachrichtentechnik
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

#include "typedef.h"
#include <QWidget>
#include <QLayout>

static void unparentWidgets(QLayout * layout)
{
  const int n = layout->count();
  for (int i = 0; i < n; ++i) {
    QLayoutItem * item = layout->itemAt(i);
    if (item->widget()) item->widget()->setParent(0);
    else if (item->layout()) unparentWidgets(item->layout());
  }
}

// See also http://stackoverflow.com/q/40497358/1329652
void setupUi(void * ui, void(*setupUi)(void * ui, QWidget * widget))
{
  QWidget widget;
  setupUi(ui, &widget);
  QLayout * wrapperLayout = widget.layout();
  Q_ASSERT(wrapperLayout);
  QObjectList const wrapperChildren = wrapperLayout->children();
  Q_ASSERT(wrapperChildren.size() == 1);
  QLayout * topLayout = qobject_cast<QLayout *>(wrapperChildren.first());
  Q_ASSERT(topLayout);
  topLayout->setParent(0);
  delete wrapperLayout;
  unparentWidgets(topLayout);
  Q_ASSERT(widget.findChildren<QObject*>().isEmpty());
}
