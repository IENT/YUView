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

#ifndef SIGNALSSLOTS_H
#define SIGNALSSLOTS_H

/* These are method pointers to signals that have multiple overloads. They
 * help prevent having to cast signals, e.g.
 * static_cast<void(QComboBox::)(int)>(&QComboBox::currentIndexChanged)
 */

// Keep this list sorted alphabetically by method pointer name
#ifdef QCOMBOBOX_H
static void(QComboBox::* const QComboBox_currentIndexChanged_int)(int) = &QComboBox::currentIndexChanged;
#endif
#ifdef QSPINBOX_H
static void(QDoubleSpinBox::* const QDoubleSpinBox_valueChanged_double)(double) = &QDoubleSpinBox::valueChanged;
static void(QSpinBox::* const QSpinBox_valueChanged_int)(int) = &QSpinBox::valueChanged;
#endif
#ifdef QTIMER_H
static void(QTimer::* const QTimer_start)() = &QTimer::start;
#endif
#ifdef QWIDGET_H
static void(QWidget::* const QWidget_update)() = &QWidget::update;
#endif

#endif // SIGNALSSLOTS_H
