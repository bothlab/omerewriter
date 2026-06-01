/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "tifffile.h"

#include <QDebug>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include <tiffio.h>

namespace omr
{

namespace
{

// libtiff reports errors through global C callbacks. We funnel the most recent
// message into a thread-local string so the wrapper can surface it.
thread_local std::string g_lastTiffError;

void tiffErrorHandler(const char *module, const char *fmt, va_list ap)
{
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    g_lastTiffError = (module ? std::string(module) + ": " : std::string()) + buf;
}

void tiffWarningHandler(const char * /*module*/, const char * /*fmt*/, va_list /*ap*/)
{
    // Warnings (e.g. unknown tags) are non-fatal; ignore to keep output clean.
}

struct TiffHandlerInit {
    TiffHandlerInit()
    {
        TIFFSetErrorHandler(tiffErrorHandler);
        TIFFSetWarningHandler(tiffWarningHandler);
    }
};
const TiffHandlerInit g_handlerInit;

void setError(QString *error, const QString &fallback)
{
    if (!error)
        return;
    if (!g_lastTiffError.empty())
        *error = QString::fromStdString(g_lastTiffError);
    else
        *error = fallback;
}

uint16_t sampleFormatTag(PixelType t)
{
    switch (pixelTypeSampleKind(t)) {
    case SampleKind::UInt:    return SAMPLEFORMAT_UINT;
    case SampleKind::Int:     return SAMPLEFORMAT_INT;
    case SampleKind::Float:   return SAMPLEFORMAT_IEEEFP;
    case SampleKind::Complex: return SAMPLEFORMAT_COMPLEXIEEEFP;
    }
    return SAMPLEFORMAT_UINT;
}

TIFF *open_tiff(const QString &path, const char *mode)
{
    g_lastTiffError.clear();
#ifdef Q_OS_WIN
    return TIFFOpenW(reinterpret_cast<const wchar_t *>(path.utf16()), mode);
#else
    return TIFFOpen(path.toUtf8().constData(), mode);
#endif
}

} // namespace

PixelType pixelTypeFromTiff(unsigned int bitsPerSample, SampleKind kind)
{
    switch (kind) {
    case SampleKind::Int:
        if (bitsPerSample <= 8)
            return PixelType::Int8;
        if (bitsPerSample <= 16)
            return PixelType::Int16;
        return PixelType::Int32;
    case SampleKind::Float:
        return bitsPerSample <= 32 ? PixelType::Float : PixelType::Double;
    case SampleKind::Complex:
        return bitsPerSample <= 64 ? PixelType::ComplexFloat : PixelType::ComplexDouble;
    case SampleKind::UInt:
    default:
        if (bitsPerSample == 1)
            return PixelType::Bit;
        if (bitsPerSample <= 8)
            return PixelType::UInt8;
        if (bitsPerSample <= 16)
            return PixelType::UInt16;
        return PixelType::UInt32;
    }
}

TiffFile::~TiffFile()
{
    close();
}

void TiffFile::close()
{
    if (m_tiff) {
        TIFFClose(static_cast<TIFF *>(m_tiff));
        m_tiff = nullptr;
    }
}

bool TiffFile::openRead(const QString &path, QString *error)
{
    close();
    m_tiff = open_tiff(path, "r");
    if (!m_tiff) {
        setError(error, QStringLiteral("Could not open TIFF for reading: %1").arg(path));
        return false;
    }
    return true;
}

bool TiffFile::openWriteBigTiff(const QString &path, QString *error)
{
    close();
    // "w8" selects BigTIFF (64-bit offsets) so we can write files > 4 GiB.
    m_tiff = open_tiff(path, "w8");
    if (!m_tiff) {
        setError(error, QStringLiteral("Could not open TIFF for writing: %1").arg(path));
        return false;
    }
    return true;
}

dimension_size_type TiffFile::directoryCount()
{
    if (!m_tiff)
        return 0;
    auto *tif = static_cast<TIFF *>(m_tiff);
    // TIFFNumberOfDirectories moves the current directory, so save/restore it.
    const tdir_t current = TIFFCurrentDirectory(tif);
    const auto count = static_cast<dimension_size_type>(TIFFNumberOfDirectories(tif));
    TIFFSetDirectory(tif, current);
    return count;
}

bool TiffFile::setDirectory(dimension_size_type index)
{
    if (!m_tiff)
        return false;
    return TIFFSetDirectory(static_cast<TIFF *>(m_tiff), static_cast<tdir_t>(index)) != 0;
}

PlaneFormat TiffFile::currentFormat() const
{
    PlaneFormat fmt;
    if (!m_tiff)
        return fmt;
    auto *tif = static_cast<TIFF *>(m_tiff);

    uint32_t w = 0, h = 0;
    uint16_t spp = 1, bps = 8, sf = SAMPLEFORMAT_UINT;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bps);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sf);

    fmt.width = w;
    fmt.height = h;
    fmt.samplesPerPixel = spp;
    fmt.bitsPerSample = bps;
    switch (sf) {
    case SAMPLEFORMAT_INT:            fmt.sampleKind = SampleKind::Int; break;
    case SAMPLEFORMAT_IEEEFP:         fmt.sampleKind = SampleKind::Float; break;
    case SAMPLEFORMAT_COMPLEXINT:
    case SAMPLEFORMAT_COMPLEXIEEEFP:  fmt.sampleKind = SampleKind::Complex; break;
    case SAMPLEFORMAT_UINT:
    default:                          fmt.sampleKind = SampleKind::UInt; break;
    }
    return fmt;
}

QString TiffFile::imageDescription() const
{
    if (!m_tiff)
        return {};
    char *desc = nullptr;
    if (TIFFGetField(static_cast<TIFF *>(m_tiff), TIFFTAG_IMAGEDESCRIPTION, &desc) && desc)
        return QString::fromUtf8(desc);
    return {};
}

bool TiffFile::readCurrentPlane(QByteArray &out, QString *error)
{
    if (!m_tiff) {
        setError(error, QStringLiteral("No TIFF open"));
        return false;
    }
    g_lastTiffError.clear();
    auto *tif = static_cast<TIFF *>(m_tiff);

    const PlaneFormat fmt = currentFormat();
    const dimension_size_type bytesPerSample = fmt.bitsPerSample <= 8 ? 1 : (fmt.bitsPerSample <= 16 ? 2 : (fmt.bitsPerSample <= 32 ? 4 : 8));
    const dimension_size_type rowBytes = fmt.width * fmt.samplesPerPixel * bytesPerSample;
    const dimension_size_type planeBytes = rowBytes * fmt.height;
    if (planeBytes == 0) {
        setError(error, QStringLiteral("TIFF directory has zero size"));
        return false;
    }
    out.resize(static_cast<qsizetype>(planeBytes));
    auto *dst = reinterpret_cast<uint8_t *>(out.data());

    if (TIFFIsTiled(tif)) {
        uint32_t tileW = 0, tileH = 0;
        TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileW);
        TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileH);
        if (tileW == 0 || tileH == 0) {
            setError(error, QStringLiteral("Invalid tile dimensions"));
            return false;
        }
        const tmsize_t tileBytes = TIFFTileSize(tif);
        QByteArray tileBuf(static_cast<qsizetype>(tileBytes), Qt::Uninitialized);
        const dimension_size_type tileRowBytes = static_cast<dimension_size_type>(tileW) * fmt.samplesPerPixel * bytesPerSample;

        for (uint32_t y0 = 0; y0 < fmt.height; y0 += tileH) {
            for (uint32_t x0 = 0; x0 < fmt.width; x0 += tileW) {
                if (TIFFReadTile(tif, tileBuf.data(), x0, y0, 0, 0) < 0) {
                    setError(error, QStringLiteral("Failed to read tile"));
                    return false;
                }
                const dimension_size_type copyW = std::min<dimension_size_type>(tileW, fmt.width - x0);
                const dimension_size_type copyH = std::min<dimension_size_type>(tileH, fmt.height - y0);
                const dimension_size_type copyRowBytes = copyW * fmt.samplesPerPixel * bytesPerSample;
                for (dimension_size_type ty = 0; ty < copyH; ++ty) {
                    const uint8_t *srcRow = reinterpret_cast<const uint8_t *>(tileBuf.constData()) + ty * tileRowBytes;
                    uint8_t *dstRow = dst + (y0 + ty) * rowBytes + x0 * fmt.samplesPerPixel * bytesPerSample;
                    std::memcpy(dstRow, srcRow, copyRowBytes);
                }
            }
        }
    } else {
        const tstrip_t nstrips = TIFFNumberOfStrips(tif);
        dimension_size_type offset = 0;
        for (tstrip_t s = 0; s < nstrips; ++s) {
            const tmsize_t read = TIFFReadEncodedStrip(tif, s, dst + offset, static_cast<tmsize_t>(-1));
            if (read < 0) {
                setError(error, QStringLiteral("Failed to read strip %1").arg(s));
                return false;
            }
            offset += static_cast<dimension_size_type>(read);
            if (offset >= planeBytes)
                break;
        }
    }
    return true;
}

bool TiffFile::writePlane(
    const uint8_t *data,
    dimension_size_type byteCount,
    dimension_size_type width,
    dimension_size_type height,
    PixelType pixelType,
    unsigned int samplesPerPixel,
    const QString &imageDescription,
    QString *error)
{
    if (!m_tiff) {
        setError(error, QStringLiteral("No TIFF open"));
        return false;
    }
    g_lastTiffError.clear();
    auto *tif = static_cast<TIFF *>(m_tiff);

    const unsigned int bits = pixelTypeBits(pixelType);
    const dimension_size_type bytesPerSample = pixelTypeBytes(pixelType);
    const dimension_size_type rowBytes = width * samplesPerPixel * bytesPerSample;
    if (byteCount < rowBytes * height) {
        setError(error, QStringLiteral("Plane buffer too small for %1x%2").arg(width).arg(height));
        return false;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(width));
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(height));
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, static_cast<uint16_t>(bits));
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, static_cast<uint16_t>(samplesPerPixel));
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, sampleFormatTag(pixelType));
    TIFFSetField(
        tif,
        TIFFTAG_PHOTOMETRIC,
        samplesPerPixel >= 3 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE);

    // Pick a sensible strip height (~rows for an 8 KiB target) and clamp it.
    uint32_t rowsPerStrip = TIFFDefaultStripSize(tif, 0);
    if (rowsPerStrip == 0)
        rowsPerStrip = 1;
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip);

    if (!imageDescription.isEmpty()) {
        const QByteArray utf8 = imageDescription.toUtf8();
        TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, utf8.constData());
    }

    // Write the plane strip by strip.
    const auto *base = data;
    for (dimension_size_type row = 0; row < height; row += rowsPerStrip) {
        const dimension_size_type rowsThisStrip = std::min<dimension_size_type>(rowsPerStrip, height - row);
        const tmsize_t stripBytes = static_cast<tmsize_t>(rowsThisStrip * rowBytes);
        const tstrip_t strip = static_cast<tstrip_t>(row / rowsPerStrip);
        if (TIFFWriteEncodedStrip(tif, strip, const_cast<uint8_t *>(base + row * rowBytes), stripBytes) < 0) {
            setError(error, QStringLiteral("Failed to write strip at row %1").arg(row));
            return false;
        }
    }

    if (!TIFFWriteDirectory(tif)) {
        setError(error, QStringLiteral("Failed to finalize TIFF directory"));
        return false;
    }
    return true;
}

} // namespace omr
