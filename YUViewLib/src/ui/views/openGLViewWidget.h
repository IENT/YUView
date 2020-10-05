/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#ifndef OPENGLVIEWWIDGET_H
#define OPENGLVIEWWIDGET_H


#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector2D>
#include <QBasicTimer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLDebugLogger>

// Include GLM
#include <glm/glm.hpp>

#include <video/yuvPixelFormat.h>



class OpenGLViewWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
//    using QOpenGLWidget::QOpenGLWidget;
    OpenGLViewWidget(QWidget *parent = 0);
    ~OpenGLViewWidget();

public slots:
    void handleOepnGLLoggerMessages( QOpenGLDebugMessage message );
    void updateFrame(const QByteArray &textureData); // display a new frame
    // change frame format (width, height, ...
    void updateFormat(int frameWidth, int frameHeight, YUV_Internals::yuvPixelFormat PxlFormat);

protected:

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void initShaders();
    void initTextures();

private:
    void computeFrameVertices(int frameWidth, int frameHeight);
    void computeFrameMesh(int frameWidth, int frameHeight);
    void computeLumaTextureCoordinates(int frameWidth, int frameHeight);
    void computeChromaTextureCoordinates(int frameWidth, int frameHeight);
    void setupVertexAttribs();
    void setupMatrices();
    glm::mat4 getProjectionFromCamCalibration(glm::mat3 &calibrationMatrix, float clipFar, float clipNear);

    QBasicTimer timer;
    QOpenGLShaderProgram program;

    QOpenGLTexture *texture = nullptr;

    QMatrix4x4 projection;

    QVector2D mousePressPosition;
    QVector3D rotationAxis;
    qreal angularSpeed = 0;
    QQuaternion rotation;


    QOpenGLDebugLogger *logger;

    int m_frameWidth;
    int m_frameHeight;
    int m_componentLength;

    YUV_Internals::yuvPixelFormat m_pixelFormat;
    int m_swapUV;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vertices_Vbo;
    QOpenGLBuffer m_vertice_indices_Vbo;
    //QOpenGLBuffer m_texture_coordinates_Vbo;
    QOpenGLBuffer m_textureLuma_coordinates_Vbo;
    QOpenGLBuffer m_textureChroma_coordinates_Vbo;
    //QOpenGLBuffer m_depth_Vbo;


    std::shared_ptr<QOpenGLTexture> m_texture_Ydata;
    std::shared_ptr<QOpenGLTexture> m_texture_Udata;
    std::shared_ptr<QOpenGLTexture> m_texture_Vdata;
    QImage::Format m_textureFormat;
    QVector<GLfloat> m_vertices_data;

    QOpenGLShaderProgram *m_program;

    int m_matMVP_Loc;
    // handles for texture, vertices and depth
    int m_vertices_Loc;
    int m_textureLuma_Loc;
    int m_textureChroma_Loc;
    glm::mat4 m_MVP;


    std::vector<glm::vec3> m_videoFrameTriangles_vertices;
    // each vector (of 3 unsigned int) holds the indices for one triangle in the video frame
    std::vector<unsigned int> m_videoFrameTriangles_indices;
    // Vector which will the texture coordinates which maps the texture (the video data) to the frame's vertices.
    std::vector<float> m_videoFrameDataPoints_Luma;

    std::vector<float> m_videoFrameDataPoints_Chroma;

};

#endif // OPENGLVIEWWIDGET_H
