/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/**
 * Self-contained OME data types.
 *
 * These replace the enums and primitives we previously pulled in from the OME
 * C++ model library (ome-model). The enumeration value order and the canonical
 * string names match the OME-XML 2016-06 schema exactly, so files and metadata
 * JSON stay compatible with bio-formats and earlier versions of this program.
 */
namespace omr
{

/// Plane/dimension counts. Was ome::files::dimension_size_type.
using dimension_size_type = std::size_t;

// ---------------------------------------------------------------------------
// PixelType
// ---------------------------------------------------------------------------

enum class PixelType {
    Int8,
    Int16,
    Int32,
    UInt8,
    UInt16,
    UInt32,
    Float,
    Double,
    ComplexFloat,
    ComplexDouble,
    Bit
};

/// How samples of a pixel type are interpreted (maps to TIFF SAMPLEFORMAT_*).
enum class SampleKind { UInt, Int, Float, Complex };

inline QString pixelTypeToString(PixelType t)
{
    switch (t) {
    case PixelType::Int8:          return QStringLiteral("int8");
    case PixelType::Int16:         return QStringLiteral("int16");
    case PixelType::Int32:         return QStringLiteral("int32");
    case PixelType::UInt8:         return QStringLiteral("uint8");
    case PixelType::UInt16:        return QStringLiteral("uint16");
    case PixelType::UInt32:        return QStringLiteral("uint32");
    case PixelType::Float:         return QStringLiteral("float");
    case PixelType::Double:        return QStringLiteral("double");
    case PixelType::ComplexFloat:  return QStringLiteral("complex");
    case PixelType::ComplexDouble: return QStringLiteral("double-complex");
    case PixelType::Bit:           return QStringLiteral("bit");
    }
    return QStringLiteral("uint8");
}

inline PixelType pixelTypeFromString(const QString &s, PixelType fallback = PixelType::UInt8)
{
    const QString v = s.trimmed().toLower();
    if (v == QLatin1String("int8"))           return PixelType::Int8;
    if (v == QLatin1String("int16"))          return PixelType::Int16;
    if (v == QLatin1String("int32"))          return PixelType::Int32;
    if (v == QLatin1String("uint8"))          return PixelType::UInt8;
    if (v == QLatin1String("uint16"))         return PixelType::UInt16;
    if (v == QLatin1String("uint32"))         return PixelType::UInt32;
    if (v == QLatin1String("float"))          return PixelType::Float;
    if (v == QLatin1String("double"))         return PixelType::Double;
    if (v == QLatin1String("complex"))        return PixelType::ComplexFloat;
    if (v == QLatin1String("double-complex")) return PixelType::ComplexDouble;
    if (v == QLatin1String("bit"))            return PixelType::Bit;
    return fallback;
}

/// Bytes occupied by one sample of the given pixel type.
inline unsigned int pixelTypeBytes(PixelType t)
{
    switch (t) {
    case PixelType::Int8:
    case PixelType::UInt8:
    case PixelType::Bit:           return 1;
    case PixelType::Int16:
    case PixelType::UInt16:        return 2;
    case PixelType::Int32:
    case PixelType::UInt32:
    case PixelType::Float:         return 4;
    case PixelType::Double:
    case PixelType::ComplexFloat:  return 8;
    case PixelType::ComplexDouble: return 16;
    }
    return 1;
}

/// Bits per sample as stored in the TIFF BitsPerSample tag / OME SignificantBits.
inline unsigned int pixelTypeBits(PixelType t)
{
    return pixelTypeBytes(t) * 8;
}

inline SampleKind pixelTypeSampleKind(PixelType t)
{
    switch (t) {
    case PixelType::Int8:
    case PixelType::Int16:
    case PixelType::Int32:         return SampleKind::Int;
    case PixelType::UInt8:
    case PixelType::UInt16:
    case PixelType::UInt32:
    case PixelType::Bit:           return SampleKind::UInt;
    case PixelType::Float:
    case PixelType::Double:        return SampleKind::Float;
    case PixelType::ComplexFloat:
    case PixelType::ComplexDouble: return SampleKind::Complex;
    }
    return SampleKind::UInt;
}

// ---------------------------------------------------------------------------
// AcquisitionMode
// ---------------------------------------------------------------------------

enum class AcquisitionMode {
    WideField,
    LaserScanningConfocalMicroscopy,
    SpinningDiskConfocal,
    SlitScanConfocal,
    MultiPhotonMicroscopy,
    StructuredIllumination,
    SingleMoleculeImaging,
    TotalInternalReflection,
    FluorescenceLifetime,
    SpectralImaging,
    FluorescenceCorrelationSpectroscopy,
    NearFieldScanningOpticalMicroscopy,
    SecondHarmonicGenerationImaging,
    PALM,
    STORM,
    STED,
    TIRF,
    FSM,
    LCM,
    Other,
    BrightField,
    SweptFieldConfocal,
    SPIM
};

inline const std::vector<std::pair<AcquisitionMode, QString>> &acquisitionModeValues()
{
    static const std::vector<std::pair<AcquisitionMode, QString>> v = {
        {AcquisitionMode::WideField,                           QStringLiteral("WideField")                          },
        {AcquisitionMode::LaserScanningConfocalMicroscopy,     QStringLiteral("LaserScanningConfocalMicroscopy")    },
        {AcquisitionMode::SpinningDiskConfocal,                QStringLiteral("SpinningDiskConfocal")               },
        {AcquisitionMode::SlitScanConfocal,                    QStringLiteral("SlitScanConfocal")                   },
        {AcquisitionMode::MultiPhotonMicroscopy,               QStringLiteral("MultiPhotonMicroscopy")              },
        {AcquisitionMode::StructuredIllumination,              QStringLiteral("StructuredIllumination")             },
        {AcquisitionMode::SingleMoleculeImaging,               QStringLiteral("SingleMoleculeImaging")              },
        {AcquisitionMode::TotalInternalReflection,             QStringLiteral("TotalInternalReflection")            },
        {AcquisitionMode::FluorescenceLifetime,                QStringLiteral("FluorescenceLifetime")               },
        {AcquisitionMode::SpectralImaging,                     QStringLiteral("SpectralImaging")                    },
        {AcquisitionMode::FluorescenceCorrelationSpectroscopy, QStringLiteral("FluorescenceCorrelationSpectroscopy")},
        {AcquisitionMode::NearFieldScanningOpticalMicroscopy,  QStringLiteral("NearFieldScanningOpticalMicroscopy") },
        {AcquisitionMode::SecondHarmonicGenerationImaging,     QStringLiteral("SecondHarmonicGenerationImaging")    },
        {AcquisitionMode::PALM,                                QStringLiteral("PALM")                               },
        {AcquisitionMode::STORM,                               QStringLiteral("STORM")                              },
        {AcquisitionMode::STED,                                QStringLiteral("STED")                               },
        {AcquisitionMode::TIRF,                                QStringLiteral("TIRF")                               },
        {AcquisitionMode::FSM,                                 QStringLiteral("FSM")                                },
        {AcquisitionMode::LCM,                                 QStringLiteral("LCM")                                },
        {AcquisitionMode::Other,                               QStringLiteral("Other")                              },
        {AcquisitionMode::BrightField,                         QStringLiteral("BrightField")                        },
        {AcquisitionMode::SweptFieldConfocal,                  QStringLiteral("SweptFieldConfocal")                 },
        {AcquisitionMode::SPIM,                                QStringLiteral("SPIM")                               },
    };
    return v;
}

inline QString acquisitionModeToString(AcquisitionMode m)
{
    for (const auto &p : acquisitionModeValues())
        if (p.first == m)
            return p.second;
    return QStringLiteral("Other");
}

inline AcquisitionMode acquisitionModeFromString(
    const QString &s,
    AcquisitionMode fallback = AcquisitionMode::LaserScanningConfocalMicroscopy)
{
    const QString v = s.trimmed();
    for (const auto &p : acquisitionModeValues())
        if (p.second.compare(v, Qt::CaseInsensitive) == 0)
            return p.first;
    return fallback;
}

// ---------------------------------------------------------------------------
// Immersion
// ---------------------------------------------------------------------------

enum class Immersion { Oil, Water, WaterDipping, Air, Multi, Glycerol, Other };

inline const std::vector<std::pair<Immersion, QString>> &immersionValues()
{
    static const std::vector<std::pair<Immersion, QString>> v = {
        {Immersion::Oil,          QStringLiteral("Oil")         },
        {Immersion::Water,        QStringLiteral("Water")       },
        {Immersion::WaterDipping, QStringLiteral("WaterDipping")},
        {Immersion::Air,          QStringLiteral("Air")         },
        {Immersion::Multi,        QStringLiteral("Multi")       },
        {Immersion::Glycerol,     QStringLiteral("Glycerol")    },
        {Immersion::Other,        QStringLiteral("Other")       },
    };
    return v;
}

inline QString immersionToString(Immersion m)
{
    for (const auto &p : immersionValues())
        if (p.first == m)
            return p.second;
    return QStringLiteral("Other");
}

inline Immersion immersionFromString(const QString &s, Immersion fallback = Immersion::Water)
{
    const QString v = s.trimmed();
    for (const auto &p : immersionValues())
        if (p.second.compare(v, Qt::CaseInsensitive) == 0)
            return p.first;
    return fallback;
}

// ---------------------------------------------------------------------------
// Medium (objective settings embedding medium)
// ---------------------------------------------------------------------------

enum class Medium { Air, Oil, Water, Glycerol, Other };

inline const std::vector<std::pair<Medium, QString>> &mediumValues()
{
    static const std::vector<std::pair<Medium, QString>> v = {
        {Medium::Air,      QStringLiteral("Air")     },
        {Medium::Oil,      QStringLiteral("Oil")     },
        {Medium::Water,    QStringLiteral("Water")   },
        {Medium::Glycerol, QStringLiteral("Glycerol")},
        {Medium::Other,    QStringLiteral("Other")   },
    };
    return v;
}

inline QString mediumToString(Medium m)
{
    for (const auto &p : mediumValues())
        if (p.first == m)
            return p.second;
    return QStringLiteral("Other");
}

inline Medium mediumFromString(const QString &s, Medium fallback = Medium::Water)
{
    const QString v = s.trimmed();
    for (const auto &p : mediumValues())
        if (p.second.compare(v, Qt::CaseInsensitive) == 0)
            return p.first;
    return fallback;
}

// ---------------------------------------------------------------------------
// DimensionOrder
// ---------------------------------------------------------------------------

enum class DimensionOrder { XYZCT, XYZTC, XYCTZ, XYCZT, XYTCZ, XYTZC };

inline QString dimensionOrderToString(DimensionOrder o)
{
    switch (o) {
    case DimensionOrder::XYZCT: return QStringLiteral("XYZCT");
    case DimensionOrder::XYZTC: return QStringLiteral("XYZTC");
    case DimensionOrder::XYCTZ: return QStringLiteral("XYCTZ");
    case DimensionOrder::XYCZT: return QStringLiteral("XYCZT");
    case DimensionOrder::XYTCZ: return QStringLiteral("XYTCZ");
    case DimensionOrder::XYTZC: return QStringLiteral("XYTZC");
    }
    return QStringLiteral("XYZCT");
}

inline DimensionOrder dimensionOrderFromString(const QString &s, DimensionOrder fallback = DimensionOrder::XYZCT)
{
    const QString v = s.trimmed().toUpper();
    if (v == QLatin1String("XYZCT")) return DimensionOrder::XYZCT;
    if (v == QLatin1String("XYZTC")) return DimensionOrder::XYZTC;
    if (v == QLatin1String("XYCTZ")) return DimensionOrder::XYCTZ;
    if (v == QLatin1String("XYCZT")) return DimensionOrder::XYCZT;
    if (v == QLatin1String("XYTCZ")) return DimensionOrder::XYTCZ;
    if (v == QLatin1String("XYTZC")) return DimensionOrder::XYTZC;
    return fallback;
}

// ---------------------------------------------------------------------------
// Length units
// ---------------------------------------------------------------------------

/**
 * Convert a length value with an OME unit string to nanometres.
 *
 * Recognises the units we actually encounter (m, mm, µm/um/micron, nm). When
 * the unit is empty or unknown, @p defaultToMicrometre selects the OME default
 * for the field: physical pixel sizes default to micrometres, while a value
 * that is already in nanometres should pass @c false.
 */
inline double lengthToNm(double value, const QString &unit, bool defaultToMicrometre)
{
    const QString u = unit.trimmed();
    if (u.isEmpty())
        return defaultToMicrometre ? value * 1000.0 : value;
    if (u == QLatin1String("nm") || u.compare(QLatin1String("nanometer"), Qt::CaseInsensitive) == 0)
        return value;
    if (u == QString::fromUtf8("µm") || u == QLatin1String("um") || u == QLatin1String("micron")
        || u.compare(QLatin1String("micrometer"), Qt::CaseInsensitive) == 0)
        return value * 1000.0;
    if (u == QLatin1String("mm") || u.compare(QLatin1String("millimeter"), Qt::CaseInsensitive) == 0)
        return value * 1e6;
    if (u == QLatin1String("m") || u.compare(QLatin1String("meter"), Qt::CaseInsensitive) == 0)
        return value * 1e9;
    // Unknown unit: assume the field default.
    return defaultToMicrometre ? value * 1000.0 : value;
}

} // namespace omr
