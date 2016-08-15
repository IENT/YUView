#include "statisticsstylecontrol.h"
#include "ui_statisticsstylecontrol.h"
#include <QPainter>
#include <QColorDialog>
#include "statisticsExtensions.h"

#define STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT 1
#if STATISTICS_STYLE_CONTROL_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_STAT_STYLE qDebug
#else
#define DEBUG_STAT_STYLE(fmt,...) ((void)0)
#endif

void showColorWidget::paintEvent(QPaintEvent * event)
{
  Q_UNUSED(event);

  // Draw
  QPainter painter(this);
  QSize s = size();

  switch (type)
  {
    case colorRangeType:
    {
      // Create a temporary color range. We scale this range to the range from 0 to 100. 
      // This is the range of values that we will draw.
      ColorRange cRange = customRange;
      cRange.rangeMin = 0;
      cRange.rangeMax = 100;

      float stepsize = s.width() / 100;
      for (int i=0; i < 100; i++)
      {
        QRectF rect = QRectF((float)i*stepsize, 0.0, stepsize, s.height());
        painter.fillRect(rect, cRange.getColor((float)i));
      }

      break;
    }
    case vectorType:
    {
      QRect rect = QRect(0,0, s.width(), s.height());
      painter.fillRect(rect, plainColor);
      break;
    }
    // Todo
    case colorMapType:
    default:
    {
      QRect rect = QRect(0,0, s.width(), s.height());
      painter.fillRect(rect, QColor(0,0,0,0));
      break;
    }
  }
}

StatisticsStyleControl::StatisticsStyleControl(QWidget *parent) :
  QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint),
  ui(new Ui::StatisticsStyleControl)
{
  ui->setupUi(this);
  ui->pushButtonEditMinColor->setIcon(QIcon(":img_edit.png"));
  ui->pushButtonEditMaxColor->setIcon(QIcon(":img_edit.png"));
  ui->pushButtonEditVectorColor->setIcon(QIcon(":img_edit.png"));
  ui->pushButtonEditGridColor->setIcon(QIcon(":img_edit.png"));
}

StatisticsStyleControl::~StatisticsStyleControl()
{
  delete ui;
}

void StatisticsStyleControl::setStatsItem(StatisticsType *item)
{
  DEBUG_STAT_STYLE("StatisticsStyleControl::setStatsItem %s", item->typeName.toStdString().c_str());
  currentItem = item;
  setWindowTitle("Edit statistics rendering: " + currentItem->typeName);

  if (currentItem->visualizationType == vectorType)
  {
    ui->groupBoxBlockData->hide();
    ui->groupBoxVector->show();

    // Update all the values in the vector controls
    Qt::PenStyle penStyle = currentItem->vectorPen.style();
    int penStyleIndex = -1;
    if (penStyle == Qt::SolidLine)
      penStyleIndex = 0;
    else if (penStyle == Qt::DashLine)
      penStyleIndex = 1;
    else if (penStyle == Qt::DotLine)
      penStyleIndex = 2;
    else if (penStyle == Qt::DashDotLine)
      penStyleIndex = 3;
    else if (penStyle == Qt::DashDotDotLine)
      penStyleIndex = 4;
    if (penStyleIndex != -1)
      ui->comboBoxVectorLineStyle->setCurrentIndex(penStyleIndex);
    ui->doubleSpinBoxVectorLineWidth->setValue(currentItem->vectorPen.widthF());
    ui->checkBoxVectorScaleToZoom->setChecked(currentItem->scaleVectorToZoom);
    ui->comboBoxVectorHeadStyle->setCurrentIndex((int)currentItem->arrowHead);
    ui->checkBoxVectorMapToColor->setChecked(currentItem->mapVectorToColor);
    ui->colorWidgetVectorColor->setPlainColor(currentItem->vectorPen.color());

  }
  else if (currentItem->visualizationType==colorRangeType)
  {
    ui->groupBoxVector->hide();
    ui->groupBoxBlockData->show();
    
    ui->spinBoxRangeMin->setValue((double)currentItem->colorRange.rangeMin);
    ui->spinBoxRangeMax->setValue((double)currentItem->colorRange.rangeMax);
    ui->widgetDataColor->setColorRange(currentItem->colorRange);

    // Update all the values in the block data controls.
    ui->comboBoxDataColorMap->setCurrentIndex((int)currentItem->colorRange.getTypeId());
    if (currentItem->colorRange.getTypeId() == 0)
    {
      // The color range is a custom range
      // Enable/setup the controls for the minimum and maximum color
      ui->widgetMinColor->setEnabled(true);
      ui->pushButtonEditMinColor->setEnabled(true);
      ui->widgetMinColor->setPlainColor(currentItem->colorRange.minColor);
      ui->widgetMaxColor->setEnabled(true);
      ui->pushButtonEditMaxColor->setEnabled(true);
      ui->widgetMaxColor->setPlainColor(currentItem->colorRange.maxColor);
      
    }
    else
    {
      // The color range is one of the predefined default color maps
      // Disable the color min/max controls
      ui->widgetMinColor->setEnabled(false);
      ui->pushButtonEditMinColor->setEnabled(false);
      ui->widgetMaxColor->setEnabled(false);
      ui->pushButtonEditMaxColor->setEnabled(false);      
    }
  }
  else
  {
    ui->groupBoxVector->hide();
    ui->groupBoxBlockData->hide();
  }

  ui->widgetGridColor->setPlainColor(currentItem->gridPen.color());
  ui->doubleSpinBoxGridLineWidth->setValue(currentItem->gridPen.widthF());
  ui->checkBoxGridScaleToZoom->setChecked(currentItem->scaleGridToZoom);

  // Convert the current pen style to an index and set it in the comboBoxGridLineStyle 
  Qt::PenStyle penStyle = currentItem->gridPen.style();
  int penStyleIndex = -1;
  if (penStyle == Qt::SolidLine)
    penStyleIndex = 0;
  else if (penStyle == Qt::DashLine)
    penStyleIndex = 1;
  else if (penStyle == Qt::DotLine)
    penStyleIndex = 2;
  else if (penStyle == Qt::DashDotLine)
    penStyleIndex = 3;
  else if (penStyle == Qt::DashDotDotLine)
    penStyleIndex = 4;
  if (penStyleIndex != -1)
    ui->comboBoxGridLineStyle->setCurrentIndex(penStyleIndex);
  
  resize(sizeHint());
}

void StatisticsStyleControl::on_groupBoxBlockData_clicked(bool check)
{
}

void StatisticsStyleControl::on_comboBoxDataColorMap_currentIndexChanged(int index)
{
  if (index == 0)
  {
    // A custom range is selected
    // Enable/setup the controls for the minimum and maximum color
    ui->widgetMinColor->setEnabled(true);
    ui->pushButtonEditMinColor->setEnabled(true);
    ui->widgetMinColor->setPlainColor(currentItem->colorRange.minColor);
    ui->widgetMaxColor->setEnabled(true);
    ui->pushButtonEditMaxColor->setEnabled(true);
    ui->widgetMaxColor->setPlainColor(currentItem->colorRange.maxColor);
  }
  else
  {
    // Disable the color min/max controls
    ui->widgetMinColor->setEnabled(false);
    ui->pushButtonEditMinColor->setEnabled(false);
    ui->widgetMaxColor->setEnabled(false);
    ui->pushButtonEditMaxColor->setEnabled(false);

    currentItem->colorRange = ColorRange(index, currentItem->colorRange.rangeMin, currentItem->colorRange.rangeMax);
  }

  ui->widgetDataColor->setColorRange(currentItem->colorRange);
  emit StyleChanged();
}

void StatisticsStyleControl::on_widgetMinColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select color range minimum"), QColorDialog::ShowAlphaChannel);
  if (currentItem->colorRange.minColor != newColor)
  {
    currentItem->colorRange.minColor = newColor;
    ui->widgetMinColor->setPlainColor(newColor);
    ui->widgetMinColor->update();
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_widgetMaxColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select color range maximum"), QColorDialog::ShowAlphaChannel);
  if (currentItem->colorRange.maxColor != newColor)
  {
    currentItem->colorRange.maxColor = newColor;
    ui->widgetMaxColor->setPlainColor(newColor);
    ui->widgetMaxColor->update();
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_spinBoxRangeMin_valueChanged(int val)
{
  currentItem->colorRange.rangeMin = val;
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxRangeMax_valueChanged(int val)
{
  currentItem->colorRange.rangeMax = val;
  emit StyleChanged();
}

void StatisticsStyleControl::on_groupBoxVector_clicked(bool check)
{
}

void StatisticsStyleControl::on_comboBoxVectorLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  Qt::PenStyle penStyle;
  if (index == 0)
    penStyle = Qt::SolidLine;
  else if (index == 1)
    penStyle = Qt::DashLine;
  else if (index == 2)
    penStyle = Qt::DotLine;
  else if (index == 3)
    penStyle = Qt::DashDotLine;
  else if (index == 4)
    penStyle = Qt::DashDotDotLine;

  currentItem->vectorPen.setStyle(penStyle);
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxVectorLineWidth_valueChanged(double arg1)
{
  currentItem->vectorPen.setWidthF(arg1);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorScaleToZoom_stateChanged(int arg1)
{
}

void StatisticsStyleControl::on_comboBoxVectorHeadStyle_currentIndexChanged(int index)
{
  currentItem->arrowHead=(arrowHead_t)index;
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxVectorMapToColor_stateChanged(int arg1)
{
  currentItem->mapVectorToColor = (arg1 != 0);
  emit StyleChanged();
}

void StatisticsStyleControl::on_colorWidgetVectorColor_clicked()
{
}

void StatisticsStyleControl::on_groupBoxGrid_clicked(bool check)
{
  currentItem->renderGrid = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_widgetGridColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
  if (newColor != currentItem->gridPen.color())
  {
    currentItem->gridPen.setColor(newColor);
    ui->widgetGridColor->setPlainColor(currentItem->gridPen.color());
    ui->widgetGridColor->update();
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_comboBoxGridLineStyle_currentIndexChanged(int index)
{
  // Convert the selection to a pen style and set it
  Qt::PenStyle penStyle;
  if (index == 0)
    penStyle = Qt::SolidLine;
  else if (index == 1)
    penStyle = Qt::DashLine;
  else if (index == 2)
    penStyle = Qt::DotLine;
  else if (index == 3)
    penStyle = Qt::DashDotLine;
  else if (index == 4)
    penStyle = Qt::DashDotDotLine;

  currentItem->gridPen.setStyle(penStyle);
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxGridLineWidth_valueChanged(double arg1)
{
  currentItem->gridPen.setWidthF(arg1);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxGridScaleToZoom_stateChanged(int arg1)
{
  currentItem->scaleGridToZoom = (arg1 != 0);
  emit StyleChanged();
}
