/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <expected>
#include <functional>

#include "ometiffimage.h" // ImageMetadata, OMETiffImage::ProgressCallback
#include "ometiffreader.h"
#include "ometypes.h"

namespace omr
{

/**
 * @brief Writes an OME-TIFF using libtiff + a hand-built OME-XML block.
 *
 * Pixel planes are copied verbatim from @p reader; the OME-XML is freshly
 * generated for plain-TIFF sources or patched from the source OME-XML so that
 * unedited metadata is preserved. This replaces the ome-files OMETIFFWriter.
 */
namespace ometiffwriter
{

/// Maps logical (z,c,t) to the reader's linear source plane index.
using PlaneIndexFn = std::function<dimension_size_type(dimension_size_type, dimension_size_type, dimension_size_type)>;

std::expected<bool, QString> write(
    const QString &outputPath,
    OmeTiffReader &reader,
    const ImageMetadata &meta,
    dimension_size_type sizeZ,
    dimension_size_type sizeC,
    dimension_size_type sizeT,
    PixelType pixelType,
    unsigned int samplesPerPixel,
    const PlaneIndexFn &planeIndexFn,
    const OMETiffImage::ProgressCallback &progress);

} // namespace ometiffwriter

} // namespace omr
