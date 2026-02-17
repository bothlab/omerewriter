/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include <ome/files/FormatReader.h>

/**
 * @brief Simple struct to hold raw image data for display purposes.
 */
struct RawImage {
    QByteArray data;       /// Raw pixel data
    int width = 0;         /// Image width in pixels
    int height = 0;        /// Image height in pixels
    int channels = 1;      /// Number of channels (1=grayscale, 3=RGB, 4=RGBA)
    int bytesPerChannel = 1; /// Bytes per channel (1=8-bit, 2=16-bit)

    [[nodiscard]] bool isEmpty() const { return data.isEmpty() || width == 0 || height == 0; }
    [[nodiscard]] size_t dataSize() const { return static_cast<size_t>(width) * height * channels * bytesPerChannel; }
};

/**
 * @brief Wrapper class for reading OME-TIFF files using ome-files library.
 *
 * This class provides on-demand loading of individual planes from OME-TIFF files.
 */
class OMETiffReader : public QObject
{
    Q_OBJECT

public:
    using dimension_size_type = ome::files::dimension_size_type;

    explicit OMETiffReader(QObject *parent = nullptr);
    ~OMETiffReader() override;

    /**
     * @brief Open an OME-TIFF file.
     * @param filename Path to the OME-TIFF file.
     * @return true if file was opened successfully, false otherwise.
     */
    bool open(const QString &filename);

    /**
     * @brief Close the currently open file.
     */
    void close();

    /**
     * @brief Check if a file is currently open.
     */
    [[nodiscard]] bool isOpen() const;

    /**
     * @brief Get the filename of the currently open file.
     */
    [[nodiscard]] QString filename() const;

    // Dimension accessors
    [[nodiscard]] dimension_size_type sizeX() const;
    [[nodiscard]] dimension_size_type sizeY() const;
    [[nodiscard]] dimension_size_type sizeZ() const;
    [[nodiscard]] dimension_size_type sizeT() const;
    [[nodiscard]] dimension_size_type sizeC() const;

    /**
     * @brief Get the total number of planes in the current series.
     */
    [[nodiscard]] dimension_size_type imageCount() const;

    /**
     * @brief Get the pixel type of the image.
     */
    [[nodiscard]] ome::xml::model::enums::PixelType pixelType() const;

    /**
     * @brief Get the number of RGB channels (typically 1 for grayscale, 3 for RGB).
     */
    [[nodiscard]] dimension_size_type rgbChannelCount() const;

    /**
     * @brief Calculate the plane index from Z, C, T coordinates.
     * @param z Z position
     * @param c Channel position
     * @param t Time position
     * @return Plane index
     */
    [[nodiscard]] dimension_size_type getIndex(dimension_size_type z,
                                                dimension_size_type c,
                                                dimension_size_type t) const;

    /**
     * @brief Read a specific plane and return as RawImage for display.
     *
     * This reads just one 2D slice from the file, which is memory efficient
     * for large multi-dimensional datasets.
     *
     * @param z Z position
     * @param c Channel position
     * @param t Time position
     * @return RawImage containing the plane data
     */
    [[nodiscard]] RawImage readPlane(dimension_size_type z,
                                      dimension_size_type c,
                                      dimension_size_type t);

    /**
     * @brief Read a plane by its index.
     * @param planeIndex The plane index
     * @return RawImage containing the plane data
     */
    [[nodiscard]] RawImage readPlaneByIndex(dimension_size_type planeIndex);

    /**
     * @brief Get the underlying reader.
     */
    [[nodiscard]] std::shared_ptr<ome::files::FormatReader> reader() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};
