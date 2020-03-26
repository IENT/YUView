/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "bitstreamAnalysisBitratePlot.h"

#include <QtWidgets/QVBoxLayout>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QLineSeries>

QT_CHARTS_USE_NAMESPACE

const int scrollBarScale = 10;

BitrateBarChartCallout::BitrateBarChartCallout() :
  QGraphicsItem()
{
}

void BitrateBarChartCallout::setChart(QtCharts::QChart *chart)
{
  this->setParentItem(chart);
  this->chart = chart;
}

QRectF BitrateBarChartCallout::boundingRect() const
{
  if (this->parentItem() == nullptr || this->chart == nullptr)
    return {};

  QPointF anchor = mapFromParent(this->chart->mapToPosition(this->anchor));
  QRectF rect;
  rect.setLeft(qMin(this->rect.left(), anchor.x()));
  rect.setRight(qMax(this->rect.right(), anchor.x()));
  rect.setTop(qMin(this->rect.top(), anchor.y()));
  rect.setBottom(qMax(this->rect.bottom(), anchor.y()));
  return rect;
}

void BitrateBarChartCallout::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  Q_UNUSED(option)
  Q_UNUSED(widget)

  painter->setBrush(QColor(255, 255, 255));
  painter->drawRect(this->rect);
  painter->drawText(this->textRect, this->text);
}

void BitrateBarChartCallout::setTextAndAnchor(const QString &text, QPointF anchor)
{
  this->text = text;
  QFont defaultFont;
  QFontMetrics metrics(defaultFont);
  this->textRect = metrics.boundingRect(QRect(0, 0, 150, 150), Qt::AlignLeft, this->text);
  prepareGeometryChange();
  this->rect = this->textRect.adjusted(-5, -5, 5, 5);
  this->anchor = anchor;
  setPos(this->chart->mapToPosition(anchor) + QPoint(-this->textRect.width() / 2, -this->textRect.height() - 10));
}

BitrateBarChart::BitrateBarChart(QWidget *parent)
  : QWidget(parent)
{
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  this->chartView = new QChartView(this);
  this->chartView->setRenderHint(QPainter::Antialiasing);
  this->chartView->setMinimumSize(640, 480);
  mainLayout->addWidget(this->chartView);

  this->chartView->setChart(&this->chart);
  this->currentTooltip.setChart(&this->chart);

  this->scrollBar = new QScrollBar(Qt::Horizontal, this);
  mainLayout->addWidget(this->scrollBar);
  connect(this->scrollBar, &QAbstractSlider::valueChanged, this, &BitrateBarChart::onScrollBarValueChanged);
}

void BitrateBarChart::setModel(parserCommon::BitrateItemModel *model)
{
  this->model = model;
  
  // Clear the current chart
  this->chart.removeAllSeries();
  if (!this->barMapper.isNull())
  {
    delete this->barMapper;
    this->barMapper.clear();
  }
  if (!this->lineModelMapper.isNull())
  {
    delete this->lineModelMapper;
    this->lineModelMapper.clear();
  }
  if (!this->axisX.isNull())
  {
    this->chart.removeAxis(this->axisX);
    delete this->axisX;
    this->axisX.clear();
  }
  if (!this->axisY.isNull())
  {
    this->chart.removeAxis(this->axisY);
    delete this->axisY;
    this->axisY.clear();
  }

  if (!this->model)
    return;

  Q_ASSERT(this->barMapper.isNull());
  Q_ASSERT(this->lineModelMapper.isNull());
  Q_ASSERT(this->axisX.isNull());
  Q_ASSERT(this->axisY.isNull());

  QStackedBarSeries *barSeries = new QStackedBarSeries;
  barSeries->setBarWidth(1.0);
  this->chart.setAnimationOptions(QChart::NoAnimation);

  this->updateScrollBarRange();

  this->barMapper = new QVBarModelMapper(this);
  this->barMapper->setFirstBarSetColumn(2);
  this->barMapper->setLastBarSetColumn(3);
  this->barMapper->setRowCount(this->model->rowCount());
  this->barMapper->setSeries(barSeries);
  this->barMapper->setModel(this->model);
  this->chart.addSeries(barSeries);

  QLineSeries *lineSeries = new QLineSeries;
  lineSeries->setName("Average");
  this->lineModelMapper = new QVXYModelMapper(this);
  this->lineModelMapper->setXColumn(0);
  this->lineModelMapper->setYColumn(1);
  this->lineModelMapper->setSeries(lineSeries);
  this->lineModelMapper->setModel(this->model);
  this->chart.addSeries(lineSeries);

  this->axisX = new QValueAxis();
  this->axisX->setLabelFormat("%.0f");
  this->axisY = new QValueAxis();
  this->chart.addAxis(this->axisX, Qt::AlignBottom);
  this->chart.addAxis(this->axisY, Qt::AlignLeft);
  barSeries->attachAxis(this->axisX);
  lineSeries->attachAxis(this->axisX);
  barSeries->attachAxis(this->axisY);
  lineSeries->attachAxis(this->axisY);
  connect(barSeries, &QAbstractBarSeries::hovered, this, &BitrateBarChart::tooltip);

  this->onScrollBarValueChanged(0);

  connect(this->model, &QAbstractItemModel::rowsInserted, this, &BitrateBarChart::onRowsInserted);
}

void BitrateBarChart::onScrollBarValueChanged(int value)
{
  Q_UNUSED(value);
  this->updateAxis();
}

void BitrateBarChart::tooltip(bool status, int index, QBarSet *barset)
{
  Q_UNUSED(barset);

  if (status)
  {
    if (this->model)
    {
      QString itemInfoText = this->model->getItemInfoText(index);
      this->currentTooltip.setTextAndAnchor(itemInfoText, QPointF(double(index), 3));
      this->currentTooltip.setZValue(11);
      this->currentTooltip.show();
    }
  }
  else
    this->currentTooltip.hide();
}

void BitrateBarChart::onRowsInserted(const QModelIndex &parent, int first, int last)
{
  Q_UNUSED(parent);
  Q_UNUSED(first);
  Q_UNUSED(last);

  if (!this->barMapper.isNull())
  {
    this->axisY->setMax(this->model->getMaximumBitrateValue() * 1.05);
    this->barMapper->setRowCount(this->model->rowCount());
    this->updateScrollBarRange();
  }
}

void BitrateBarChart::resizeEvent(QResizeEvent *event)
{
  Q_UNUSED(event);
  this->updateScrollBarRange();
  this->updateAxis();
}

void BitrateBarChart::updateAxis()
{
  if (this->axisX.isNull())
    return;

  const auto plotWidth = this->chart.plotArea().width();
  double barsVisible = plotWidth / 100 * this->barsPerWidthOf100Pixels;

  auto currentScrollBarValue = this->scrollBar->value();
  double v = double(currentScrollBarValue) / scrollBarScale;
  this->axisX->setRange(v - 0.5, v + barsVisible - 0.5);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
  this->axisX->setTickType(QValueAxis::TicksDynamic);
  this->axisX->setTickInterval(5.0);
#endif
}

void BitrateBarChart::updateScrollBarRange()
{
  if (this->barMapper.isNull())
    return;

  const auto plotWidth = this->chart.plotArea().width();
  const double barsVisible = plotWidth / 100 * this->barsPerWidthOf100Pixels;

  auto nrRows = this->barMapper->model()->rowCount();
  auto maxValue = scrollBarScale * nrRows - int(barsVisible * scrollBarScale);
  if (maxValue <= 0)
    this->scrollBar->setEnabled(false);
  else
  {
    QSignalBlocker scrollBarSignalBlocker(this->scrollBar);
    this->scrollBar->setEnabled(true);
    this->scrollBar->setMinimum(0);
    this->scrollBar->setMaximum(maxValue);
  }
}
