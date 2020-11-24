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
#include <QVector3D>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector2D>
#include <QBasicTimer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLDebugLogger>

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
    void updateFrame(const int frameWidth, const int frameHeight, const YUV_Internals::yuvPixelFormat PxlFormat, const QByteArray &textureData); // display a new frame

    // change frame format (width, height, ...
    void updateFormat(int frameWidth, int frameHeight, YUV_Internals::yuvPixelFormat PxlFormat);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QBasicTimer timer;
    QOpenGLShaderProgram program;

    QMatrix4x4 projection;


    QOpenGLDebugLogger *logger;

    int m_frameWidth;
    int m_frameHeight;
    int m_componentLength;

    YUV_Internals::yuvPixelFormat m_pixelFormat;
    int m_swapUV;

    // Vertex Array Object (VAO) is an OpenGL Object that stores all of the state needed to supply vertex data.
    // It stores the format of the vertex data as well as the Buffer Objects  (vertex coords, indices, texture coords)
    // providing the vertex data arrays.
    QOpenGLVertexArrayObject m_vao;

    std::vector<QVector3D> m_videoFrameTriangles_vertices;  // will hold the frame corners in 3d coordinates
    QOpenGLBuffer m_vertices_Vbo; // will hold the frame corners in 3d coordinates, once transmitted to GPU

    // each vector (of 3 unsigned int) holds the indices for one triangle in the video frame
    std::vector<unsigned int> m_videoFrameTriangles_indices;
    QOpenGLBuffer m_vertice_indices_Vbo; // will hold the indices, once transmitted to GPU

    // Vector which will the texture coordinates which maps the texture (the video data) to the frame's vertices.
    std::vector<float> m_videoFrameDataPoints_Luma; // will hold the frame corners in texture coordinates
    QOpenGLBuffer m_textureLuma_coordinates_Vbo; // will hold the frame corners in texture coordinates, once transmitted to GPU

    // for uploading the video frame as a texture:    
    QScopedPointer<QOpenGLTexture> m_texture_Ydata;
    QScopedPointer<QOpenGLTexture> m_texture_Udata;
    QScopedPointer<QOpenGLTexture> m_texture_Vdata;

    // texture format and pixel type depend on the input bit depth, keep track of them
    QOpenGLTexture::TextureFormat m_openGLTextureFormat;
    QOpenGLTexture::PixelType m_openGLTexturePixelType;

    QOpenGLShaderProgram *m_program;

    void computeFrameVertices();
    void computeFrameMesh();
    void computeLumaTextureCoordinates();
};

#endif // OPENGLVIEWWIDGET_H
