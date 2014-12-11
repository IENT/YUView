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
