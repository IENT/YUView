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
  QFrame::paintEvent(event);

  // Draw
  QPainter painter(this);
  int fw = frameWidth();
  QRect r = rect();
  QRect drawRect = QRect(r.left()+fw, r.top()+fw, r.width()-fw*2, r.height()-fw*2);

  if (renderRangeValues)
  {
    // How high is one digit when drawing it?
    QFontMetrics metrics(font());
    int h = metrics.size(0, "0").height();

    // Draw two small lines (at the left and at the right)
    const int lineHeight = 3;
    int y = drawRect.height() - h;
    painter.drawLine(drawRect.left(), y, drawRect.left(), y-lineHeight);
    painter.drawLine(drawRect.right(), y, drawRect.right(), y-lineHeight);
    painter.drawLine(drawRect.center().x(), y, drawRect.center().x(), y-lineHeight);

    // Draw the text values left and right
    painter.drawText(drawRect.left(), y, drawRect.width(), h, Qt::AlignLeft,  QString::number(customRange.rangeMin));
    painter.drawText(drawRect.left(), y, drawRect.width(), h, Qt::AlignRight, QString::number(customRange.rangeMax));
    // Draw the middle value
    int middleValue = (customRange.rangeMax - customRange.rangeMin) / 2 + customRange.rangeMin;
    painter.drawText(drawRect.left(), y, drawRect.width(), h, Qt::AlignHCenter, QString::number(middleValue));

    // Modify the draw rect, so that the actual range is drawn over the values
    drawRect.setHeight( drawRect.height() - h - lineHeight );
  }

  if (renderRange)
  {
    // Create a temporary color range. We scale this range to the range from 0 to 1. 
    // This is the range of values that we will draw.
    ColorRange cRange = customRange;
    cRange.rangeMin = 0;
    cRange.rangeMax = 1;

    // Split the rect into lines with width of 1 pixel
    const int y0 = drawRect.bottom();
    const int y1 = drawRect.top();
    for (int x=drawRect.left(); x <= drawRect.right(); x++)
    {
      // For every line (1px width), draw a line.
      // Set the right color
      QColor c = cRange.getColor( float(x-drawRect.left())/(drawRect.width()) );
      if (isEnabled())
        painter.setPen(c);
      else
      {
        int gray = 64 + qGray(c.rgb()) / 2;
        painter.setPen(QColor(gray, gray, gray));
      }
      // Draw the line
      painter.drawLine(x, y0, x, y1);
    }
  }
  else
  {
    if (isEnabled())
      painter.fillRect(drawRect, plainColor);
    else
    {
      int gray = 64 + qGray(plainColor.rgb()) / 2;
      painter.fillRect(drawRect, QColor(gray, gray, gray));
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

  // The default custom range is black to blue
  ui->frameMinColor->setPlainColor(QColor(0, 0, 0));
  ui->frameMaxColor->setPlainColor(QColor(0, 0, 255));

  // For the preview, render the value range
  ui->frameDataColor->setRenderRangeValues(true);
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
    ui->colorFrameVectorColor->setPlainColor(currentItem->vectorPen.color());

  }
  else if (currentItem->visualizationType==colorRangeType)
  {
    ui->groupBoxVector->hide();
    ui->groupBoxBlockData->show();
    
    ui->spinBoxRangeMin->setValue((double)currentItem->colorRange.rangeMin);
    ui->spinBoxRangeMax->setValue((double)currentItem->colorRange.rangeMax);
    ui->frameDataColor->setColorRange(currentItem->colorRange);

    // Update all the values in the block data controls.
    ui->comboBoxDataColorMap->setCurrentIndex((int)currentItem->colorRange.getTypeId());
    if (currentItem->colorRange.getTypeId() == 0)
    {
      // The color range is a custom range
      // Enable/setup the controls for the minimum and maximum color
      ui->frameMinColor->setEnabled(true);
      ui->pushButtonEditMinColor->setEnabled(true);
      ui->frameMinColor->setPlainColor(currentItem->colorRange.minColor);
      ui->frameMaxColor->setEnabled(true);
      ui->pushButtonEditMaxColor->setEnabled(true);
      ui->frameMaxColor->setPlainColor(currentItem->colorRange.maxColor);
      
    }
    else
    {
      // The color range is one of the predefined default color maps
      // Disable the color min/max controls
      ui->frameMinColor->setEnabled(false);
      ui->pushButtonEditMinColor->setEnabled(false);
      ui->frameMaxColor->setEnabled(false);
      ui->pushButtonEditMaxColor->setEnabled(false);      
    }
  }
  else
  {
    ui->groupBoxVector->hide();
    ui->groupBoxBlockData->hide();
  }

  ui->frameGridColor->setPlainColor(currentItem->gridPen.color());
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
  currentItem->renderData = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_comboBoxDataColorMap_currentIndexChanged(int index)
{
  // Enable/Disable the color min/max controls
  ui->frameMinColor->setEnabled(index == 0);
  ui->pushButtonEditMinColor->setEnabled(index == 0);
  ui->frameMaxColor->setEnabled(index == 0);
  ui->pushButtonEditMaxColor->setEnabled(index == 0);

  if (index == 0)
    // A custom range is selected
    currentItem->colorRange = ColorRange(currentItem->colorRange.rangeMin, ui->frameMinColor->getPlainColor(), currentItem->colorRange.rangeMax, ui->frameMaxColor->getPlainColor());
  else
    // One of the preset ranges is selected
    currentItem->colorRange = ColorRange(index, currentItem->colorRange.rangeMin, currentItem->colorRange.rangeMax);

  ui->frameDataColor->setColorRange(currentItem->colorRange);
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameMinColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select color range minimum"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentItem->colorRange.minColor != newColor)
  {
    currentItem->colorRange.minColor = newColor;
    ui->frameDataColor->setColorRange(currentItem->colorRange);
    ui->frameMinColor->setPlainColor(newColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_frameMaxColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select color range maximum"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && currentItem->colorRange.maxColor != newColor)
  {
    currentItem->colorRange.maxColor = newColor;
    ui->frameDataColor->setColorRange(currentItem->colorRange);
    ui->frameMaxColor->setPlainColor(newColor);
    emit StyleChanged();
  }
}

void StatisticsStyleControl::on_spinBoxRangeMin_valueChanged(int val)
{
  currentItem->colorRange.rangeMin = val;
  ui->frameDataColor->setColorRange(currentItem->colorRange);
  emit StyleChanged();
}

void StatisticsStyleControl::on_spinBoxRangeMax_valueChanged(int val)
{
  currentItem->colorRange.rangeMax = val;
  ui->frameDataColor->setColorRange(currentItem->colorRange);
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

void StatisticsStyleControl::on_colorFrameVectorColor_clicked()
{
}

void StatisticsStyleControl::on_groupBoxGrid_clicked(bool check)
{
  currentItem->renderGrid = check;
  emit StyleChanged();
}

void StatisticsStyleControl::on_frameGridColor_clicked()
{
  QColor newColor = QColorDialog::getColor(currentItem->gridPen.color(), this, tr("Select grid color"), QColorDialog::ShowAlphaChannel);
  if (newColor.isValid() && newColor != currentItem->gridPen.color())
  {
    currentItem->gridPen.setColor(newColor);
    ui->frameGridColor->setPlainColor(currentItem->gridPen.color());
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
