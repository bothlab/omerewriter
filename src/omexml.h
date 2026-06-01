/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <vector>

#include "ometiffimage.h" // ImageMetadata / ChannelParams
#include "ometypes.h"

namespace omr
{

/**
 * @brief One @c <TiffData> mapping entry from an OME-XML block.
 *
 * Maps a run of @c planeCount logical planes (starting at the given
 * First[ZCT]) onto consecutive IFDs starting at @c ifd in the file named by
 * @c fileName (empty means "this file").
 */
struct TiffDataEntry {
    int firstZ = 0;
    int firstC = 0;
    int firstT = 0;
    int ifd = 0;
    int planeCount = 0; // 0 == "until the end"
    QString fileName;   // empty == same file
};

/**
 * @brief Parsed contents of an embedded OME-XML block.
 */
struct OmeXmlInfo {
    bool valid = false;

    int sizeX = 0;
    int sizeY = 0;
    int sizeZ = 1;
    int sizeC = 1;
    int sizeT = 1;
    PixelType pixelType = PixelType::UInt8;
    DimensionOrder dimensionOrder = DimensionOrder::XYZCT;
    int significantBits = 0;
    bool bigEndian = false;
    bool interleaved = false;
    unsigned int samplesPerPixel = 1;

    ImageMetadata metadata;             // app-facing metadata (name, sizes, channels, optics)
    std::vector<TiffDataEntry> tiffData; // may be empty (implicit IFD ordering)
};

/// A logical plane position, used to drive both pixel and TiffData ordering.
struct PlaneCoord {
    int z = 0;
    int c = 0;
    int t = 0;
};

namespace omexml
{

/// Parse an OME-XML string. Returns an info struct with @c valid=false on failure.
OmeXmlInfo parse(const QString &xml);

/// Linear plane index for (z,c,t) under a dimension order (X/Y excluded).
int linearPlaneIndex(DimensionOrder order, int z, int c, int t, int sizeZ, int sizeC, int sizeT);

/// Ordered list of (z,c,t) for output, XYZCT (Z fastest, then C, then T).
std::vector<PlaneCoord> planeOrderXYZCT(int sizeZ, int sizeC, int sizeT);

/// Build a fresh OME-XML document (used when the source has no OME-XML).
QString buildFresh(
    const ImageMetadata &meta,
    int sizeX,
    int sizeY,
    int sizeZ,
    int sizeC,
    int sizeT,
    PixelType pixelType,
    unsigned int samplesPerPixel,
    const std::vector<PlaneCoord> &planeOrder,
    const QString &fileName);

/// Patch an existing OME-XML document, preserving metadata we do not edit.
/// Returns an empty string if @p srcXml cannot be parsed.
QString patch(
    const QString &srcXml,
    const ImageMetadata &meta,
    const std::vector<PlaneCoord> &planeOrder,
    const QString &fileName);

} // namespace omexml

} // namespace omr
