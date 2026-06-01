/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

#include "ometypes.h"

namespace omr
{

/**
 * @brief Pixel layout of a single TIFF directory (one 2D plane).
 */
struct PlaneFormat {
    dimension_size_type width = 0;
    dimension_size_type height = 0;
    unsigned int samplesPerPixel = 1;
    unsigned int bitsPerSample = 8;
    SampleKind sampleKind = SampleKind::UInt;
};

/// Best-effort mapping from raw TIFF tags to an OME pixel type.
PixelType pixelTypeFromTiff(unsigned int bitsPerSample, SampleKind kind);

/**
 * @brief Thin RAII wrapper around a libtiff @c TIFF* handle.
 *
 * Handles reading decoded planes (strips and tiles) and writing single-plane
 * directories as BigTIFF. This replaces the TIFF container handling that was
 * previously done inside the ome-files library.
 */
class TiffFile
{
public:
    TiffFile() = default;
    ~TiffFile();

    TiffFile(const TiffFile &) = delete;
    TiffFile &operator=(const TiffFile &) = delete;

    bool openRead(const QString &path, QString *error = nullptr);
    bool openWriteBigTiff(const QString &path, QString *error = nullptr);
    void close();
    [[nodiscard]] bool isOpen() const
    {
        return m_tiff != nullptr;
    }

    // --- read side (operate on the "current" directory) ---

    [[nodiscard]] dimension_size_type directoryCount();
    bool setDirectory(dimension_size_type index);
    [[nodiscard]] PlaneFormat currentFormat() const;
    /// ImageDescription of the current directory (empty if absent).
    [[nodiscard]] QString imageDescription() const;
    /// Read the decoded, native-endian pixel bytes of the current directory.
    bool readCurrentPlane(QByteArray &out, QString *error = nullptr);

    // --- write side ---

    /**
     * @brief Append one plane as a new directory and finalize it.
     * @param imageDescription OME-XML to store in the ImageDescription tag;
     *        pass an empty string for every directory except the first.
     */
    bool writePlane(
        const uint8_t *data,
        dimension_size_type byteCount,
        dimension_size_type width,
        dimension_size_type height,
        PixelType pixelType,
        unsigned int samplesPerPixel,
        const QString &imageDescription,
        QString *error = nullptr);

private:
    void *m_tiff = nullptr; // TIFF*
};

} // namespace omr
