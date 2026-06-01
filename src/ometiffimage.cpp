/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ometiffimage.h"

#include <QDebug>
#include <QFileInfo>
#include <cstring>

#include "ometiffreader.h"
#include "ometiffwriter.h"
#include "ometypes.h"

using omr::dimension_size_type;
using PT = omr::PixelType;

class OMETiffImage::Private
{
public:
    omr::OmeTiffReader reader;
    QString currentFilename;
    bool isOmeTiff = true;

    // Channel interleaving for raw TIFFs (number of interleaved channels)
    // When > 1, the planes are interpreted as interleaved channels
    // e.g., if interleavedChannels=2 and imageCount=10, we have 5 Z positions with 2 channels each
    dimension_size_type interleavedChannels = 1;

    // Dimension sizes (raw from reader)
    dimension_size_type rawSizeX = 0;
    dimension_size_type rawSizeY = 0;
    dimension_size_type rawSizeZ = 0;
    dimension_size_type rawSizeT = 0;
    dimension_size_type rawSizeC = 0;
    dimension_size_type rawImageCount = 0;
    dimension_size_type rawRGBChannelCount = 0;
    PT cachedPixelType = PT::UInt8;

    // Effective dimensions after applying interleaving interpretation
    dimension_size_type sizeX = 0;
    dimension_size_type sizeY = 0;
    dimension_size_type sizeZ = 0;
    dimension_size_type sizeT = 0;
    dimension_size_type sizeC = 0;
    dimension_size_type imageCount = 0;
    dimension_size_type rgbChannelCount = 0;

    void updateCachedDimensions()
    {
        if (!reader.isOpen())
            return;

        rawSizeX = reader.rawSizeX();
        rawSizeY = reader.rawSizeY();
        rawSizeZ = reader.rawSizeZ();
        rawSizeT = reader.rawSizeT();
        rawSizeC = reader.rawSizeC();
        rawImageCount = reader.rawImageCount();
        rawRGBChannelCount = reader.rawRGBChannelCount();
        cachedPixelType = reader.pixelType();
        isOmeTiff = reader.isOmeTiff();

        applyInterleavingInterpretation();
    }

    void applyInterleavingInterpretation()
    {
        sizeX = rawSizeX;
        sizeY = rawSizeY;
        rgbChannelCount = rawRGBChannelCount;

        if (interleavedChannels > 1 && !isOmeTiff) {
            // For interleaved raw TIFFs:
            // - The total number of planes (rawImageCount) is divided among the channels
            // - Each set of consecutive planes represents one Z position across all channels
            // - So for a file with 168 images and 2 interleaved channels:
            //   plane 0 = z=0, c=0
            //   plane 1 = z=0, c=1
            //   plane 2 = z=1, c=0
            //   plane 3 = z=1, c=1
            //   etc.
            // - effectiveZ = rawImageCount / interleavedChannels
            // - effectiveC = interleavedChannels

            sizeC = interleavedChannels;
            sizeZ = rawImageCount / interleavedChannels;
            sizeT = 1; // We ignore the time dimension (for now... - how does ScanImage save this?)
            imageCount = rawImageCount;
        } else {
            // No interleaving or OME-TIFF, we can just trust the existing metadata
            sizeZ = rawSizeZ;
            sizeT = rawSizeT;
            sizeC = rawSizeC;
            imageCount = rawImageCount;
        }
    }

    /**
     * @brief Convert logical (z, c, t) coordinates to a raw plane index
     *
     * For interleaved TIFFs, the raw plane order is:
     * plane 0: z=0, c=0
     * plane 1: z=0, c=1
     * plane 2: z=1, c=0
     * plane 3: z=1, c=1
     * etc.
     */
    [[nodiscard]] dimension_size_type getPlaneIndex(dimension_size_type z, dimension_size_type c, dimension_size_type t)
        const
    {
        if (interleavedChannels > 1 && !isOmeTiff) {
            // Interleaved order: for each Z, all channels are consecutive
            // plane = z * numChannels + c
            // FIXME: We ignore the time-dimension here! (Not currently needed / supported)
            return z * interleavedChannels + c;
        }

        // For OME-TIFF or non-interleaved, use the reader's native indexing
        return reader.getIndex(z, c, t);
    }
};

OMETiffImage::OMETiffImage(QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>())
{
}

OMETiffImage::~OMETiffImage()
{
    close();
}

bool OMETiffImage::open(const QString &filename)
{
    close();

    QFileInfo fileInfo(filename);
    if (!fileInfo.exists()) {
        qWarning() << "File does not exist:" << filename;
        return false;
    }

    QString error;
    if (!d->reader.open(filename, &error)) {
        qWarning().noquote() << "Failed to open TIFF:" << error;
        return false;
    }

    d->currentFilename = filename;
    d->isOmeTiff = d->reader.isOmeTiff();
    d->updateCachedDimensions();

    qDebug() << "Opened image:" << filename;
    qDebug().nospace() << "  Dimensions: X=" << d->sizeX << "Y=" << d->sizeY << "Z=" << d->sizeZ << "T=" << d->sizeT
                       << "C=" << d->sizeC;
    qDebug() << "  Image count:" << d->imageCount;
    qDebug() << "  RGB channel count:" << d->rgbChannelCount;

    return true;
}

void OMETiffImage::close()
{
    d->reader.close();
    d->currentFilename.clear();
    d->interleavedChannels = 1;
    d->rawSizeX = d->rawSizeY = d->rawSizeZ = d->rawSizeT = d->rawSizeC = 0;
    d->rawImageCount = 0;
    d->sizeX = d->sizeY = d->sizeZ = d->sizeT = d->sizeC = 0;
    d->imageCount = 0;
}

bool OMETiffImage::isOpen() const
{
    return d->reader.isOpen();
}

QString OMETiffImage::filename() const
{
    return d->currentFilename;
}

bool OMETiffImage::isOmeTiff() const
{
    return d->isOmeTiff;
}

std::expected<bool, QString> OMETiffImage::setInterleavedChannelCount(dimension_size_type channelCount)
{
    if (channelCount < 1)
        channelCount = 1;

    // Never mess with proper OME-TIFF files
    if (d->isOmeTiff)
        return std::unexpected("Cannot set interleaved channel count for OME-TIFF files!");

    // Make sure the channel count divides evenly into the image count
    if (d->rawImageCount > 0 && channelCount > 1) {
        if (d->rawImageCount % channelCount != 0)
            return std::unexpected(
                QStringLiteral("Interleaved channel count %1 does not divide evenly into image count %2")
                    .arg(channelCount)
                    .arg(d->rawImageCount));
    }

    d->interleavedChannels = channelCount;
    d->applyInterleavingInterpretation();

    qDebug().noquote() << "Set interleaved channels to" << channelCount << "- Effective dimensions: Z=" << d->sizeZ
                       << "C=" << d->sizeC << "T=" << d->sizeT;

    return true;
}

dimension_size_type OMETiffImage::interleavedChannelCount() const
{
    return d->interleavedChannels;
}

dimension_size_type OMETiffImage::rawImageCount() const
{
    return d->rawImageCount;
}

dimension_size_type OMETiffImage::sizeX() const
{
    return d->sizeX;
}

dimension_size_type OMETiffImage::sizeY() const
{
    return d->sizeY;
}

dimension_size_type OMETiffImage::sizeZ() const
{
    return d->sizeZ;
}

dimension_size_type OMETiffImage::sizeT() const
{
    return d->sizeT;
}

dimension_size_type OMETiffImage::sizeC() const
{
    return d->sizeC;
}

dimension_size_type OMETiffImage::imageCount() const
{
    return d->imageCount;
}

omr::PixelType OMETiffImage::pixelType() const
{
    return d->cachedPixelType;
}

dimension_size_type OMETiffImage::rgbChannelCount() const
{
    return d->rgbChannelCount;
}

dimension_size_type OMETiffImage::getIndex(dimension_size_type z, dimension_size_type c, dimension_size_type t) const
{
    if (!d->reader.isOpen())
        return 0;

    return d->getPlaneIndex(z, c, t);
}

namespace
{

// Min/max normalize an arbitrary numeric plane to 16-bit for display.
template<typename T>
RawImage normalizeToUint16(const T *src, dimension_size_type width, dimension_size_type height)
{
    RawImage result;
    const size_t numPixels = static_cast<size_t>(width) * height;
    result.data.resize(static_cast<qsizetype>(numPixels * 2));
    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.channels = 1;
    result.bytesPerChannel = 2;

    T minVal = src[0], maxVal = src[0];
    for (size_t i = 1; i < numPixels; ++i) {
        if (src[i] < minVal)
            minVal = src[i];
        if (src[i] > maxVal)
            maxVal = src[i];
    }

    auto *dst = reinterpret_cast<uint16_t *>(result.data.data());
    if (maxVal > minVal) {
        const double scale = 65535.0 / (static_cast<double>(maxVal) - static_cast<double>(minVal));
        for (size_t i = 0; i < numPixels; ++i)
            dst[i] = static_cast<uint16_t>((static_cast<double>(src[i]) - static_cast<double>(minVal)) * scale);
    } else {
        std::memset(dst, 0, numPixels * 2);
    }
    return result;
}

// Convert one decoded plane to a display-friendly RawImage, reproducing the
// behaviour the previous ome-files based visitor had.
RawImage rawPlaneToRawImage(const omr::PlaneData &pd)
{
    RawImage result;
    const dimension_size_type w = pd.width;
    const dimension_size_type h = pd.height;
    const unsigned int spp = pd.samplesPerPixel < 1 ? 1 : pd.samplesPerPixel;
    const size_t numPixels = static_cast<size_t>(w) * h;
    const size_t numSamples = numPixels * spp;
    const auto *raw = reinterpret_cast<const uint8_t *>(pd.bytes.constData());

    switch (pd.pixelType) {
    case PT::UInt8:
    case PT::Bit: {
        result.width = static_cast<int>(w);
        result.height = static_cast<int>(h);
        result.channels = static_cast<int>(spp);
        result.bytesPerChannel = 1;
        result.data.resize(static_cast<qsizetype>(numSamples));
        std::memcpy(result.data.data(), raw, numSamples);
        break;
    }
    case PT::UInt16: {
        result.width = static_cast<int>(w);
        result.height = static_cast<int>(h);
        result.channels = static_cast<int>(spp);
        result.bytesPerChannel = 2;
        result.data.resize(static_cast<qsizetype>(numSamples * 2));
        std::memcpy(result.data.data(), raw, numSamples * 2);
        break;
    }
    case PT::Int8: {
        result.width = static_cast<int>(w);
        result.height = static_cast<int>(h);
        result.channels = static_cast<int>(spp);
        result.bytesPerChannel = 1;
        result.data.resize(static_cast<qsizetype>(numSamples));
        const auto *src = reinterpret_cast<const int8_t *>(raw);
        auto *dst = reinterpret_cast<uint8_t *>(result.data.data());
        for (size_t i = 0; i < numSamples; ++i)
            dst[i] = static_cast<uint8_t>(static_cast<int>(src[i]) + 128);
        break;
    }
    case PT::Int16: {
        result.width = static_cast<int>(w);
        result.height = static_cast<int>(h);
        result.channels = static_cast<int>(spp);
        result.bytesPerChannel = 2;
        result.data.resize(static_cast<qsizetype>(numSamples * 2));
        const auto *src = reinterpret_cast<const int16_t *>(raw);
        auto *dst = reinterpret_cast<uint16_t *>(result.data.data());
        for (size_t i = 0; i < numSamples; ++i)
            dst[i] = static_cast<uint16_t>(static_cast<int>(src[i]) + 32768);
        break;
    }
    case PT::UInt32:
        result = normalizeToUint16(reinterpret_cast<const uint32_t *>(raw), w, h);
        break;
    case PT::Int32:
        result = normalizeToUint16(reinterpret_cast<const int32_t *>(raw), w, h);
        break;
    case PT::Float:
        result = normalizeToUint16(reinterpret_cast<const float *>(raw), w, h);
        break;
    case PT::Double:
        result = normalizeToUint16(reinterpret_cast<const double *>(raw), w, h);
        break;
    default:
        qWarning().noquote() << "Pixel type not supported for display";
        break;
    }

    return result;
}

} // anonymous namespace

RawImage OMETiffImage::readPlane(dimension_size_type z, dimension_size_type c, dimension_size_type t)
{
    if (!d->reader.isOpen()) {
        qWarning().noquote() << "No file open";
        return {};
    }

    dimension_size_type index = getIndex(z, c, t);
    return readPlaneByIndex(index);
}

RawImage OMETiffImage::readPlaneByIndex(dimension_size_type planeIndex)
{
    if (!d->reader.isOpen()) {
        qWarning().noquote() << "No file open";
        return {};
    }

    QString error;
    omr::PlaneData plane = d->reader.readPlaneByFileIndex(planeIndex, &error);
    if (!plane.isValid()) {
        qWarning().noquote() << "Failed to read plane" << planeIndex << ":" << error;
        return {};
    }

    return rawPlaneToRawImage(plane);
}

ImageMetadata OMETiffImage::extractMetadata(dimension_size_type /*imageIndex*/) const
{
    ImageMetadata meta;

    if (!d->reader.isOpen()) {
        qWarning().noquote() << "extractMetadata: No file open";
        return meta;
    }

    if (d->reader.hasMetadata()) {
        meta = d->reader.parsedMetadata();
        if (meta.imageName.isEmpty())
            meta.imageName = QFileInfo(d->currentFilename).fileName();

        const size_t bytesPerPixel = omr::pixelTypeBytes(d->cachedPixelType);
        meta.dataSizeBytes = static_cast<size_t>(meta.sizeX) * meta.sizeY * meta.sizeZ * meta.sizeC * meta.sizeT
                             * bytesPerPixel;
        return meta;
    }

    // No embedded metadata (plain TIFF): use effective dimensions and basic info.
    meta.sizeX = static_cast<int>(d->sizeX);
    meta.sizeY = static_cast<int>(d->sizeY);
    meta.sizeZ = static_cast<int>(d->sizeZ);
    meta.sizeC = static_cast<int>(d->sizeC);
    meta.sizeT = static_cast<int>(d->sizeT);
    meta.pixelType = omr::pixelTypeToString(d->cachedPixelType);

    QFileInfo fi(d->currentFilename);
    meta.imageName = fi.fileName();
    meta.dataSizeBytes = static_cast<size_t>(fi.size());

    for (int c = 0; c < meta.sizeC; ++c) {
        ChannelParams chParams;
        chParams.name = QStringLiteral("Channel %1").arg(c + 1);
        meta.channels.push_back(chParams);
    }
    return meta;
}

std::expected<bool, QString> OMETiffImage::saveWithMetadata(
    const QString &outputPath,
    const ImageMetadata &metadata,
    ProgressCallback progressCallback)
{
    if (!d->reader.isOpen())
        return std::unexpected("No image data loaded");

    auto planeIndexFn = [this](dimension_size_type z, dimension_size_type c, dimension_size_type t) {
        return d->getPlaneIndex(z, c, t);
    };

    return omr::ometiffwriter::write(
        outputPath,
        d->reader,
        metadata,
        d->sizeZ,
        d->sizeC,
        d->sizeT,
        d->cachedPixelType,
        static_cast<unsigned int>(d->rgbChannelCount < 1 ? 1 : d->rgbChannelCount),
        planeIndexFn,
        progressCallback);
}
