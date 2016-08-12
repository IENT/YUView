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

  void setStatsItem(StatisticsType *item);
signals:
  void StyleChanged();

private slots:
  void on_comboBoxDataLineSzyle_currentIndexChanged(int index);
  void on_comboBoxGridLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxGridLineStyle_valueChanged(double arg1);
  void on_doubleSpinBoxDataLineStyle_valueChanged(double arg1);
  void on_comboBoxDataColorMap_currentIndexChanged(int index);
  //void on_pushButtonGridColor_released();
  void on_doubleSpinBoxRangeMin_valueChanged(double arg1);
  void on_doubleSpinBoxRangeMax_valueChanged(double arg1);
  void on_checkBoxMapColor_stateChanged(int arg1);
  void on_widgetGridColor_clicked();
  void on_pushButtonEditGridColor_clicked() { on_widgetGridColor_clicked(); }
  void on_groupBoxGrid_clicked(bool check);
  void on_checkBoxScaleGridToZoom_stateChanged(int arg1);

  void on_comboBoxDataHeadStyle_currentIndexChanged(int index);

private:
  Ui::StatisticsStyleControl *ui;
 StatisticsType *currentItem;
};

class showColorWidget : public QWidget
{
  Q_OBJECT
public:
  showColorWidget(QWidget *parent) : QWidget(parent) {};
  virtual void paintEvent(QPaintEvent * event) Q_DECL_OVERRIDE;
  void setParentControl (StatisticsStyleControl *someController) { control=someController; }
  void setColorOrColorMap(visualizationType_t someType,ColorRange *someColorRange, QColor someColor) { type=someType; customRange=someColorRange; plainColor=someColor; }
signals:
  void clicked();
protected:
  virtual void mouseReleaseEvent(QMouseEvent * event) Q_DECL_OVERRIDE { Q_UNUSED(event); emit clicked(); }
  StatisticsStyleControl *control;
  visualizationType_t type;
  ColorRange        *customRange;
  QColor            plainColor;
};

#endif // STATISTICSSTYLECONTROL_H
