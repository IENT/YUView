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

#include "sliderdelegate.h"
#include <QSpinBox>

SliderDelegate::SliderDelegate(QObject *parent) :
    QItemDelegate(parent)
{
}


QWidget *SliderDelegate::createEditor(QWidget *parent,
    const QStyleOptionViewItem &/* option */,
    const QModelIndex &/* index */) const
{
    QSlider *editor = new QSlider(parent);
    editor->setOrientation(Qt::Vertical);
    editor->setMinimum(0);
    editor->setMaximum(100);

    return editor;
}

void SliderDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    QSlider *slider = static_cast<QSlider*>(editor);
    slider->setValue(value);
}

void SliderDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QSlider *slider = static_cast<QSlider*>(editor);
    int value = slider->value();

    model->setData(index, value, Qt::EditRole);
}

void SliderDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect.x()+option.rect.width()-25, option.rect.y(), 25, 80);
}
