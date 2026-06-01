/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ometiffreader.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include "omexml.h"

namespace omr
{

OmeTiffReader::OmeTiffReader() = default;
OmeTiffReader::~OmeTiffReader() = default;

void OmeTiffReader::close()
{
    m_files.clear();
    m_planeMap.clear();
    m_path.clear();
    m_sourceXml.clear();
    m_metadata = ImageMetadata();
    m_isOmeTiff = false;
    m_hasMetadata = false;
    m_rawSizeX = m_rawSizeY = m_rawSizeZ = m_rawSizeT = m_rawSizeC = 0;
    m_rawImageCount = 0;
    m_rawRGBChannelCount = 1;
    m_pixelType = PixelType::UInt8;
    m_dimensionOrder = DimensionOrder::XYZCT;
}

TiffFile *OmeTiffReader::fileFor(const QString &absPath)
{
    auto it = m_files.find(absPath);
    if (it != m_files.end())
        return it->second.get();

    auto tf = std::make_unique<TiffFile>();
    QString err;
    if (!tf->openRead(absPath, &err)) {
        qWarning().noquote() << "OmeTiffReader: cannot open referenced file" << absPath << ":" << err;
        return nullptr;
    }
    TiffFile *raw = tf.get();
    m_files.emplace(absPath, std::move(tf));
    return raw;
}

bool OmeTiffReader::open(const QString &path, QString *error)
{
    close();

    QFileInfo fi(path);
    if (!fi.exists()) {
        if (error)
            *error = QStringLiteral("File does not exist: %1").arg(path);
        return false;
    }
    m_path = fi.absoluteFilePath();

    TiffFile *master = fileFor(m_path);
    if (!master) {
        if (error)
            *error = QStringLiteral("Could not open TIFF: %1").arg(path);
        return false;
    }

    const bool looksOme =
        path.endsWith(QLatin1String(".ome.tiff"), Qt::CaseInsensitive) || path.endsWith(QLatin1String(".ome.tif"), Qt::CaseInsensitive);

    OmeXmlInfo info;
    if (looksOme) {
        master->setDirectory(0);
        m_sourceXml = master->imageDescription();
        info = omexml::parse(m_sourceXml);
    }

    if (info.valid) {
        m_isOmeTiff = true;
        m_hasMetadata = true;
        m_metadata = info.metadata;
        m_dimensionOrder = info.dimensionOrder;
        m_rawSizeX = info.sizeX;
        m_rawSizeY = info.sizeY;
        m_rawSizeZ = info.sizeZ;
        m_rawSizeC = info.sizeC;
        m_rawSizeT = info.sizeT;
        m_rawImageCount = static_cast<dimension_size_type>(info.sizeZ) * info.sizeC * info.sizeT;
        m_rawRGBChannelCount = info.samplesPerPixel > 0 ? info.samplesPerPixel : 1;
        m_pixelType = info.pixelType;

        // Build the linear-plane -> (file, IFD) map from TiffData.
        if (!info.tiffData.empty()) {
            m_planeMap.assign(m_rawImageCount, {m_path, 0});
            const QDir baseDir = QFileInfo(m_path).absoluteDir();
            for (const auto &td : info.tiffData) {
                const QString absFile =
                    td.fileName.isEmpty() ? m_path : baseDir.absoluteFilePath(td.fileName);
                const int start = omexml::linearPlaneIndex(
                    m_dimensionOrder, td.firstZ, td.firstC, td.firstT, info.sizeZ, info.sizeC, info.sizeT);
                int count = td.planeCount;
                if (count <= 0)
                    count = static_cast<int>(m_rawImageCount) - start;
                for (int k = 0; k < count; ++k) {
                    const int linear = start + k;
                    if (linear >= 0 && linear < static_cast<int>(m_planeMap.size()))
                        m_planeMap[linear] = {absFile, td.ifd + k};
                }
            }
        }
        // else: empty map => identity mapping on the master file.
    } else {
        // Plain TIFF: a flat stack of planes, all Z by default.
        m_isOmeTiff = false;
        m_hasMetadata = false;
        master->setDirectory(0);
        const PlaneFormat fmt = master->currentFormat();
        m_rawSizeX = fmt.width;
        m_rawSizeY = fmt.height;
        m_rawImageCount = master->directoryCount();
        m_rawSizeZ = m_rawImageCount;
        m_rawSizeC = 1;
        m_rawSizeT = 1;
        m_rawRGBChannelCount = fmt.samplesPerPixel > 0 ? fmt.samplesPerPixel : 1;
        m_pixelType = pixelTypeFromTiff(fmt.bitsPerSample, fmt.sampleKind);
        m_dimensionOrder = DimensionOrder::XYZCT;
    }

    return true;
}

dimension_size_type OmeTiffReader::getIndex(
    dimension_size_type z,
    dimension_size_type c,
    dimension_size_type t) const
{
    return static_cast<dimension_size_type>(omexml::linearPlaneIndex(
        m_dimensionOrder,
        static_cast<int>(z),
        static_cast<int>(c),
        static_cast<int>(t),
        static_cast<int>(m_rawSizeZ),
        static_cast<int>(m_rawSizeC),
        static_cast<int>(m_rawSizeT)));
}

PlaneData OmeTiffReader::readPlaneByFileIndex(dimension_size_type planeIndex, QString *error)
{
    PlaneData out;

    QString filePath = m_path;
    int ifd = static_cast<int>(planeIndex);
    if (!m_planeMap.empty()) {
        if (planeIndex >= m_planeMap.size()) {
            if (error)
                *error = QStringLiteral("Plane index %1 out of range").arg(planeIndex);
            return out;
        }
        filePath = m_planeMap[planeIndex].first;
        ifd = m_planeMap[planeIndex].second;
    }

    TiffFile *tf = fileFor(filePath);
    if (!tf) {
        if (error)
            *error = QStringLiteral("Could not open file for plane %1: %2").arg(planeIndex).arg(filePath);
        return out;
    }
    if (!tf->setDirectory(static_cast<dimension_size_type>(ifd))) {
        if (error)
            *error = QStringLiteral("No IFD %1 in %2").arg(ifd).arg(filePath);
        return out;
    }

    const PlaneFormat fmt = tf->currentFormat();
    if (!tf->readCurrentPlane(out.bytes, error))
        return PlaneData();

    out.width = fmt.width;
    out.height = fmt.height;
    out.samplesPerPixel = fmt.samplesPerPixel;
    out.bitsPerSample = fmt.bitsPerSample;
    out.pixelType = pixelTypeFromTiff(fmt.bitsPerSample, fmt.sampleKind);
    return out;
}

} // namespace omr
