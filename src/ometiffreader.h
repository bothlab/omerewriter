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
#include <ome/xml/meta/OMEXMLMetadata.h>

/**
 * @brief Structure to hold channel-specific microscopy parameters
 */
struct ChannelParams {
    QString name; /// channel name
    ome::xml::model::enums::AcquisitionMode acquisitionMode = ome::xml::model::enums::AcquisitionMode::OTHER;
    double exWavelengthNm = 0;
    double emWavelengthNm = 0;
    double pinholeSizeNm = 0;
    bool isMultiPhoton = false;
    int photonCount = 1;
};

/**
 * @brief Structure to hold image-level microscopy metadata
 */
struct ImageMetadata {
    QString imageName;

    // Dimensions
    int sizeX = 0;
    int sizeY = 0;
    int sizeZ = 0;
    int sizeC = 0;
    int sizeT = 0;
    QString pixelType;
    size_t dataSizeBytes = 0;

    // Physical sizes (in nm)
    double physSizeXNm = 0;
    double physSizeYNm = 0;
    double physSizeZNm = 0;

    // Optical parameters
    double numericalAperture = 0;
    ome::xml::model::enums::Immersion lensImmersion = ome::xml::model::enums::Immersion::WATER;
    ome::xml::model::enums::Medium embeddingMedium = ome::xml::model::enums::Medium::WATER;
    double immersionRI = 1.0; /// Refractive index of the immersion medium

    // Channel parameters
    std::vector<ChannelParams> channels;
};

/**
 * @brief Simple struct to hold raw image data for display purposes.
 */
struct RawImage {
    QByteArray data;         /// Raw pixel data
    int width = 0;           /// Image width in pixels
    int height = 0;          /// Image height in pixels
    int channels = 1;        /// Number of channels (1=grayscale, 3=RGB, 4=RGBA)
    int bytesPerChannel = 1; /// Bytes per channel (1=8-bit, 2=16-bit)

    [[nodiscard]] bool isEmpty() const
    {
        return data.isEmpty() || width == 0 || height == 0;
    }
    [[nodiscard]] size_t dataSize() const
    {
        return static_cast<size_t>(width) * height * channels * bytesPerChannel;
    }
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

    /**
     * @brief Check if the currently open file is an OME-TIFF.
     * @return true if it's an OME-TIFF, false otherwise.
     */
    [[nodiscard]] bool isOmeTiff() const;

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
    [[nodiscard]] dimension_size_type getIndex(dimension_size_type z, dimension_size_type c, dimension_size_type t)
        const;

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
    [[nodiscard]] RawImage readPlane(dimension_size_type z, dimension_size_type c, dimension_size_type t);

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

    /**
     * @brief Get the OME-XML metadata object.
     */
    [[nodiscard]] std::shared_ptr<ome::xml::meta::OMEXMLMetadata> omeMetadata() const;

    /**
     * @brief Extract metadata from the loaded file into a convenient structure.
     * @param imageIndex The image/series index (default 0)
     * @return ImageMetadata structure with extracted values
     */
    [[nodiscard]] ImageMetadata extractMetadata(dimension_size_type imageIndex = 0) const;

    /**
     * @brief Save the current file with modified metadata to a new location.
     * @param outputPath Path for the output OME-TIFF file
     * @param metadata Modified metadata to apply
     * @return true if save was successful, false otherwise
     */
    bool saveWithMetadata(const QString &outputPath, const ImageMetadata &metadata);

private:
    class Private;
    std::unique_ptr<Private> d;
};
