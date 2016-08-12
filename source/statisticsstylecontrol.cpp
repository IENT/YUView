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
    case defaultColorRangeType:
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
    // Todo
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
  QDialog(parent, Qt::Dialog | Qt::WindowStaysOnTopHint),
  ui(new Ui::StatisticsStyleControl)
{
  ui->setupUi(this);
  ui->widgetDataColorRange->setParentControl(this);
  ui->widgetDataColor->setParentControl(this);
  ui->widgetGridColor->setParentControl(this);
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

  if (currentItem->visualizationType==vectorType)
  {
    ui->groupBoxData->setTitle("Vector");

    ui->labelDataColor->show();
    ui->labelDataLineStyle->show();
    ui->labelDataMapToColor->show();
    ui->labelDataLineWidth->show();
    ui->labelDataVectorHead->show();

    ui->labelDataColorRange->hide();
    ui->comboBoxDataColorMap->hide();

    ui->comboBoxDataLineSzyle->show();
    ui->checkBoxMapColor->show();
    ui->doubleSpinBoxDataLineStyle->show();
    ui->comboBoxDataHeadStyle->show();

    ui->widgetDataColor->show();
    ui->widgetDataColorRange->hide();
    ui->doubleSpinBoxRangeMax->hide();
    ui->doubleSpinBoxRangeMin->hide();

    ui->doubleSpinBoxDataLineStyle->setValue(currentItem->vectorPen.widthF());
    ui->widgetDataColor->setColorOrColorMap(vectorType,NULL,currentItem->vectorPen.color());
    ui->widgetDataColor->repaint();
    ui->comboBoxDataLineSzyle->setCurrentIndex((int)currentItem->vectorPen.style());
    ui->checkBoxMapColor->setChecked(currentItem->mapVectorToColor);
    ui->comboBoxDataHeadStyle->setCurrentIndex((int)currentItem->arrowHead);
  }
  else if (currentItem->visualizationType==colorRangeType||currentItem->visualizationType==defaultColorRangeType)
  {
    ui->groupBoxData->setTitle("Color Range");

    ui->labelDataColor->hide();
    ui->labelDataLineStyle->hide();
    ui->labelDataMapToColor->hide();
    ui->labelDataLineWidth->hide();
    ui->labelDataVectorHead->hide();

    ui->labelDataColorRange->show();
    ui->comboBoxDataColorMap->show();

    ui->comboBoxDataLineSzyle->hide();
    ui->checkBoxMapColor->hide();
    ui->doubleSpinBoxDataLineStyle->hide();
    ui->comboBoxDataHeadStyle->hide();

    ui->widgetDataColor->hide();
    ui->widgetDataColorRange->show();
    ui->doubleSpinBoxRangeMax->show();
    ui->doubleSpinBoxRangeMin->show();

    if (currentItem->visualizationType==colorRangeType)
    {
      ui->doubleSpinBoxRangeMin->setValue((double)currentItem->colorRange->rangeMin);
      ui->doubleSpinBoxRangeMax->setValue((double)currentItem->colorRange->rangeMax);
      ui->comboBoxDataColorMap->setCurrentIndex(0);
      ui->widgetDataColorRange->setColorOrColorMap(currentItem->visualizationType,currentItem->colorRange,currentItem->vectorPen.color());
    }
    else
    {
      ui->doubleSpinBoxRangeMin->setValue((double)currentItem->defaultColorRange->rangeMin);
      ui->doubleSpinBoxRangeMax->setValue((double)currentItem->defaultColorRange->rangeMax);
      ui->comboBoxDataColorMap->setCurrentIndex((int)currentItem->defaultColorRange->getType()+1);
      ui->widgetDataColorRange->setColorOrColorMap(currentItem->visualizationType,currentItem->defaultColorRange,currentItem->vectorPen.color());
    }
  }
  else
  {
    // todo
  }

  ui->widgetGridColor->setColorOrColorMap(vectorType,NULL,currentItem->gridPen.color());
  ui->doubleSpinBoxGridLineStyle->setValue(currentItem->gridPen.widthF());
  ui->checkBoxScaleGridToZoom->setChecked(currentItem->scaleGridToZoom);

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

void StatisticsStyleControl::on_comboBoxDataLineSzyle_currentIndexChanged(int index)
{
  currentItem->vectorPen.setStyle((Qt::PenStyle)index);
  emit StyleChanged();
}

void StatisticsStyleControl::on_groupBoxGrid_clicked(bool check)
{
  currentItem->renderGrid = check;
  emit StyleChanged();
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

void StatisticsStyleControl::on_doubleSpinBoxGridLineStyle_valueChanged(double arg1)
{
  currentItem->gridPen.setWidthF(arg1);
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxDataLineStyle_valueChanged(double arg1)
{
  currentItem->vectorPen.setWidthF(arg1);
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxScaleGridToZoom_stateChanged(int arg1)
{
  currentItem->scaleGridToZoom = (arg1 != 0);
  emit StyleChanged();
}

//void StatisticsStyleControl::on_widgetGridColor_clicked()
//{
//  if (currentItem->visualizationType==vectorType)
//  {
//    QColor newColor = QColorDialog::getColor(currentItem->vectorPen.color(), this, tr("Select data color"), QColorDialog::ShowAlphaChannel);
//    if (newColor!=currentItem->vectorPen.color())
//    {
//      currentItem->vectorPen.setColor(newColor);
//      ui->widgetDataColor->setColorOrColorMap(vectorType,NULL,currentItem->vectorPen.color());
//      ui->widgetDataColor->repaint();
//      emit StyleChanged();
//    }
//  }
//  else
//  {
//    // do nothing
//  }
//}

void StatisticsStyleControl::on_widgetGridColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
  if (newColor!=currentItem->gridPen.color())
  {
    currentItem->gridPen.setColor(newColor);
    ui->widgetGridColor->setColorOrColorMap(vectorType,NULL,currentItem->gridPen.color());
    ui->widgetGridColor->repaint();
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_comboBoxDataColorMap_currentIndexChanged(int index)
{
  // store old custom range
  if (index>0)
  {
    int oldmin,oldmax;
    if (currentItem->defaultColorRange)
    {
      oldmin = currentItem->defaultColorRange->rangeMin;
      oldmax = currentItem->defaultColorRange->rangeMax;
    }
    else if (currentItem->colorRange)
    {
      oldmin = currentItem->colorRange->rangeMin;
      oldmax = currentItem->colorRange->rangeMax;
    }
    else
    {
      oldmin = 0;
      oldmax = 0;
    }
    DefaultColorRange* newRange = new DefaultColorRange(ui->comboBoxDataColorMap->currentText(),oldmin,oldmax);
    currentItem->defaultColorRange = newRange;
    currentItem->visualizationType = defaultColorRangeType;
    ui->widgetDataColorRange->setColorOrColorMap(currentItem->visualizationType,currentItem->defaultColorRange,currentItem->vectorPen.color());
  }
  else
  {
    if (currentItem->colorRange)
    {
      currentItem->visualizationType = colorRangeType;
      ui->widgetDataColorRange->setColorOrColorMap(currentItem->visualizationType,currentItem->colorRange,currentItem->vectorPen.color());
    }
  }
  ui->widgetDataColorRange->repaint();
  emit StyleChanged();
}

//void StatisticsStyleControl::on_pushButtonGridColor_released()
//{
//    QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
//    if (newColor!=currentItem->gridPen.color())
//    {
//      currentItem->gridPen.setColor(newColor);
//      ui->widgetGridColor->setColorOrColorMap(vectorType,NULL,currentItem->gridPen.color());
//      ui->widgetGridColor->repaint();
//      emit StyleChanged();
//    }
//}

void StatisticsStyleControl::on_doubleSpinBoxRangeMin_valueChanged(double arg1)
{
  if (currentItem->colorRange && currentItem->visualizationType==colorRangeType)
  {
    currentItem->colorRange->rangeMin = (int)arg1;
  }
  else if (currentItem->defaultColorRange && currentItem->visualizationType==defaultColorRangeType)
  {
    currentItem->defaultColorRange->rangeMin = (int)arg1;
  }
  else
  {
    assert(0);
  }
  emit StyleChanged();
}

void StatisticsStyleControl::on_doubleSpinBoxRangeMax_valueChanged(double arg1)
{
  if (currentItem->colorRange && currentItem->visualizationType==colorRangeType)
  {
    currentItem->colorRange->rangeMax = (int)arg1;
  }
  else if (currentItem->defaultColorRange && currentItem->visualizationType==defaultColorRangeType)
  {
    currentItem->defaultColorRange->rangeMax = (int)arg1;
  }
  else
  {
    assert(0);
  }
  emit StyleChanged();
}

void StatisticsStyleControl::on_checkBoxMapColor_stateChanged(int arg1)
{
    currentItem->mapVectorToColor=(bool)arg1;
    emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxDataHeadStyle_currentIndexChanged(int index)
{
    currentItem->arrowHead=(arrowHead_t)index;
    emit StyleChanged();
}
