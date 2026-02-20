/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "imageviewwidget.h"

#include <QDebug>
#include <QMessageBox>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <cmath>
#include <cstring>

#if defined(QT_OPENGL_ES)
#define USE_GLES 1
#else
#undef USE_GLES
#endif

static const char *vertexShaderSource =
#ifdef USE_GLES
    "#version 320 es\n"
#else
    "#version 410 core\n"
#endif
    "layout(location = 0) in vec2 position;\n"
    "out vec2 texCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    texCoord = vec2(position.x * 0.5 + 0.5, 1.0 - position.y * 0.5 - 0.5);\n"
    "}\n";

static const char *fragmentShaderSource =
#ifdef USE_GLES
    "#version 320 es\n"
    "precision highp float;\n"
#else
    "#version 410 core\n"
    "#define lowp\n"
    "#define mediump\n"
    "#define highp\n"
#endif
    "in vec2 texCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D tex;\n"
    "uniform float aspectRatio;\n"
    "uniform vec4 bgColor;\n"
    "uniform lowp float showSaturation;\n"
    "uniform lowp float isGrayscale;\n"
    "uniform float minPixelValue;\n"
    "uniform float maxPixelValue;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec2 sceneCoord = texCoord;\n"
    "    if (aspectRatio > 1.0) {\n"
    "        sceneCoord.x *= aspectRatio;\n"
    "        sceneCoord.x -= (aspectRatio - 1.0) * 0.5;\n"
    "    } else {\n"
    "        sceneCoord.y *= 1.0 / aspectRatio;\n" // Adjust for vertical centering
    "        sceneCoord.y += (1.0 - (1.0 / aspectRatio)) * 0.5;\n"
    "    }\n"
    "    if (sceneCoord.x < 0.0 || sceneCoord.x > 1.0 || "
    "        sceneCoord.y < 0.0 || sceneCoord.y > 1.0) {\n"
    "        FragColor = bgColor;\n" // Bars around image
    "    } else {\n"
    "        vec4 texColor = texture(tex, sceneCoord);\n"
    "        // Apply contrast mapping\n"
    "        if (maxPixelValue > minPixelValue) {\n"
    "            texColor = (texColor - minPixelValue) / (maxPixelValue - minPixelValue);\n"
    "            texColor = clamp(texColor, 0.0, 1.0);\n"
    "        }\n"
    "        if (isGrayscale > 0.5) {\n"
    "            FragColor = vec4(texColor.rrr, 1.0);\n"
    "        } else {\n"
    "            FragColor = texColor;\n"
    "        }\n"
    "        if (showSaturation > 0.5) {\n"
    "            lowp float cVal = dot(FragColor.rgb, vec3(0.299, 0.587, 0.114));\n"
    "            if (cVal >= 0.99)\n"
    "                FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
    "        }\n"
    "    }\n"
    "}\n";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class ImageViewWidget::Private
{
public:
    Private()
        : highlightSaturation(false),
          textureId(0),
          textureWidth(0),
          textureHeight(0),
          textureFormat(GL_RED),
          textureInternalFormat(GL_RED),
          textureType(GL_UNSIGNED_BYTE),
          lastAspectRatio(-1.0f),
          lastHighlightSaturation(false),
          lastBgColor(-1.0f, -1.0f, -1.0f, -1.0f),
          pboIndex(0),
          pboSize(0),
          imageDataChanged(false),
          pixelRangeMin(0),
          pixelRangeMax(65535),
          lastPixelRangeMin(-1),
          lastPixelRangeMax(-1)
    {
        pboIds[0] = pboIds[1] = 0;
    }

    ~Private() = default;

    QVector4D bgColorVec;
    RawImage glImage; // Image data

    bool highlightSaturation;

    // OpenGL resources
    std::unique_ptr<QOpenGLVertexArrayObject> vao;
    std::unique_ptr<QOpenGLBuffer> vbo;
    std::unique_ptr<QOpenGLShaderProgram> shaderProgram;

    // Optimized texture handling
    GLuint textureId;
    int textureWidth, textureHeight;
    GLenum textureFormat;
    GLenum textureInternalFormat;
    GLenum textureType;

    // Cache uniforms to avoid redundant updates
    float lastAspectRatio;
    bool lastHighlightSaturation;
    QVector4D lastBgColor;

    // Pixel Buffer Objects for async texture uploads
    GLuint pboIds[2]; // Double buffering
    int pboIndex;
    size_t pboSize;
    bool imageDataChanged; // Flag to track when new image data needs immediate upload

    // Pixel range for contrast adjustment
    int pixelRangeMin;
    int pixelRangeMax;
    int lastPixelRangeMin;
    int lastPixelRangeMax;

    void setupTextureFormat(int channels, int bytesPerChannel)
    {
        // Set texture type based on bytes per channel
        textureType = (bytesPerChannel == 2) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

        // Set internal format based on channels and bit depth
        switch (channels) {
        case 1:
            textureFormat = GL_RED;
            textureInternalFormat = (bytesPerChannel == 2) ? GL_R16 : GL_R8;
            break;
        case 3:
            textureFormat = GL_RGB;
            textureInternalFormat = (bytesPerChannel == 2) ? GL_RGB16 : GL_RGB8;
            break;
        default: // 4 channels
            textureFormat = GL_RGBA;
            textureInternalFormat = (bytesPerChannel == 2) ? GL_RGBA16 : GL_RGBA8;
            break;
        }
    }
};
#pragma GCC diagnostic pop

ImageViewWidget::ImageViewWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      d(std::make_unique<ImageViewWidget::Private>())
{
    d->bgColorVec = QVector4D(0.46f, 0.46f, 0.46f, 1.0f);
    setWindowTitle("Video");
    QWidget::setMinimumSize(QSize(320, 256));
}

ImageViewWidget::~ImageViewWidget()
{
    // Clean up OpenGL resources when context is still current
    makeCurrent();
    cleanupGL();
    doneCurrent();
}

void ImageViewWidget::cleanupGL()
{
    if (d->textureId != 0) {
        glDeleteTextures(1, &d->textureId);
        d->textureId = 0;
        d->textureWidth = 0;
        d->textureHeight = 0;
    }
    if (d->pboIds[0] != 0) {
        glDeleteBuffers(2, d->pboIds);
        d->pboIds[0] = d->pboIds[1] = 0;
        d->pboSize = 0;
        d->pboIndex = 0;
    }

    // Destroy heap-allocated GL objects (they are bound to the old context)
    d->vao.reset();
    d->vbo.reset();
    d->shaderProgram.reset();
}

void ImageViewWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Clean up any resources from a previous context (e.g., when the widget is
    // reparented by undocking a QDockWidget, a new context is created and
    // initializeGL() is called again)
    cleanupGL();

    auto bgColor = QColor::fromRgb(150, 150, 150);
    float r = ((float)bgColor.darker().red()) / 255.0f;
    float g = ((float)bgColor.darker().green()) / 255.0f;
    float b = ((float)bgColor.darker().blue()) / 255.0f;
    d->bgColorVec = QVector4D(r, g, b, 1.0);
    glClearColor(r, g, b, 1.0f);

    // Compile & link shaders
    d->shaderProgram = std::make_unique<QOpenGLShaderProgram>();

    bool glOkay = true;
    glOkay = d->shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource) && glOkay;
    glOkay = d->shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource) && glOkay;

    if (!d->shaderProgram->link()) {
        glOkay = false;
        qWarning().noquote() << "Unable to link shader program:" << d->shaderProgram->log();
    }

    // Initialize VAO & VBO
    d->vao = std::make_unique<QOpenGLVertexArrayObject>();
    d->vao->create();
    glOkay = glOkay && d->vao->isCreated();
    if (!glOkay) {
        QMessageBox::critical(
            this,
            QStringLiteral("Unable to initialize OpenGL"),
            QStringLiteral(
                "Unable to compiler or link OpenGL shader or initialize vertex array object. Your system needs at "
                "least OpenGL 4.1 or GLES 3.2 to run this application.\n"
                "You may want to try to upgrade your graphics drivers, or check the application log for details."),
            QMessageBox::Ok);
        qFatal(
            "Unable to initialize OpenGL:\nVAO: %s\nShader Log: %s",
            d->vao->isCreated() ? "true" : "false",
            qPrintable(d->shaderProgram->log()));
        exit(6);
    }

    d->vao->bind();

    GLfloat vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

    d->vbo = std::make_unique<QOpenGLBuffer>();
    d->vbo->create();
    d->vbo->bind();
    d->vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    d->vbo->allocate(vertices, sizeof(vertices));

    d->shaderProgram->enableAttributeArray(0);
    d->shaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));

    d->vbo->release();
    d->vao->release();

    // Initialize shader uniforms with default values
    d->shaderProgram->bind();
    d->shaderProgram->setUniformValue("minPixelValue", 0.0f);
    d->shaderProgram->setUniformValue("maxPixelValue", 1.0f);
    d->shaderProgram->release();

    // Initialize PBOs for async texture uploads (if supported)
    d->pboIds[0] = d->pboIds[1] = 0;
    if (context()->hasExtension("GL_ARB_pixel_buffer_object") || context()->format().majorVersion() >= 3) {
        glGenBuffers(2, d->pboIds);
    }

    // Reset cached uniform state so they get re-applied on next render
    d->lastAspectRatio = -1.0f;
    d->lastHighlightSaturation = false;
    d->lastBgColor = QVector4D(-1.0f, -1.0f, -1.0f, -1.0f);
    d->lastPixelRangeMin = -1;
    d->lastPixelRangeMax = -1;

    // If we already have image data, mark it for re-upload to the new context
    if (!d->glImage.isEmpty())
        d->imageDataChanged = true;
}

void ImageViewWidget::paintGL()
{
    renderImage();
}

void ImageViewWidget::renderImage()
{
    if (d->glImage.isEmpty())
        return;

    const auto imgWidth = d->glImage.width;
    const auto imgHeight = d->glImage.height;
    const auto channels = d->glImage.channels;
    const auto bytesPerChannel = d->glImage.bytesPerChannel;

    // Setup or recreate texture only when dimensions change
    if (d->textureId == 0 || d->textureWidth != imgWidth || d->textureHeight != imgHeight) {
        if (d->textureId != 0)
            glDeleteTextures(1, &d->textureId);

        glGenTextures(1, &d->textureId);
        glBindTexture(GL_TEXTURE_2D, d->textureId);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        d->setupTextureFormat(channels, bytesPerChannel);
        d->textureWidth = imgWidth;
        d->textureHeight = imgHeight;

        // Set pixel unpack alignment (data is tightly packed)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Allocate texture storage
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            d->textureInternalFormat,
            imgWidth,
            imgHeight,
            0,
            d->textureFormat,
            d->textureType,
            nullptr);

        // Setup PBOs if available
        if (d->pboIds[0] != 0) {
            const size_t dataSize = d->glImage.dataSize();
            if (d->pboSize != dataSize) {
                d->pboSize = dataSize;
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[0]);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[1]);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }
        }
    } else {
        glBindTexture(GL_TEXTURE_2D, d->textureId);
    }

    // Upload texture data
    if (d->pboIds[0] != 0) {
        // Check if we need immediate upload (new image data)
        if (d->imageDataChanged) {
            // Upload to both PBOs immediately to avoid one-frame delay
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[0]);
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, d->pboSize, d->glImage.data.constData());
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[1]);
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, d->pboSize, d->glImage.data.constData());

            // Now upload from the current PBO
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgWidth, imgHeight, d->textureFormat, d->textureType, nullptr);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            d->imageDataChanged = false;
        } else {
            // Use PBO double buffering for asynchronous texture upload
            d->pboIndex = (d->pboIndex + 1) % 2;
            const int nextIndex = (d->pboIndex + 1) % 2;

            // Bind PBO to upload data from previous frame
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[d->pboIndex]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgWidth, imgHeight, d->textureFormat, d->textureType, nullptr);

            // Bind next PBO and update data for next frame
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[nextIndex]);
            glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, d->pboSize, d->glImage.data.constData());
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
    } else {
        // Direct texture upload, if we have no PBO support
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0, imgWidth, imgHeight, d->textureFormat, d->textureType, d->glImage.data.constData());
    }

    // Render
    glClear(GL_COLOR_BUFFER_BIT);

    d->shaderProgram->bind();

    // Semi-static uniforms
    if (d->lastBgColor != d->bgColorVec) {
        d->shaderProgram->setUniformValue("bgColor", d->bgColorVec);
        d->shaderProgram->setUniformValue("isGrayscale", channels == 1 ? 1.0f : 0.0f);
        d->lastBgColor = d->bgColorVec;
    }

    // Only update uniforms when they change
    const float imageAspectRatio = static_cast<float>(imgWidth) / imgHeight;
    const float aspectRatio = static_cast<float>(width()) / height() / imageAspectRatio;

    if (std::abs(aspectRatio - d->lastAspectRatio) > 0.001f) {
        d->shaderProgram->setUniformValue("aspectRatio", aspectRatio);
        d->lastAspectRatio = aspectRatio;
    }

    if (d->highlightSaturation != d->lastHighlightSaturation) {
        d->shaderProgram->setUniformValue("showSaturation", d->highlightSaturation ? 1.0f : 0.0f);
        d->lastHighlightSaturation = d->highlightSaturation;
    }

    // Update pixel range uniforms for contrast adjustment
    if (d->pixelRangeMin != d->lastPixelRangeMin || d->pixelRangeMax != d->lastPixelRangeMax) {
        // Normalize to 0-1 range based on bit depth
        const float maxValue = (bytesPerChannel == 2) ? 65535.0f : 255.0f;
        const float minNorm = static_cast<float>(d->pixelRangeMin) / maxValue;
        const float maxNorm = static_cast<float>(d->pixelRangeMax) / maxValue;

        d->shaderProgram->setUniformValue("minPixelValue", minNorm);
        d->shaderProgram->setUniformValue("maxPixelValue", maxNorm);
        d->lastPixelRangeMin = d->pixelRangeMin;
        d->lastPixelRangeMax = d->pixelRangeMax;
    }

    d->vao->bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    d->vao->release();

    d->shaderProgram->release();
}

bool ImageViewWidget::showImage(const RawImage &image)
{
    if (image.isEmpty())
        return false;

    d->glImage = image;

    // Mark that image data has changed and needs immediate upload
    d->imageDataChanged = true;

    update();

    return true;
}

RawImage ImageViewWidget::currentImage() const
{
    return d->glImage;
}

void ImageViewWidget::setMinimumSize(const QSize &size)
{
    setMinimumWidth(size.width());
    setMinimumHeight(size.height());
}

void ImageViewWidget::setHighlightSaturation(bool enabled)
{
    d->highlightSaturation = enabled;
}

bool ImageViewWidget::highlightSaturation() const
{
    return d->highlightSaturation;
}

void ImageViewWidget::setPixelRange(int minValue, int maxValue)
{
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    d->pixelRangeMin = minValue;
    d->pixelRangeMax = maxValue;
    update();
}

void ImageViewWidget::getPixelRange(int &minValue, int &maxValue) const
{
    minValue = d->pixelRangeMin;
    maxValue = d->pixelRangeMax;
}

int ImageViewWidget::pixelRangeMin() const
{
    return d->pixelRangeMin;
}

int ImageViewWidget::pixelRangeMax() const
{
    return d->pixelRangeMax;
}

bool ImageViewWidget::usesGLES() const
{
#ifdef USE_GLES
    return true;
#else
    return false;
#endif
}
