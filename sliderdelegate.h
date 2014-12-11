#ifndef SLIDERDELEGATE_H
#define SLIDERDELEGATE_H

#include <QItemDelegate>

class SliderDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    explicit SliderDelegate(QObject *parent = 0);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const;

    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const;

    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const;
    
};

#endif // SLIDERDELEGATE_H
