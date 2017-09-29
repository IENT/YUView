#include "yuvcharts.h"

YUVCharts::YUVCharts(QWidget* aNoDataToShowWidget, QWidget* aDataIsLoadingWidget)
{
  this->mNoDataToShowWidget = aNoDataToShowWidget;
  this->mDataIsLoadingWidget = aDataIsLoadingWidget;
}

QWidget* YUVBarChart::createChart(QList<collectedData> *aSortedData, const ChartOrderBy aOrderBy, playlistItem *aItem)
{
  return this->mNoDataToShowWidget;
}
