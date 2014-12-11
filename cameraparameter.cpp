#include "cameraparameter.h"

CameraParameter::CameraParameter():
    fx(0),fy(0),Z_near(0),Z_far(0),width(0),height(0), viewNumber(-1)
{
    position[0] = 0;
    position[1] = 0;
    position[2] = 0;

    rotation[0] = 0;
    rotation[1] = 0;
    rotation[2] = 0;
    rotation[3] = 0;
    rotation[4] = 0;
    rotation[5] = 0;
    rotation[6] = 0;
    rotation[7] = 0;
    rotation[8] = 0;
}
