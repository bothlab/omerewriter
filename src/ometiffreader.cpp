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
#include <ome/files/in/TIFFReader.h>
#include <ome/files/out/OMETIFFWriter.h>
#include <ome/files/PixelBuffer.h>
#include <ome/files/VariantPixelBuffer.h>
#include <ome/files/MetadataTools.h>
#include <ome/xml/meta/Convert.h>
#include <ome/xml/meta/OMEXMLMetadata.h>
#include <ome/xml/model/primitives/Quantity.h>

#include "ome/xml/meta/DummyMetadata.h"

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
    : QObject(parent),
      d(std::make_unique<Private>())
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
        if (filename.endsWith(".ome.tiff", Qt::CaseInsensitive) || filename.endsWith(".ome.tif", Qt::CaseInsensitive)) {
            auto omeReader = std::make_shared<ome::files::in::OMETIFFReader>();

            // Create an OMEXMLMetadata store for the reader to populate
            std::shared_ptr<ome::xml::meta::MetadataStore> metaStore =
                std::make_shared<ome::xml::meta::OMEXMLMetadata>();
            omeReader->setMetadataStore(metaStore);

            // Now open the file - this will populate the metadata store
            omeReader->setId(filename.toStdString());
            d->reader = omeReader;
        } else {
            d->reader = std::make_shared<ome::files::in::TIFFReader>();
            d->reader->setId(filename.toStdString());
        }
        d->currentFilename = filename;
        d->series = 0;
        d->resolution = 0;
        d->updateCachedDimensions();

        qDebug() << "Opened OME-TIFF:" << filename;
        qDebug() << "  Dimensions: X=" << d->cachedSizeX << "Y=" << d->cachedSizeY << "Z=" << d->cachedSizeZ
                 << "T=" << d->cachedSizeT << "C=" << d->cachedSizeC;
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

dimension_size_type OMETiffReader::getIndex(dimension_size_type z, dimension_size_type c, dimension_size_type t) const
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

namespace
{

/**
 * @brief Visitor to convert VariantPixelBuffer to RawImage
 */
struct PixelBufferToRawImageVisitor {
    dimension_size_type width;
    dimension_size_type height;
    RawImage result;

    PixelBufferToRawImageVisitor(dimension_size_type w, dimension_size_type h)
        : width(w),
          height(h)
    {
    }

    // Handle 8-bit unsigned
    void operator()(const std::shared_ptr<PixelBuffer<uint8_t>> &buf)
    {
        if (!buf)
            return;
        copyData(buf->data(), 1);
    }

    // Handle 16-bit unsigned
    void operator()(const std::shared_ptr<PixelBuffer<uint16_t>> &buf)
    {
        if (!buf)
            return;
        copyData(buf->data(), 2);
    }

    // Handle 8-bit signed - convert to unsigned
    void operator()(const std::shared_ptr<PixelBuffer<int8_t>> &buf)
    {
        if (!buf)
            return;
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 1;

        const int8_t *src = buf->data();
        uint8_t *dst = reinterpret_cast<uint8_t *>(result.data.data());
        for (size_t i = 0; i < numPixels; ++i) {
            dst[i] = static_cast<uint8_t>(static_cast<int>(src[i]) + 128);
        }
    }

    // Handle 16-bit signed - convert to unsigned
    void operator()(const std::shared_ptr<PixelBuffer<int16_t>> &buf)
    {
        if (!buf)
            return;
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        const int16_t *src = buf->data();
        uint16_t *dst = reinterpret_cast<uint16_t *>(result.data.data());
        for (size_t i = 0; i < numPixels; ++i) {
            dst[i] = static_cast<uint16_t>(static_cast<int>(src[i]) + 32768);
        }
    }

    // Handle 32-bit unsigned - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<uint32_t>> &buf)
    {
        if (!buf)
            return;
        normalizeToUint16(buf->data());
    }

    // Handle 32-bit signed - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<int32_t>> &buf)
    {
        if (!buf)
            return;
        normalizeSignedToUint16(buf->data());
    }

    // Handle float - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<float>> &buf)
    {
        if (!buf)
            return;
        normalizeFloatToUint16(buf->data());
    }

    // Handle double - normalize to 16-bit
    void operator()(const std::shared_ptr<PixelBuffer<double>> &buf)
    {
        if (!buf)
            return;
        normalizeDoubleToUint16(buf->data());
    }

    // Handle bool/bit
    void operator()(const std::shared_ptr<PixelBuffer<bool>> &buf)
    {
        if (!buf)
            return;
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 1;

        const bool *src = buf->data();
        uint8_t *dst = reinterpret_cast<uint8_t *>(result.data.data());
        for (size_t i = 0; i < numPixels; ++i) {
            dst[i] = src[i] ? 255 : 0;
        }
    }

    // Handle complex types (not supported for display)
    void operator()(const std::shared_ptr<PixelBuffer<std::complex<float>>> & /* buf */)
    {
        qWarning() << "Complex float pixel types not supported for display";
    }

    void operator()(const std::shared_ptr<PixelBuffer<std::complex<double>>> & /* buf */)
    {
        qWarning() << "Complex double pixel types not supported for display";
    }

private:
    template<typename T>
    void copyData(const T *src, int bytesPerChan)
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
    void normalizeToUint16(const T *src)
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
            if (src[i] < minVal)
                minVal = src[i];
            if (src[i] > maxVal)
                maxVal = src[i];
        }

        uint16_t *dst = reinterpret_cast<uint16_t *>(result.data.data());
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
    void normalizeSignedToUint16(const T *src)
    {
        const size_t numPixels = width * height;
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

        uint16_t *dst = reinterpret_cast<uint16_t *>(result.data.data());
        if (maxVal > minVal) {
            const double scale = 65535.0 / static_cast<double>(maxVal - minVal);
            for (size_t i = 0; i < numPixels; ++i) {
                dst[i] = static_cast<uint16_t>((static_cast<double>(src[i]) - minVal) * scale);
            }
        } else {
            std::memset(dst, 0, numPixels * 2);
        }
    }

    void normalizeFloatToUint16(const float *src)
    {
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        float minVal = src[0], maxVal = src[0];
        for (size_t i = 1; i < numPixels; ++i) {
            if (src[i] < minVal)
                minVal = src[i];
            if (src[i] > maxVal)
                maxVal = src[i];
        }

        uint16_t *dst = reinterpret_cast<uint16_t *>(result.data.data());
        if (maxVal > minVal) {
            const float scale = 65535.0f / (maxVal - minVal);
            for (size_t i = 0; i < numPixels; ++i) {
                dst[i] = static_cast<uint16_t>((src[i] - minVal) * scale);
            }
        } else {
            std::memset(dst, 0, numPixels * 2);
        }
    }

    void normalizeDoubleToUint16(const double *src)
    {
        const size_t numPixels = width * height;
        result.data.resize(static_cast<qsizetype>(numPixels * 2));
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.channels = 1;
        result.bytesPerChannel = 2;

        double minVal = src[0], maxVal = src[0];
        for (size_t i = 1; i < numPixels; ++i) {
            if (src[i] < minVal)
                minVal = src[i];
            if (src[i] > maxVal)
                maxVal = src[i];
        }

        uint16_t *dst = reinterpret_cast<uint16_t *>(result.data.data());
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

std::shared_ptr<ome::xml::meta::OMEXMLMetadata> OMETiffReader::omeMetadata() const
{
    if (!d->reader)
        return nullptr;

    // We set up an OMEXMLMetadata store when opening the OME-TIFF file.
    auto metaStore = d->reader->getMetadataStore();
    return std::dynamic_pointer_cast<ome::xml::meta::OMEXMLMetadata>(metaStore);
}

static double floatQuantityToNm(
    const ome::xml::model::primitives::
        Quantity<ome::xml::model::enums::UnitsLength, ome::xml::model::primitives::PositiveFloat> &q)
{
    double value = q.getValue();
    auto unit = q.getUnit();
    if (unit == ome::xml::model::enums::UnitsLength::MICROMETER)
        value *= 1000.0;
    else if (unit == ome::xml::model::enums::UnitsLength::MILLIMETER)
        value *= 1000000.0;
    else if (unit == ome::xml::model::enums::UnitsLength::NANOMETER) { /* already nm */
    } else if (unit == ome::xml::model::enums::UnitsLength::METER)
        value *= 1e9;
    return value;
}

ImageMetadata OMETiffReader::extractMetadata(dimension_size_type imageIndex) const
{
    ImageMetadata meta;

    if (!d->reader) {
        qWarning() << "extractMetadata: No reader available";
        return meta;
    }

    std::shared_ptr<ome::xml::meta::MetadataRetrieve> retrieve;
    auto metaStore = d->reader->getMetadataStore();
    retrieve = std::dynamic_pointer_cast<ome::xml::meta::MetadataRetrieve>(metaStore);

    // check if real metadata is available, if not, fall back to basic info
    bool metadataAvailable = std::dynamic_pointer_cast<ome::xml::meta::DummyMetadata>(metaStore) == nullptr;
    if (!retrieve || !metadataAvailable) {
        qWarning() << "extractMetadata: No metadata retrieve available, using fallback";
        meta.sizeX = static_cast<int>(d->cachedSizeX);
        meta.sizeY = static_cast<int>(d->cachedSizeY);
        meta.sizeZ = static_cast<int>(d->cachedSizeZ);
        meta.sizeC = static_cast<int>(d->cachedSizeC);
        meta.sizeT = static_cast<int>(d->cachedSizeT);
        meta.pixelType = QString::fromStdString(std::string(d->cachedPixelType));
        QFileInfo fi(d->currentFilename);
        meta.imageName = fi.fileName();
        meta.dataSizeBytes = fi.size();
        return meta;
    }

    using namespace ome::xml::model;

    // Image name
    try {
        auto name = retrieve->getImageName(imageIndex);
        meta.imageName = QString::fromStdString(name);
    } catch (const std::exception &e) {
        QFileInfo fi(d->currentFilename);
        meta.imageName = fi.fileName();
    }

    // Dimensions
    try {
        meta.sizeX = static_cast<int>(retrieve->getPixelsSizeX(imageIndex));
        meta.sizeY = static_cast<int>(retrieve->getPixelsSizeY(imageIndex));
        meta.sizeZ = static_cast<int>(retrieve->getPixelsSizeZ(imageIndex));
        meta.sizeC = static_cast<int>(retrieve->getPixelsSizeC(imageIndex));
        meta.sizeT = static_cast<int>(retrieve->getPixelsSizeT(imageIndex));
    } catch (const std::exception &e) {
        qWarning() << "extractMetadata: Failed to get dimensions:" << e.what();
    }

    // Pixel type
    try {
        auto pt = retrieve->getPixelsType(imageIndex);
        meta.pixelType = QString::fromStdString(std::string(pt));

        // Calculate approximate data size
        int bytesPerPixel = 1;
        if (pt == enums::PixelType::UINT16 || pt == enums::PixelType::INT16)
            bytesPerPixel = 2;
        else if (pt == enums::PixelType::UINT32 || pt == enums::PixelType::INT32 || pt == enums::PixelType::FLOAT)
            bytesPerPixel = 4;
        else if (pt == enums::PixelType::DOUBLE)
            bytesPerPixel = 8;
        meta.dataSizeBytes = static_cast<size_t>(meta.sizeX) * meta.sizeY * meta.sizeZ * meta.sizeC * meta.sizeT
                             * bytesPerPixel;
    } catch (const std::exception &e) {
        qWarning() << "extractMetadata: Failed to get pixel type:" << e.what();
    }

    // Physical sizes (convert to nm)
    try {
        auto physX = retrieve->getPixelsPhysicalSizeX(imageIndex);
        meta.physSizeXNm = floatQuantityToNm(physX);
    } catch (const std::exception &e) {
    }

    try {
        auto physY = retrieve->getPixelsPhysicalSizeY(imageIndex);
        meta.physSizeYNm = floatQuantityToNm(physY);
    } catch (const std::exception &e) {
    }

    try {
        auto physZ = retrieve->getPixelsPhysicalSizeZ(imageIndex);
        meta.physSizeZNm = floatQuantityToNm(physZ);
    } catch (const std::exception &e) {
    }

    // Objective/optical parameters from Instrument
    try {
        // Get objective settings from image
        auto objectiveID = retrieve->getObjectiveSettingsID(imageIndex);

        // Find the objective
        auto instrumentCount = retrieve->getInstrumentCount();

        for (dimension_size_type inst = 0; inst < instrumentCount; ++inst) {
            auto objectiveCount = retrieve->getObjectiveCount(inst);

            for (dimension_size_type obj = 0; obj < objectiveCount; ++obj) {
                auto objID = retrieve->getObjectiveID(inst, obj);
                if (objID != objectiveID)
                    continue;

                try {
                    meta.numericalAperture = retrieve->getObjectiveLensNA(inst, obj);
                } catch (const std::exception &e) {
                }

                try {
                    meta.lensImmersion = retrieve->getObjectiveImmersion(inst, obj);
                } catch (const std::exception &e) {
                }
                break;
            }
        }
    } catch (const std::exception &e) {
    }

    // Refractive index
    try {
        meta.immersionRI = retrieve->getObjectiveSettingsRefractiveIndex(imageIndex);
    } catch (const std::exception &e) {
    }

    // Medium
    try {
        meta.embeddingMedium = retrieve->getObjectiveSettingsMedium(imageIndex);
    } catch (const std::exception &e) {
    }

    // Channel information
    try {
        auto channelCount = retrieve->getChannelCount(imageIndex);

        for (dimension_size_type ch = 0; ch < channelCount; ++ch) {
            ChannelParams chParams;

            try {
                chParams.name = QString::fromStdString(retrieve->getChannelName(imageIndex, ch));
                qDebug() << "extractMetadata: Channel" << ch << "name:" << chParams.name;
            } catch (const std::exception &e) {
                chParams.name = QString("Channel %1").arg(ch);
            }

            try {
                chParams.acquisitionMode = retrieve->getChannelAcquisitionMode(imageIndex, ch);
                if (chParams.acquisitionMode == enums::AcquisitionMode::MULTIPHOTONMICROSCOPY) {
                    chParams.isMultiPhoton = true;
                    chParams.photonCount = 2; // Default assumption
                }
                qDebug() << "extractMetadata: Channel" << ch << "acquisition mode:" << chParams.acquisitionMode;
            } catch (const std::exception &e) {
                chParams.acquisitionMode = enums::AcquisitionMode::LASERSCANNINGCONFOCALMICROSCOPY;
            }

            try {
                auto excWL = retrieve->getChannelExcitationWavelength(imageIndex, ch);
                double value = excWL.getValue();
                auto unit = excWL.getUnit();
                // Convert to nm
                if (unit == enums::UnitsLength::MICROMETER)
                    value *= 1000.0;
                else if (unit == enums::UnitsLength::METER)
                    value *= 1e9;
                chParams.exWavelengthNm = value;
                qDebug() << "extractMetadata: Channel" << ch << "excitation:" << value << "nm";
            } catch (const std::exception &e) {
            }

            try {
                auto emWL = retrieve->getChannelEmissionWavelength(imageIndex, ch);
                double value = emWL.getValue();
                auto unit = emWL.getUnit();
                // Convert to nm
                if (unit == enums::UnitsLength::MICROMETER)
                    value *= 1000.0;
                else if (unit == enums::UnitsLength::METER)
                    value *= 1e9;
                chParams.emWavelengthNm = value;
                qDebug() << "extractMetadata: Channel" << ch << "emission:" << value << "nm";
            } catch (const std::exception &e) {
            }

            try {
                auto pinhole = retrieve->getChannelPinholeSize(imageIndex, ch);
                double value = pinhole.getValue();
                auto unit = pinhole.getUnit();
                // Convert to nm
                if (unit == enums::UnitsLength::MICROMETER)
                    value *= 1000.0;
                else if (unit == enums::UnitsLength::METER)
                    value *= 1e9;
                chParams.pinholeSizeNm = value;
                qDebug() << "extractMetadata: Channel" << ch << "pinhole:" << value << "nm";
            } catch (const std::exception &e) {
            }

            meta.channels.push_back(chParams);
        }
    } catch (const std::exception &e) {
        qWarning() << "extractMetadata: Error extracting channel information:" << e.what();
    }

    return meta;
}

bool OMETiffReader::saveWithMetadata(const QString &outputPath, const ImageMetadata &metadata)
{
    if (!d->reader) {
        qWarning() << "No file open";
        return false;
    }

    try {
        using namespace ome::xml::model;
        using PositiveLength = primitives::Quantity<enums::UnitsLength, primitives::PositiveFloat>;
        using Length = primitives::Quantity<enums::UnitsLength>;

        // Get the current metadata from the reader's metadata store
        auto metaStore = d->reader->getMetadataStore();
        auto sourceMetadata = std::dynamic_pointer_cast<ome::xml::meta::OMEXMLMetadata>(metaStore);

        if (!sourceMetadata) {
            qWarning() << "Failed to get source metadata";
            return false;
        }

        // Create a copy of the metadata to modify
        auto modifiedMeta = std::make_shared<ome::xml::meta::OMEXMLMetadata>();
        ome::xml::meta::convert(*sourceMetadata, *modifiedMeta);

        dimension_size_type imageIndex = 0;

        // Update physical sizes (convert from nm to micrometers for OME standard)
        if (metadata.physSizeXNm > 0) {
            modifiedMeta->setPixelsPhysicalSizeX(
                PositiveLength(metadata.physSizeXNm / 1000.0, enums::UnitsLength::MICROMETER), imageIndex);
        }
        if (metadata.physSizeYNm > 0) {
            modifiedMeta->setPixelsPhysicalSizeY(
                PositiveLength(metadata.physSizeYNm / 1000.0, enums::UnitsLength::MICROMETER), imageIndex);
        }
        if (metadata.physSizeZNm > 0) {
            modifiedMeta->setPixelsPhysicalSizeZ(
                PositiveLength(metadata.physSizeZNm / 1000.0, enums::UnitsLength::MICROMETER), imageIndex);
        }

        // Update objective settings
        try {
            if (metadata.immersionRI > 0)
                modifiedMeta->setObjectiveSettingsRefractiveIndex(metadata.immersionRI, imageIndex);

            // Set medium
            modifiedMeta->setObjectiveSettingsMedium(metadata.embeddingMedium, imageIndex);
        } catch (...) {
        }

        // Update instrument/objective data
        try {
            auto objectiveID = modifiedMeta->getObjectiveSettingsID(imageIndex);
            auto instrumentCount = modifiedMeta->getInstrumentCount();
            for (dimension_size_type inst = 0; inst < instrumentCount; ++inst) {
                auto objectiveCount = modifiedMeta->getObjectiveCount(inst);
                for (dimension_size_type obj = 0; obj < objectiveCount; ++obj) {
                    auto objID = modifiedMeta->getObjectiveID(inst, obj);
                    if (objID == objectiveID) {
                        if (metadata.numericalAperture > 0) {
                            modifiedMeta->setObjectiveLensNA(metadata.numericalAperture, inst, obj);
                        }

                        modifiedMeta->setObjectiveImmersion(metadata.lensImmersion, inst, obj);
                        break;
                    }
                }
            }
        } catch (...) {
        }

        // Update channel information
        for (size_t ch = 0; ch < metadata.channels.size(); ++ch) {
            const auto &chParams = metadata.channels[ch];

            try {
                modifiedMeta->setChannelName(chParams.name.toStdString(), imageIndex, ch);
            } catch (...) {
            }

            try {
                modifiedMeta->setChannelAcquisitionMode(chParams.acquisitionMode, imageIndex, ch);
            } catch (...) {
            }

            try {
                if (chParams.exWavelengthNm > 0) {
                    PositiveLength excWL(chParams.exWavelengthNm, enums::UnitsLength::NANOMETER);
                    modifiedMeta->setChannelExcitationWavelength(excWL, imageIndex, ch);
                }
            } catch (...) {
            }

            try {
                if (chParams.emWavelengthNm > 0) {
                    PositiveLength emWL(chParams.emWavelengthNm, enums::UnitsLength::NANOMETER);
                    modifiedMeta->setChannelEmissionWavelength(emWL, imageIndex, ch);
                }
            } catch (...) {
            }

            try {
                if (chParams.pinholeSizeNm > 0) {
                    Length pinhole(chParams.pinholeSizeNm, enums::UnitsLength::NANOMETER);
                    modifiedMeta->setChannelPinholeSize(pinhole, imageIndex, ch);
                }
            } catch (...) {
            }
        }

        // Create writer and write the file
        auto writer = std::make_shared<ome::files::out::OMETIFFWriter>();
        std::shared_ptr<ome::xml::meta::MetadataRetrieve> metaRetrieve = modifiedMeta;
        writer->setMetadataRetrieve(metaRetrieve);
        // Always use BigTIFF format to support large files (>4GB)
        writer->setBigTIFF(true);

        // Use interleaved (contiguous) storage
        writer->setInterleaved(true);

        // Use zlib compression for smaller file sizes
        // A warning is emitted for "Deflate", recommending to use "AdobeDeflate" instead for wider support,
        // and claiming "Deflate" was legacy. So we just use "AdobeDeflate", it's the same algorithm.
        writer->setCompression("AdobeDeflate");

        writer->setId(outputPath.toStdString());

        // Copy all planes from the source file
        dimension_size_type seriesCount = d->reader->getSeriesCount();
        for (dimension_size_type series = 0; series < seriesCount; ++series) {
            d->reader->setSeries(series);
            writer->setSeries(series);

            dimension_size_type planeCount = d->reader->getImageCount();
            for (dimension_size_type plane = 0; plane < planeCount; ++plane) {
                VariantPixelBuffer buf;
                d->reader->openBytes(plane, buf);
                writer->saveBytes(plane, buf);
            }
        }

        writer->close();

        qDebug() << "Successfully saved OME-TIFF with modified metadata to:" << outputPath;
        return true;

    } catch (const std::exception &e) {
        qWarning() << "Failed to save OME-TIFF:" << e.what();
        return false;
    }
}
