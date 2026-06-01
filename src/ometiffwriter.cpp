/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ometiffwriter.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include "omexml.h"
#include "tifffile.h"

namespace omr::ometiffwriter
{

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
    const OMETiffImage::ProgressCallback &progress)
{
    if (samplesPerPixel < 1)
        samplesPerPixel = 1;

    // One ordered (z,c,t) list drives both the OME-XML TiffData and the pixel
    // write loop, so IFD indices and plane coordinates always line up.
    const std::vector<PlaneCoord> planeOrder =
        omexml::planeOrderXYZCT(static_cast<int>(sizeZ), static_cast<int>(sizeC), static_cast<int>(sizeT));
    const dimension_size_type totalPlanes = planeOrder.size();
    if (totalPlanes == 0)
        return std::unexpected(QStringLiteral("Nothing to write: image has no planes"));

    const QString fileName = QFileInfo(outputPath).fileName();

    // Build the OME-XML up front (we know everything before writing pixels).
    const QString sourceXml = reader.sourceOmeXml();
    QString xml;
    if (sourceXml.isEmpty()) {
        xml = omexml::buildFresh(
            meta,
            static_cast<int>(reader.rawSizeX()),
            static_cast<int>(reader.rawSizeY()),
            static_cast<int>(sizeZ),
            static_cast<int>(sizeC),
            static_cast<int>(sizeT),
            pixelType,
            samplesPerPixel,
            planeOrder,
            fileName);
    } else {
        xml = omexml::patch(sourceXml, meta, planeOrder, fileName);
        if (xml.isEmpty()) {
            // Source XML could not be parsed; fall back to a fresh document.
            qWarning().noquote() << "OmeTiffWriter: failed to patch source OME-XML, generating fresh metadata";
            xml = omexml::buildFresh(
                meta,
                static_cast<int>(reader.rawSizeX()),
                static_cast<int>(reader.rawSizeY()),
                static_cast<int>(sizeZ),
                static_cast<int>(sizeC),
                static_cast<int>(sizeT),
                pixelType,
                samplesPerPixel,
                planeOrder,
                fileName);
        }
    }

    TiffFile out;
    QString err;
    if (!out.openWriteBigTiff(outputPath, &err))
        return std::unexpected(QStringLiteral("Failed to open output: %1").arg(err));

    for (dimension_size_type i = 0; i < totalPlanes; ++i) {
        if (progress && !progress(i, totalPlanes)) {
            out.close();
            QFile::remove(outputPath);
            return std::unexpected(QStringLiteral("Save operation cancelled by user"));
        }

        const PlaneCoord &p = planeOrder[i];
        const dimension_size_type srcIndex = planeIndexFn(
            static_cast<dimension_size_type>(p.z),
            static_cast<dimension_size_type>(p.c),
            static_cast<dimension_size_type>(p.t));

        PlaneData plane = reader.readPlaneByFileIndex(srcIndex, &err);
        if (!plane.isValid()) {
            out.close();
            QFile::remove(outputPath);
            return std::unexpected(QStringLiteral("Failed to read source plane %1: %2").arg(srcIndex).arg(err));
        }

        const QString &description = (i == 0) ? xml : QString();
        if (!out.writePlane(
                reinterpret_cast<const uint8_t *>(plane.bytes.constData()),
                static_cast<dimension_size_type>(plane.bytes.size()),
                plane.width,
                plane.height,
                pixelType,
                samplesPerPixel,
                description,
                &err)) {
            out.close();
            QFile::remove(outputPath);
            return std::unexpected(QStringLiteral("Failed to write plane %1: %2").arg(i).arg(err));
        }
    }

    out.close();
    qDebug().noquote() << "Saved OME-TIFF to" << outputPath;
    return true;
}

} // namespace omr::ometiffwriter
