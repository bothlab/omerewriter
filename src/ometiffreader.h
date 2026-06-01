/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "ometiffimage.h" // ImageMetadata
#include "ometypes.h"
#include "tifffile.h"

namespace omr
{

/**
 * @brief Decoded pixel data of a single plane plus its format.
 */
struct PlaneData {
    QByteArray bytes;
    dimension_size_type width = 0;
    dimension_size_type height = 0;
    unsigned int samplesPerPixel = 1;
    unsigned int bitsPerSample = 8;
    PixelType pixelType = PixelType::UInt8;

    [[nodiscard]] bool isValid() const
    {
        return !bytes.isEmpty() && width > 0 && height > 0;
    }
};

/**
 * @brief Reads OME-TIFF and plain TIFF files via libtiff + QtXml.
 *
 * For OME-TIFF the embedded OME-XML (first IFD ImageDescription) is parsed for
 * metadata and the @c <TiffData> plane map; multi-file sets are resolved by
 * opening sibling files referenced by @c <UUID FileName="...">. Plain TIFFs are
 * treated as a flat stack of planes. This replaces ome-files for reading.
 */
class OmeTiffReader
{
public:
    OmeTiffReader();
    ~OmeTiffReader();

    OmeTiffReader(const OmeTiffReader &) = delete;
    OmeTiffReader &operator=(const OmeTiffReader &) = delete;

    bool open(const QString &path, QString *error = nullptr);
    void close();
    [[nodiscard]] bool isOpen() const
    {
        return !m_files.empty();
    }

    [[nodiscard]] bool isOmeTiff() const
    {
        return m_isOmeTiff;
    }
    [[nodiscard]] QString filename() const
    {
        return m_path;
    }
    /// Raw embedded OME-XML, or empty for plain TIFF (used by the writer).
    [[nodiscard]] QString sourceOmeXml() const
    {
        return m_sourceXml;
    }
    [[nodiscard]] bool hasMetadata() const
    {
        return m_hasMetadata;
    }
    [[nodiscard]] const ImageMetadata &parsedMetadata() const
    {
        return m_metadata;
    }

    // Raw (file) geometry, before any interleaving reinterpretation.
    [[nodiscard]] dimension_size_type rawSizeX() const { return m_rawSizeX; }
    [[nodiscard]] dimension_size_type rawSizeY() const { return m_rawSizeY; }
    [[nodiscard]] dimension_size_type rawSizeZ() const { return m_rawSizeZ; }
    [[nodiscard]] dimension_size_type rawSizeT() const { return m_rawSizeT; }
    [[nodiscard]] dimension_size_type rawSizeC() const { return m_rawSizeC; }
    [[nodiscard]] dimension_size_type rawImageCount() const { return m_rawImageCount; }
    [[nodiscard]] dimension_size_type rawRGBChannelCount() const { return m_rawRGBChannelCount; }
    [[nodiscard]] PixelType pixelType() const { return m_pixelType; }

    /// Linear plane index for (z,c,t) in the source dimension order.
    [[nodiscard]] dimension_size_type getIndex(
        dimension_size_type z,
        dimension_size_type c,
        dimension_size_type t) const;

    /// Read the plane at a linear file index (resolving file + IFD).
    [[nodiscard]] PlaneData readPlaneByFileIndex(dimension_size_type planeIndex, QString *error = nullptr);

private:
    TiffFile *fileFor(const QString &absPath);

    QString m_path;
    bool m_isOmeTiff = false;
    bool m_hasMetadata = false;
    QString m_sourceXml;
    ImageMetadata m_metadata;

    DimensionOrder m_dimensionOrder = DimensionOrder::XYZCT;
    dimension_size_type m_rawSizeX = 0;
    dimension_size_type m_rawSizeY = 0;
    dimension_size_type m_rawSizeZ = 0;
    dimension_size_type m_rawSizeT = 0;
    dimension_size_type m_rawSizeC = 0;
    dimension_size_type m_rawImageCount = 0;
    dimension_size_type m_rawRGBChannelCount = 1;
    PixelType m_pixelType = PixelType::UInt8;

    // Linear plane index -> (absolute file path, IFD). Empty => identity on master.
    std::vector<std::pair<QString, int>> m_planeMap;

    // Open file handles, keyed by absolute path (includes the master).
    std::map<QString, std::unique_ptr<TiffFile>> m_files;
};

} // namespace omr
