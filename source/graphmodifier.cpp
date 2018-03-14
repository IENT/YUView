#include "graphmodifier.h"
#include <QtDataVisualization/qcategory3daxis.h>
#include <QtDataVisualization/qvalue3daxis.h>
#include <QtDataVisualization/qbardataproxy.h>
#include <QtDataVisualization/q3dscene.h>
#include <QtDataVisualization/q3dcamera.h>
#include <QtDataVisualization/qbar3dseries.h>
#include <QtDataVisualization/q3dtheme.h>
#include <QtCore/QTime>
#include <QtWidgets/QComboBox>
#include <QtCore/qmath.h>

using namespace QtDataVisualization;

const QString celsiusString = QString(QChar(0xB0)) + "C";

GraphModifier3DBars::GraphModifier3DBars(Q3DBars* aBarGraph)
  : mGraph(aBarGraph),
    mPrimarySeries(new QBar3DSeries),
    mRotationX(0.0f),
    mRotationY(0.0f),
    mMinval(0.0f),
    mMaxval(10.0f),
    mAmountAxis(new QValue3DAxis),
    mXValueAxis(new QCategory3DAxis),
    mYValueAxis(new QCategory3DAxis)
{
  this->mGraph->setShadowQuality(QAbstract3DGraph::ShadowQualitySoftMedium);
  this->mGraph->activeTheme()->setBackgroundEnabled(false);
  this->mGraph->activeTheme()->setFont(QFont("Times New Roman", 10));
  this->mGraph->activeTheme()->setLabelBackgroundEnabled(true);
  this->mGraph->setMultiSeriesUniform(true);

  this->mGraph->setBarThickness(0.5f);
  this->mGraph->setBarSpacing(QSizeF(2.0, 2.0));

  // necessary, that we can display the title of the axis, the stringlists includes at minimum of one element
  this->mCategoriesY << "";
  this->mCategoriesX << "";

  // generate default settings
  this->mAmountAxis->setTitle("Amount (z-axis)");
  this->mAmountAxis->setRange(mMinval, mMaxval);
  this->mAmountAxis->setLabelFormat(QString(QStringLiteral("%.0f ")));
  this->mAmountAxis->setLabelAutoRotation(30.0f);
  this->mAmountAxis->setTitleVisible(true);

  this->mXValueAxis->setTitle("x-value (x-axis)");
  this->mXValueAxis->setLabelAutoRotation(30.0f);
  this->mXValueAxis->setTitleVisible(true);

  this->mYValueAxis->setTitle("y-value (y-axis)");
  this->mYValueAxis->setLabelAutoRotation(30.0f);
  this->mYValueAxis->setTitleVisible(true);

  this->mGraph->setValueAxis(mAmountAxis);
  this->mGraph->setRowAxis(mXValueAxis);
  this->mGraph->setColumnAxis(mYValueAxis);

  this->mPrimarySeries->setItemLabelFormat(QStringLiteral("(x:@rowLabel/y:@colLabel) - amount:@valueLabel"));
  this->mPrimarySeries->setMesh(QAbstract3DSeries::MeshBevelBar);
  this->mPrimarySeries->setMeshSmooth(true);

  this->mGraph->addSeries(this->mPrimarySeries);

  this->changePresetCamera();

  // Set up property animations for zooming to the selected bar
  Q3DCamera* camera     = mGraph->scene()->activeCamera();
  this->mDefaultAngleX  = camera->xRotation();
  this->mDefaultAngleY  = camera->yRotation();
  this->mDefaultZoom    = camera->zoomLevel();
  this->mDefaultTarget  = camera->target();

  this->mAnimationCameraX.setTargetObject(camera);
  this->mAnimationCameraY.setTargetObject(camera);
  this->mAnimationCameraZoom.setTargetObject(camera);
  this->mAnimationCameraTarget.setTargetObject(camera);

  this->mAnimationCameraX.setPropertyName("xRotation");
  this->mAnimationCameraY.setPropertyName("yRotation");
  this->mAnimationCameraZoom.setPropertyName("zoomLevel");
  this->mAnimationCameraTarget.setPropertyName("target");

  int duration = 1700;
  this->mAnimationCameraX.setDuration(duration);
  this->mAnimationCameraY.setDuration(duration);
  this->mAnimationCameraZoom.setDuration(duration);
  this->mAnimationCameraTarget.setDuration(duration);

  // The zoom always first zooms out above the graph and then zooms in
  qreal zoomOutFraction = 0.3;
  this->mAnimationCameraX.setKeyValueAt(zoomOutFraction, QVariant::fromValue(0.0f));
  this->mAnimationCameraY.setKeyValueAt(zoomOutFraction, QVariant::fromValue(90.0f));
  this->mAnimationCameraZoom.setKeyValueAt(zoomOutFraction, QVariant::fromValue(50.0f));
  this->mAnimationCameraTarget.setKeyValueAt(zoomOutFraction, QVariant::fromValue(QVector3D(0.0f, 0.0f, 0.0f)));
}

GraphModifier3DBars::~GraphModifier3DBars()
{
    delete this->mGraph;
}

void GraphModifier3DBars::applyDataToGraph(chartSettingsData aSettings)
{
  this->mSettings = aSettings;

  this->mCategoriesX.clear();
  this->mCategoriesY.clear();

  //holder for the maximum amount for the amount-axis
  float maxAmount = -1.0f;

  // Set up data
  // Create data arrays
  QBarDataArray* dataSet = new QBarDataArray;
  QBarDataRow* dataRow;

  dataSet->reserve(this->mSettings.m3DData.keys().count());

  foreach (int x, this->mSettings.m3DData.keys())
  {
    this->mCategoriesX << QString::number(x);

    QMap<int, double> row = this->mSettings.m3DData.value(x);

    dataRow = new QBarDataRow(row.keys().count());
    int posY = 0; // this is necessary for the position in the row. The value of y is not always the position in the row
    foreach (int y, row.keys())
    {
      this->mCategoriesY << QString::number(y);

      // Add data to the row
      double amount = this->mSettings.m3DData[x][y];
      (*dataRow)[posY++].setValue(amount);

      // getting the maximun amount to resize the axis
      if(amount > maxAmount)
          maxAmount = amount;
    }

    dataSet->append(dataRow);
  }

  // Add data to the data proxy (the data proxy assumes ownership of it)
  this->mPrimarySeries->dataProxy()->resetArray(dataSet, mCategoriesX, mCategoriesY);

  // setting the new maxval
  if(maxAmount != -1.0f)
    this->mMaxval = maxAmount;

  // setting the new range
  this->mAmountAxis->setRange(this->mMinval, this->mMaxval);

  if(this->mMaxval != (int)this->mMaxval) // check if number has precision
    this->mAmountAxis->setLabelFormat(QString(QStringLiteral("%.4f ")));
  else
    this->mAmountAxis->setLabelFormat(QString(QStringLiteral("%.0f ")));

  int segments = ((int)this->mMaxval) % 2;
  this->mAmountAxis->setSubSegmentCount(segments);
}

void GraphModifier3DBars::changePresetCamera()
{
  this->mAnimationCameraX.stop();
  this->mAnimationCameraY.stop();
  this->mAnimationCameraZoom.stop();
  this->mAnimationCameraTarget.stop();

  // Restore camera target in case animation has changed it
  this->mGraph->scene()->activeCamera()->setTarget(QVector3D(0.0f, 0.0f, 0.0f));

  // static is necessary, so we can look next time which position the camera has
  static int preset = Q3DCamera::CameraPresetFront;

  this->mGraph->scene()->activeCamera()->setCameraPreset((Q3DCamera::CameraPreset)preset);

  if (++preset > Q3DCamera::CameraPresetDirectlyBelow)
      preset = Q3DCamera::CameraPresetFrontLow;
}

void GraphModifier3DBars::changeLabelRotation(int aRotation)
{
  this->mAmountAxis->setLabelAutoRotation(float(aRotation));
  this->mYValueAxis->setLabelAutoRotation(float(aRotation));
  this->mXValueAxis->setLabelAutoRotation(float(aRotation));
}

void GraphModifier3DBars::zoomToSelectedBar()
{
  // stop all cameras
  this->mAnimationCameraX.stop();
  this->mAnimationCameraY.stop();
  this->mAnimationCameraZoom.stop();
  this->mAnimationCameraTarget.stop();

  // get actual active camera position
  Q3DCamera* camera       = this->mGraph->scene()->activeCamera();

  float currentX          = camera->xRotation();
  float currentY          = camera->yRotation();
  float currentZoom       = camera->zoomLevel();
  QVector3D currentTarget = camera->target();

  this->mAnimationCameraX.setStartValue(QVariant::fromValue(currentX));
  this->mAnimationCameraY.setStartValue(QVariant::fromValue(currentY));
  this->mAnimationCameraZoom.setStartValue(QVariant::fromValue(currentZoom));
  this->mAnimationCameraTarget.setStartValue(QVariant::fromValue(currentTarget));

  // get the selected bar or mark as invalid
  QPoint selectedBar = mGraph->selectedSeries()
                                    ? this->mGraph->selectedSeries()->selectedBar()
                                    : QBar3DSeries::invalidSelectionPosition();

  if (selectedBar != QBar3DSeries::invalidSelectionPosition())
  {
    // Normalize selected bar position within axis range to determine target coordinates
    QVector3D endTarget;
    float xMin    = this->mGraph->columnAxis()->min();
    float xRange  = this->mGraph->columnAxis()->max() - xMin;
    float zMin    = this->mGraph->rowAxis()->min();
    float zRange  = this->mGraph->rowAxis()->max() - zMin;
    endTarget.setX((selectedBar.y() - xMin) / xRange * 2.0f - 1.0f);
    endTarget.setZ((selectedBar.x() - zMin) / zRange * 2.0f - 1.0f);

    // Rotate the camera so that it always points approximately to the graph center
    qreal endAngleX = 90.0 - qRadiansToDegrees(qAtan(qreal(endTarget.z() / endTarget.x())));

    if (endTarget.x() > 0.0f)
        endAngleX -= 180.0f;

    // get selected barvalue
    float barValue = this->mGraph->selectedSeries()->dataProxy()->itemAt(selectedBar.x(), selectedBar.y())->value();
    float endAngleY = barValue >= 0.0f ? 30.0f : -30.0f;

    if (this->mGraph->valueAxis()->reversed())
        endAngleY *= -1.0f;

    this->mAnimationCameraX.setEndValue(QVariant::fromValue(float(endAngleX)));
    this->mAnimationCameraY.setEndValue(QVariant::fromValue(endAngleY));
    this->mAnimationCameraZoom.setEndValue(QVariant::fromValue(250));
    this->mAnimationCameraTarget.setEndValue(QVariant::fromValue(endTarget));
  }
  else // No selected bar, so return to the default view
  {
    this->mAnimationCameraX.setEndValue(QVariant::fromValue(this->mDefaultAngleX));
    this->mAnimationCameraY.setEndValue(QVariant::fromValue(this->mDefaultAngleY));
    this->mAnimationCameraZoom.setEndValue(QVariant::fromValue(this->mDefaultZoom));
    this->mAnimationCameraTarget.setEndValue(QVariant::fromValue(this->mDefaultTarget));
  }

  // start the camera, because at the beginning we stop it
  this->mAnimationCameraX.start();
  this->mAnimationCameraY.start();
  this->mAnimationCameraZoom.start();
  this->mAnimationCameraTarget.start();
}

void GraphModifier3DBars::rotateX(int aRotation)
{
  this->mRotationX = aRotation;
  this->mGraph->scene()->activeCamera()->setCameraPosition(mRotationX, mRotationY);
}

void GraphModifier3DBars::rotateY(int aRotation)
{
  this->mRotationY = aRotation;
  this->mGraph->scene()->activeCamera()->setCameraPosition(mRotationX, mRotationY);
}
