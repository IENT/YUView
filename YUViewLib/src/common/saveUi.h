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

#ifndef SAVEUI_H
#define SAVEUI_H

#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

/* A safe wrapper around Ui::Form class, for delayed initialization
 * and in support of widget-less setupUi.
 * The Ui::Form is zeroed out as a way of catching null pointer dereferences
 * before the Ui has been set up.
 */
template <class Ui> class SafeUi : public Ui
{
public:
  SafeUi()
  {
    clear();
  }

  void setupUi(QWidget *widget)
  {
    Q_ASSERT(!m_created);
    Ui::setupUi(widget);
    m_created = true;
  }

  void setupUi()
  {
    Q_ASSERT(!m_created);
    setupUi(static_cast<void*>(this), &SafeUi::setup_ui_helper);
    this->wrapperLayout = NULL; // The wrapper was deleted, don't leave a dangling pointer.
    m_created = true;
  }

  void clear()
  {
    memset(static_cast<Ui*>(this), 0, sizeof(Ui));
    m_created = false;
  }
  
  bool created() const
  {
    return m_created;
  }

private:

  static void unparentWidgets(QLayout *layout)
  {
    const int n = layout->count();
    for (int i = 0; i < n; ++i) 
    {
      QLayoutItem *item = layout->itemAt(i);
      if (item->widget()) 
        item->widget()->setParent(0);
      else if (item->layout()) 
        unparentWidgets(item->layout());
    }
  }

  // See also http://stackoverflow.com/q/40497358/1329652
  void setupUi(void *ui, void(*setupUi)(void *ui, QWidget *widget))
  {
    QWidget widget;
    setupUi(ui, &widget);
    QLayout *wrapperLayout = widget.layout();
    Q_ASSERT(wrapperLayout);
    QObjectList const wrapperChildren = wrapperLayout->children();
    Q_ASSERT(wrapperChildren.size() == 1);
    QLayout *topLayout = qobject_cast<QLayout *>(wrapperChildren.first());
    Q_ASSERT(topLayout);
    topLayout->setParent(0);
    delete wrapperLayout;
    unparentWidgets(topLayout);
    Q_ASSERT(widget.findChildren<QObject*>().isEmpty());
  }

  static void setup_ui_helper(void *ui, QWidget *widget)
  {
    reinterpret_cast<SafeUi*>(ui)->setupUi(widget);
  }

  bool m_created;
};

#endif // SAVEUI_H
