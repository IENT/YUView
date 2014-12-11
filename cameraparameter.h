#ifndef CAMERAPARAMETER_H
#define CAMERAPARAMETER_H

class CameraParameter
{
public:
    CameraParameter();
    float fx, fy, Z_near, Z_far;
    int width, height;
    float position[3];
    float rotation[9];
    int viewNumber; // -1 means unset
};

#endif // CAMERAPARAMETER_H
