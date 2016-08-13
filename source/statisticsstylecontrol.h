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

  // Set the current statistics item to edit. If this function is called, the controls will be updated to show
  // the current style of the given item. If a control is changed, the style of this given item will be updated.
  void setStatsItem(StatisticsType *item);
signals:
  // The style was changed by the user. Redraw the view.
  void StyleChanged();

private slots:
  // Block data controls
  void on_groupBoxBlockData_clicked(bool check);
  void on_comboBoxDataColorMap_currentIndexChanged(int index);
  void on_widgetMinColor_clicked();
  void on_pushButtonEditMinColor_clicked() { on_widgetMinColor_clicked(); }
  void on_widgetMaxColor_clicked();
  void on_pushButtonEditMaxColor_clicked() { on_widgetMaxColor_clicked(); }
  void on_spinBoxRangeMin_valueChanged(int arg1);
  void on_spinBoxRangeMax_valueChanged(int arg1);

  // Vector data controls
  void on_groupBoxVector_clicked(bool check);
  void on_comboBoxVectorLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxVectorLineWidth_valueChanged(double arg1);
  void on_checkBoxVectorScaleToZoom_stateChanged(int arg1);
  void on_comboBoxVectorHeadStyle_currentIndexChanged(int index);
  void on_checkBoxVectorMapToColor_stateChanged(int arg1);
  void on_colorWidgetVectorColor_clicked();
  void on_pushButtonEditVectorColor_clicked() { on_colorWidgetVectorColor_clicked(); }

  // Grid slots
  void on_groupBoxGrid_clicked(bool check);
  void on_widgetGridColor_clicked();
  void on_pushButtonEditGridColor_clicked() { on_widgetGridColor_clicked(); }
  void on_comboBoxGridLineStyle_currentIndexChanged(int index);
  void on_doubleSpinBoxGridLineWidth_valueChanged(double arg1);
  void on_checkBoxGridScaleToZoom_stateChanged(int arg1);

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
  void setColorRange(ColorRange range) { type=colorRangeType; customRange=range; update(); }
  void setPlainColor(QColor color) { type=vectorType; plainColor=color; update(); }
signals:
  // Emitted if the user clicked this widget.
  void clicked();
protected:
  // If the mouse is released, emit a clicked() event.
  virtual void mouseReleaseEvent(QMouseEvent * event) Q_DECL_OVERRIDE { Q_UNUSED(event); emit clicked(); }
  visualizationType_t type;
  ColorRange        customRange;
  QColor            plainColor;
};

#endif // STATISTICSSTYLECONTROL_H
