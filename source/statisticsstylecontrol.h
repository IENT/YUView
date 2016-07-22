#ifndef STATISTICSSTYLECONTROL_H
#define STATISTICSSTYLECONTROL_H

#include <QDialog>
#include <QWidget>
#include <QPen>
#include "statisticsExtensions.h"



namespace Ui {
  class StatisticsStyleControl;
}

class StatisticsStyleControl : public QDialog
{
  Q_OBJECT

public:
  explicit StatisticsStyleControl(QWidget *parent = 0);
  ~StatisticsStyleControl();

  void setStatsItem(int id, StatisticsType *item);
public slots:
  void updateStyleControl();

private slots:
  void on_comboBoxDataLineSzyle_currentIndexChanged(int index);
  void on_comboBoxGridLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxGridLineStyle_valueChanged(double arg1);
  void on_doubleSpinBoxDataLineStyle_valueChanged(double arg1);

  void on_pushButtonDataColor_released();

  void on_comboBoxDataColorMap_currentIndexChanged(int index);

  void on_pushButtonGridColor_released();

  void on_doubleSpinBoxRangeMin_valueChanged(double arg1);

  void on_doubleSpinBoxRangeMax_valueChanged(double arg1);

private:
  Ui::StatisticsStyleControl *ui;
  Qt::PenStyle getPenStyleFromIndex(int i);
 int currentId;
 StatisticsType *currentItem;
};

class showColorWidget : public QWidget
{
public:
  showColorWidget(QWidget *parent) : QWidget(parent) {};
  virtual void paintEvent(QPaintEvent * event) Q_DECL_OVERRIDE;
  void setParentControl (StatisticsStyleControl *someController) {control=someController;}
  void setColorOrColorMap(visualizationType_t someType,ColorRange *someColorRange, QColor someColor) {type=someType;customRange=someColorRange;plainColor=someColor;}
private:
  StatisticsStyleControl *control;
  visualizationType_t type;
  ColorRange        *customRange;
  QColor            plainColor;
};

#endif // STATISTICSSTYLECONTROL_H
