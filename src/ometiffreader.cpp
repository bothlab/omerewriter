/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ometiffreader.h"

#include <QDebug>
#include <QFileInfo>
#include <cstring>

#include <ome/files/in/OMETIFFReader.h>
#include <ome/files/PixelBuffer.h>
#include <ome/files/VariantPixelBuffer.h>

using ome::files::dimension_size_type;
using ome::files::PixelBuffer;
using ome::files::VariantPixelBuffer;
typedef ome::xml::model::enums::PixelType PT;

class OMETiffReader::Private
{
public:
    std::shared_ptr<ome::files::FormatReader> reader;
    QString currentFilename;
    dimension_size_type series = 0;
    dimension_size_type resolution = 0;

    // Cached dimension sizes
    dimension_size_type cachedSizeX = 0;
    dimension_size_type cachedSizeY = 0;
    dimension_size_type cachedSizeZ = 0;
    dimension_size_type cachedSizeT = 0;
    dimension_size_type cachedSizeC = 0;
    dimension_size_type cachedImageCount = 0;
    dimension_size_type cachedRGBChannelCount = 0;
    PT cachedPixelType = PT::UINT8;

    void updateCachedDimensions()
    {
        if (!reader)
            return;

        dimension_size_type oldSeries = reader->getSeries();
        reader->setSeries(series);
        reader->setResolution(resolution);

        cachedSizeX = reader->getSizeX();
        cachedSizeY = reader->getSizeY();
        cachedSizeZ = reader->getSizeZ();
        cachedSizeT = reader->getSizeT();
        cachedSizeC = reader->getEffectiveSizeC();
        cachedImageCount = reader->getImageCount();
        cachedRGBChannelCount = reader->getRGBChannelCount(0);
        cachedPixelType = reader->getPixelType();

        reader->setSeries(oldSeries);
    }
};

OMETiffReader::OMETiffReader(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

OMETiffReader::~OMETiffReader()
{
    close();
}

bool OMETiffReader::open(const QString &filename)
{
    close();

    QFileInfo fileInfo(filename);
    if (!fileInfo.exists()) {
        qWarning() << "File does not exist:" << filename;
        return false;
    }

    try {
        d->reader = std::make_shared<ome::files::in::OMETIFFReader>();
        d->reader->setId(filename.toStdString());
        d->currentFilename = filename;
        d->series = 0;
        d->resolution = 0;
        d->updateCachedDimensions();

        qDebug() << "Opened OME-TIFF:" << filename;
        qDebug() << "  Dimensions: X=" << d->cachedSizeX << "Y=" << d->cachedSizeY
                 << "Z=" << d->cachedSizeZ << "T=" << d->cachedSizeT << "C=" << d->cachedSizeC;
        qDebug() << "  Image count:" << d->cachedImageCount;
        qDebug() << "  RGB channel count:" << d->cachedRGBChannelCount;

        return true;
    } catch (const std::exception &e) {
        qWarning() << "Failed to open OME-TIFF:" << e.what();
        d->reader.reset();
        return false;
    }
}

void OMETiffReader::close()
{
    if (d->reader) {
        try {
            d->reader->close();
        } catch (const std::exception &e) {
            qWarning() << "Error closing file:" << e.what();
        }
        d->reader.reset();
    }
    d->currentFilename.clear();
    d->cachedSizeX = d->cachedSizeY = d->cachedSizeZ = d->cachedSizeT = d->cachedSizeC = 0;
    d->cachedImageCount = 0;
}

bool OMETiffReader::isOpen() const
{
    return d->reader != nullptr;
}

QString OMETiffReader::filename() const
{
    return d->currentFilename;
}

dimension_size_type OMETiffReader::sizeX() const
{
    return d->cachedSizeX;
}

dimension_size_type OMETiffReader::sizeY() const
{
    return d->cachedSizeY;
}

dimension_size_type OMETiffReader::sizeZ() const
{
    return d->cachedSizeZ;
}

dimension_size_type OMETiffReader::sizeT() const
{
    return d->cachedSizeT;
}

dimension_size_type OMETiffReader::sizeC() const
{
    return d->cachedSizeC;
}

dimension_size_type OMETiffReader::imageCount() const
{
    return d->cachedImageCount;
}

ome::xml::model::enums::PixelType OMETiffReader::pixelType() const
{
    return d->cachedPixelType;
}

dimension_size_type OMETiffReader::rgbChannelCount() const
{
    return d->cachedRGBChannelCount;
}

dimension_size_type OMETiffReader::getIndex(dimension_size_type z,
                                             dimension_size_type c,
                                             dimension_size_type t) const
{
    if (!d->reader)
        return 0;

    dimension_size_type oldSeries = d->reader->getSeries();
    d->reader->setSeries(d->series);
    d->reader->setResolution(d->resolution);
    dimension_size_type index = d->reader->getIndex(z, c, t);
    d->reader->setSeries(oldSeries);
    return index;
}

namespace {

/**
 * @brief Visitor to convert VariantPixelBuffer to RawImage
 */
struct PixelBufferToRawImageVisitor
{
    dimension_size_type width;
    dimension_size_type height;
    RawImage result;

    PixelBufferToRawImageVisitor(dimension_size_type w, dimension_size_type h)
        : width(w), height(h)
    {}

    // Handle 8-bit unsigned
    void operator()(const std::shared_ptr<PixelBuffer<uint8_t>>& buf)
    {
        if (!buf) return;
        copyData(buf->data(), 1);
    }

    // Handle 16-bit unsigned
    void operator()(const std::shared_ptr<PixelBuffer<uint16_t>>& buf)
    {
        if (!buf) return;
        copyData(buf->data(), 2);
    }

    // Handle 8-bit signed - convert to unsigned
    void operator()(const std::shared_ptr<PixelBuffer<int8_t>>& buf)
    {
        if (!buf) return;
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 1;

        const int8_t* src = buf->data();
        uint8_t* dst = reinterpret_cast<uint8_t*>(result.data.data());
        for (size_t i = 0; i < numPixels; ++i) {
            dst[i] = static_cast<uint8_t>(static_cast<int>(src[i]) + 128);
        }
    }

    // Handle 16-bit signed - convert to unsigned
    void operator()(const std::shared_ptr<PixelBuffer<int16_t>>& buf)
    {
        if (!buf) return;
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        const int16_t* src = buf->data();
        uint16_t* dst = reinterpret_cast<uint16_t*>(result.data.data());
        for (size_t i = 0; i < numPixels; ++i) {
            dst[i] = static_cast<uint16_t>(static_cast<int>(src[i]) + 32768);
        }
    }

    // Handle 32-bit unsigned - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<uint32_t>>& buf)
    {
        if (!buf) return;
        normalizeToUint16(buf->data());
    }

    // Handle 32-bit signed - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<int32_t>>& buf)
    {
        if (!buf) return;
        normalizeSignedToUint16(buf->data());
    }

    // Handle float - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<float>>& buf)
    {
        if (!buf) return;
        normalizeFloatToUint16(buf->data());
    }

    // Handle double - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<double>>& buf)
    {
        if (!buf) return;
        normalizeDoubleToUint16(buf->data());
    }

    // Handle bool/bit
    void operator()(const std::shared_ptr<PixelBuffer<bool>>& buf)
    {
        if (!buf) return;
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 1;

        const bool* src = buf->data();
        uint8_t* dst = reinterpret_cast<uint8_t*>(result.data.data());
        for (size_t i = 0; i < numPixels; ++i) {
            dst[i] = src[i] ? 255 : 0;
        }
    }

    // Handle complex types (not supported for display)
    void operator()(const std::shared_ptr<PixelBuffer<std::complex<float>>>& /* buf */)
    {
        qWarning() << "Complex float pixel types not supported for display";
    }

    void operator()(const std::shared_ptr<PixelBuffer<std::complex<double>>>& /* buf */)
    {
        qWarning() << "Complex double pixel types not supported for display";
    }

private:
    template<typename T>
    void copyData(const T* src, int bytesPerChan)
    {
        const size_t dataSize = width * height * bytesPerChan;
        result.data.resize(static_cast<qsizetype>(dataSize));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = bytesPerChan;
        std::memcpy(result.data.data(), src, dataSize);
    }

    template<typename T>
    void normalizeToUint16(const T* src)
    {
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        // Find min/max
        T minVal = src[0], maxVal = src[0];
        for (size_t i = 1; i < numPixels; ++i) {
            if (src[i] < minVal) minVal = src[i];
            if (src[i] > maxVal) maxVal = src[i];
        }

        uint16_t* dst = reinterpret_cast<uint16_t*>(result.data.data());
        if (maxVal > minVal) {
            const double scale = 65535.0 / static_cast<double>(maxVal - minVal);
            for (size_t i = 0; i < numPixels; ++i) {
                dst[i] = static_cast<uint16_t>((src[i] - minVal) * scale);
            }
        } else {
            std::memset(dst, 0, numPixels * 2);
        }
    }

    template<typename T>
    void normalizeSignedToUint16(const T* src)
    {
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        T minVal = src[0], maxVal = src[0];
        for (size_t i = 1; i < numPixels; ++i) {
            if (src[i] < minVal) minVal = src[i];
            if (src[i] > maxVal) maxVal = src[i];
        }

        uint16_t* dst = reinterpret_cast<uint16_t*>(result.data.data());
        if (maxVal > minVal) {
            const double scale = 65535.0 / static_cast<double>(maxVal - minVal);
            for (size_t i = 0; i < numPixels; ++i) {
                dst[i] = static_cast<uint16_t>((static_cast<double>(src[i]) - minVal) * scale);
            }
        } else {
            std::memset(dst, 0, numPixels * 2);
        }
    }

    void normalizeFloatToUint16(const float* src)
    {
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        float minVal = src[0], maxVal = src[0];
        for (size_t i = 1; i < numPixels; ++i) {
            if (src[i] < minVal) minVal = src[i];
            if (src[i] > maxVal) maxVal = src[i];
        }

        uint16_t* dst = reinterpret_cast<uint16_t*>(result.data.data());
        if (maxVal > minVal) {
            const float scale = 65535.0f / (maxVal - minVal);
            for (size_t i = 0; i < numPixels; ++i) {
                dst[i] = static_cast<uint16_t>((src[i] - minVal) * scale);
            }
        } else {
            std::memset(dst, 0, numPixels * 2);
        }
    }

    void normalizeDoubleToUint16(const double* src)
    {
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        double minVal = src[0], maxVal = src[0];
        for (size_t i = 1; i < numPixels; ++i) {
            if (src[i] < minVal) minVal = src[i];
            if (src[i] > maxVal) maxVal = src[i];
        }

        uint16_t* dst = reinterpret_cast<uint16_t*>(result.data.data());
        if (maxVal > minVal) {
            const double scale = 65535.0 / (maxVal - minVal);
            for (size_t i = 0; i < numPixels; ++i) {
                dst[i] = static_cast<uint16_t>((src[i] - minVal) * scale);
            }
        } else {
            std::memset(dst, 0, numPixels * 2);
        }
    }
};

} // anonymous namespace

RawImage OMETiffReader::readPlane(dimension_size_type z, dimension_size_type c, dimension_size_type t)
{
    if (!d->reader) {
        qWarning() << "No file open";
        return {};
    }

    dimension_size_type index = getIndex(z, c, t);
    return readPlaneByIndex(index);
}

RawImage OMETiffReader::readPlaneByIndex(dimension_size_type planeIndex)
{
    if (!d->reader) {
        qWarning() << "No file open";
        return {};
    }

    try {
        dimension_size_type oldSeries = d->reader->getSeries();
        d->reader->setSeries(d->series);
        d->reader->setResolution(d->resolution);

        VariantPixelBuffer buf;
        d->reader->openBytes(planeIndex, buf);

        d->reader->setSeries(oldSeries);

        PixelBufferToRawImageVisitor visitor(d->cachedSizeX, d->cachedSizeY);
        std::visit(visitor, buf.vbuffer());

        return visitor.result;

    } catch (const std::exception &e) {
        qWarning() << "Failed to read plane" << planeIndex << ":" << e.what();
        return {};
    }
}

std::shared_ptr<ome::files::FormatReader> OMETiffReader::reader() const
{
    return d->reader;
}
