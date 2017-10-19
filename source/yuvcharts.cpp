#include "yuvcharts.h"

YUVCharts::YUVCharts(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget)
{
  this->mNoDataToShowWidget = aNoDataToShowWidget;
  this->mDataIsLoadingWidget = aDataIsLoadingWidget;
}

QWidget* YUVBarChart::createChart(const ChartOrderBy aOrderBy, playlistItem *aItem)
{
  // getting sorted data from the item, in our specifed range
  return this->mNoDataToShowWidget;
}
