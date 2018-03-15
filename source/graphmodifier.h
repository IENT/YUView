#ifndef GRAPHMODIFIER_H
#define GRAPHMODIFIER_H

#include "chartHandlerTypedef.h"

#include <QtDataVisualization/q3dbars.h>
#include <QtDataVisualization/qbardataproxy.h>
#include <QtDataVisualization/qabstract3dseries.h>

#include <QtGui/QFont>
#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QPointer>
#include <QtCore/QPropertyAnimation>

using namespace QtDataVisualization;

/**
 * @brief The GraphModifier3DBars class
 * the GraphModifier3DBars-Class will modify the given 3D-chart. It can modify the displaying and the dataset
 */
class GraphModifier3DBars : public QObject
{
  Q_OBJECT
public:
  /**
   * @brief GraphModifier3DBars
   * default-constructor
   *
   * @param bargraph
   * chart
   *
   * @param aSettings
   * necessary settings to set the data
   */
  explicit GraphModifier3DBars(Q3DBars* aBarGraph);

  /**
  * @brief
  * default-destructor
  */
  ~GraphModifier3DBars();

  /**
   * @brief applyDataToGraph
   * sets the data to the graph
   */
  void applyDataToGraph(chartSettingsData aSettings);

  /**
   * @brief changePresetCamera
   * changes the camera settings
   */
  void changePresetCamera();

public slots:
  /**
   * @brief rotateX
   * slot for slider to rotate the x-axis
   *
   * @param aRotation
   * value to rotate
   */
  void rotateX(int aRotation);

  /**
   * @brief rotateY
   * slot for slider to rotate the y-axis
   *
   * @param aRotation
   * value to rotate
   */
  void rotateY(int aRotation);

  /**
   * @brief changeLabelRotation
   * slot for slider to rotate the label-rotation
   *
   * @param aRotation
   * value to rotate
   */
  void changeLabelRotation(int aRotation);

  /**
   * @brief zoomToSelectedBar
   * slot for QPushButton to zoom into an selected bar
   * if no bar is selected, an default-angle will display
   */
  void zoomToSelectedBar();

private:
    Q3DBars* mGraph;
    QBar3DSeries* mPrimarySeries;
    chartSettingsData mSettings;
    float mRotationX;
    float mRotationY;
    float mMinval;
    float mMaxVal;
    QStringList mCategoriesY;
    QStringList mCategoriesX;
    QValue3DAxis* mAmountAxis;
    QCategory3DAxis* mXValueAxis;
    QCategory3DAxis* mYValueAxis;
    QPropertyAnimation mAnimationCameraX;
    QPropertyAnimation mAnimationCameraY;
    QPropertyAnimation mAnimationCameraZoom;
    QPropertyAnimation mAnimationCameraTarget;
    float mDefaultAngleX;
    float mDefaultAngleY;
    float mDefaultZoom;
    QVector3D mDefaultTarget;
};

#endif // GRAPHMODIFIER_H
