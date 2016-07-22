#include "statisticsstylecontrol.h"
#include "ui_statisticsstylecontrol.h"
#include <QPainter>
#include <QColorDialog>
#include "statisticsExtensions.h"
void showColorWidget::paintEvent(QPaintEvent * event)
{
  // Draw
  QPainter painter(this);
  QSize s = size();

  switch (type)
  {
    case colorRangeType:
    {
      float min=customRange->rangeMin;
      float max=customRange->rangeMax;
      float colorsteps = (max-min)/100;

      float stepsize = s.width()/100;
      for (int i=0;i<100;i++)
      {
        QRectF rect = QRectF((float)i*stepsize,0.0,stepsize,s.height());
        painter.fillRect(rect,customRange->getColor((float)i*colorsteps));
      }

      break;
    }
    case vectorType:
    {

      QRect rect = QRect(0,0,s.width(),s.height());
      painter.fillRect(rect,plainColor);
      break;
    }
    case colorMapType:
    default:
    {
      QRect rect = QRect(0,0,s.width(),s.height());
      painter.fillRect(rect,QColor(0,0,0,0));
      break;
    }
  }
}


StatisticsStyleControl::StatisticsStyleControl(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::StatisticsStyleControl)
{
  currentId=-1;
  ui->setupUi(this);
  ui->widgetDataColor->setParentControl(this);
  ui->widgetGridColor->setParentControl(this);
}

StatisticsStyleControl::~StatisticsStyleControl()
{
  delete ui;
}

void StatisticsStyleControl::updateStyleControl()
{

}

void StatisticsStyleControl::setStatsItem(int id, StatisticsType *item)
{
  currentId = id;
  currentItem = item;

  if (currentItem->visualizationType==vectorType)
  {
    ui->labelDataColor->setText("Color");
    ui->comboBoxDataColorMap->setEnabled(false);
    ui->pushButtonDataColor->setEnabled(true);
    ui->doubleSpinBoxRangeMin->setEnabled(false);
    ui->doubleSpinBoxRangeMax->setEnabled(false);
  }
  else
  {
    ui->labelDataColor->setText("Color Map");

    ui->doubleSpinBoxRangeMin->setEnabled(true);
    ui->doubleSpinBoxRangeMax->setEnabled(true);
    ui->comboBoxDataColorMap->setEnabled(true);
    ui->pushButtonDataColor->setEnabled(false);
    ui->doubleSpinBoxRangeMin->setValue((double)currentItem->colorRange->rangeMin);
    ui->doubleSpinBoxRangeMax->setValue((double)currentItem->colorRange->rangeMax);
    if (!currentItem->colorRange->isDefaultRange())
    {
      ui->comboBoxDataColorMap->setCurrentIndex(0);
    }
    else
    {
      DefaultColorRange *castCustomRange = dynamic_cast<DefaultColorRange*>(currentItem->colorRange);
      ui->comboBoxDataColorMap->setCurrentIndex((int)castCustomRange->getType()+1);
    }
  }
  ui->doubleSpinBoxDataLineStyle->setValue(currentItem->vectorPen->widthF());
  ui->doubleSpinBoxGridLineStyle->setValue(currentItem->gridPen->widthF());
  ui->comboBoxDataLineSzyle->setCurrentIndex((int)currentItem->vectorPen->style());
  ui->comboBoxGridLineStyle->setCurrentIndex((int)currentItem->gridPen->style());
  ui->widgetDataColor->setColorOrColorMap(currentItem->visualizationType,currentItem->colorRange,currentItem->vectorPen->color());
  ui->widgetGridColor->setColorOrColorMap(vectorType,currentItem->colorRange,currentItem->gridPen->color());
  ui->gridLayout->update();
}

void StatisticsStyleControl::on_comboBoxDataLineSzyle_currentIndexChanged(int index)
{
  currentItem->vectorPen->setStyle((Qt::PenStyle)index);
}


void StatisticsStyleControl::on_comboBoxGridLineStyle_currentIndexChanged(int index)
{
  currentItem->gridPen->setStyle((Qt::PenStyle)index);
}

void StatisticsStyleControl::on_doubleSpinBoxGridLineStyle_valueChanged(double arg1)
{
  currentItem->gridPen->setWidthF(arg1);
}

void StatisticsStyleControl::on_doubleSpinBoxDataLineStyle_valueChanged(double arg1)
{
  currentItem->vectorPen->setWidthF(arg1);
}

void StatisticsStyleControl::on_pushButtonDataColor_released()
{
  if (currentItem->visualizationType==vectorType)
  {
    QColor newColor = QColorDialog::getColor(currentItem->vectorPen->color(), this, tr("Select data color"), QColorDialog::ShowAlphaChannel);
    if (newColor!=currentItem->vectorPen->color())
    {
      currentItem->vectorPen->setColor(newColor);
      ui->widgetDataColor->setColorOrColorMap(currentItem->visualizationType,currentItem->colorRange,currentItem->vectorPen->color());
    }
  }
  else
  {
    // do nothing
  }
}

void StatisticsStyleControl::on_comboBoxDataColorMap_currentIndexChanged(int index)
{
  // store old custom range
    if (index>0)
    {
      int oldmin = currentItem->colorRange->rangeMin;
      int oldmax = currentItem->colorRange->rangeMax;
      DefaultColorRange* newRange = new DefaultColorRange(ui->comboBoxDataColorMap->currentText(),oldmin,oldmax);
      currentItem->colorRange = newRange;
    }
    else
    {
      //customRange->setRange(userCustomRange);
    }
    ui->widgetDataColor->setColorOrColorMap(currentItem->visualizationType,currentItem->colorRange,currentItem->vectorPen->color());
    ui->widgetDataColor->repaint();
}

void StatisticsStyleControl::on_pushButtonGridColor_released()
{
    QColor newColor = QColorDialog::getColor(currentItem->gridPen->color(), this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
    if (newColor!=currentItem->gridPen->color())
    {
      currentItem->gridPen->setColor(newColor);
      ui->widgetGridColor->setColorOrColorMap(vectorType,currentItem->colorRange,currentItem->gridPen->color());
    }
}

void StatisticsStyleControl::on_doubleSpinBoxRangeMin_valueChanged(double arg1)
{
  if (currentItem->colorRange)
    currentItem->colorRange->rangeMin = (int)arg1;
}

void StatisticsStyleControl::on_doubleSpinBoxRangeMax_valueChanged(double arg1)
{
  if (currentItem->colorRange)
    currentItem->colorRange->rangeMax = (int)arg1;
}
