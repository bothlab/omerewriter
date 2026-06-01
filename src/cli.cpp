/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * omerw-cli: a small headless utility to inspect and rewrite OME-TIFF files.
 * Useful for scripting and for verifying the TIFF/OME-XML I/O without the GUI.
 */

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include "metadatajson.h"
#include "ometiffimage.h"
#include "ometiffreader.h"
#include "ometypes.h"

namespace
{

QTextStream out(stdout);
QTextStream err(stderr);

int usage()
{
    err << "omerw-cli — inspect and rewrite OME-TIFF files\n\n"
        << "Usage:\n"
        << "  omerw-cli info <file>            Print dimensions and metadata\n"
        << "  omerw-cli xml  <file>            Print the embedded OME-XML\n"
        << "  omerw-cli convert <in> <out>     Rewrite <in> to <out> (preserving metadata)\n"
        << "  omerw-cli convert <in> <out> --meta <file.json>\n"
        << "                                   Rewrite, applying metadata overrides from JSON\n"
        << "  omerw-cli channels <file> <interleaved>\n"
        << "                                   Set interleaved channel count for a plain TIFF, then info\n";
    return 2;
}

QString boolStr(bool b)
{
    return b ? QStringLiteral("yes") : QStringLiteral("no");
}

void printMetadata(const ImageMetadata &m)
{
    out << "  name:        " << m.imageName << "\n";
    out << "  dimensions:  X=" << m.sizeX << " Y=" << m.sizeY << " Z=" << m.sizeZ << " C=" << m.sizeC
        << " T=" << m.sizeT << "\n";
    out << "  pixel type:  " << m.pixelType << "\n";
    out << "  data size:   " << m.dataSizeBytes << " bytes\n";
    out << "  phys size:   X=" << m.physSizeXNm << "nm Y=" << m.physSizeYNm << "nm Z=" << m.physSizeZNm << "nm\n";
    out << "  objective:   NA=" << m.numericalAperture << " immersion=" << omr::immersionToString(m.lensImmersion)
        << " medium=" << omr::mediumToString(m.embeddingMedium) << " RI=" << m.immersionRI << "\n";
    out << "  channels:    " << m.channels.size() << "\n";
    for (size_t i = 0; i < m.channels.size(); ++i) {
        const auto &c = m.channels[i];
        out << "    [" << i << "] " << c.name << "  mode=" << omr::acquisitionModeToString(c.acquisitionMode)
            << " ex=" << c.exWavelengthNm << "nm em=" << c.emWavelengthNm << "nm pinhole=" << c.pinholeSizeNm
            << "nm\n";
    }
}

int cmdInfo(const QString &path)
{
    OMETiffImage img;
    if (!img.open(path)) {
        err << "Failed to open: " << path << "\n";
        return 1;
    }
    out << "File: " << path << "\n";
    out << "  OME-TIFF:    " << boolStr(img.isOmeTiff()) << "\n";
    out << "  raw planes:  " << img.rawImageCount() << "\n";
    out << "  effective:   X=" << img.sizeX() << " Y=" << img.sizeY() << " Z=" << img.sizeZ() << " C=" << img.sizeC()
        << " T=" << img.sizeT() << "\n";
    out << "  RGB samples: " << img.rgbChannelCount() << "\n";
    printMetadata(img.extractMetadata());

    const RawImage p = img.readPlane(0, 0, 0);
    out << "  plane(0,0,0): " << p.width << "x" << p.height << " ch=" << p.channels << " bpc=" << p.bytesPerChannel
        << " bytes=" << p.data.size() << "\n";
    return 0;
}

int cmdXml(const QString &path)
{
    omr::OmeTiffReader reader;
    QString error;
    if (!reader.open(path, &error)) {
        err << "Failed to open: " << error << "\n";
        return 1;
    }
    const QString xml = reader.sourceOmeXml();
    if (xml.isEmpty()) {
        err << "No embedded OME-XML in " << path << "\n";
        return 1;
    }
    out << xml << "\n";
    return 0;
}

int cmdConvert(const QString &in, const QString &outPath, const QString &metaJson)
{
    OMETiffImage img;
    if (!img.open(in)) {
        err << "Failed to open: " << in << "\n";
        return 1;
    }

    ImageMetadata meta = img.extractMetadata();
    if (!metaJson.isEmpty()) {
        auto loaded = MetadataJson::loadFromFile(metaJson);
        if (!loaded) {
            err << "Failed to load metadata JSON: " << loaded.error() << "\n";
            return 1;
        }
        // Keep dimensions/name from the image, take editable fields from JSON.
        ImageMetadata merged = meta;
        merged.physSizeXNm = loaded->physSizeXNm;
        merged.physSizeYNm = loaded->physSizeYNm;
        merged.physSizeZNm = loaded->physSizeZNm;
        merged.numericalAperture = loaded->numericalAperture;
        merged.lensImmersion = loaded->lensImmersion;
        merged.embeddingMedium = loaded->embeddingMedium;
        merged.immersionRI = loaded->immersionRI;
        for (size_t i = 0; i < merged.channels.size() && i < loaded->channels.size(); ++i)
            merged.channels[i] = loaded->channels[i];
        meta = merged;
    }

    auto r = img.saveWithMetadata(outPath, meta, [](omr::dimension_size_type cur, omr::dimension_size_type total) {
        out << "\r  writing plane " << (cur + 1) << "/" << total;
        out.flush();
        return true;
    });
    out << "\n";
    if (!r) {
        err << "Save failed: " << r.error() << "\n";
        return 1;
    }
    out << "Wrote " << outPath << "\n";
    return 0;
}

int cmdChannels(const QString &path, int interleaved)
{
    OMETiffImage img;
    if (!img.open(path)) {
        err << "Failed to open: " << path << "\n";
        return 1;
    }
    auto r = img.setInterleavedChannelCount(static_cast<omr::dimension_size_type>(interleaved));
    if (!r) {
        err << "Failed: " << r.error() << "\n";
        return 1;
    }
    out << "Set interleaved channels to " << interleaved << "\n";
    out << "  effective: Z=" << img.sizeZ() << " C=" << img.sizeC() << " T=" << img.sizeT() << "\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 3)
        return usage();

    const QString cmd = args[1];
    if (cmd == QLatin1String("info"))
        return cmdInfo(args[2]);
    if (cmd == QLatin1String("xml"))
        return cmdXml(args[2]);
    if (cmd == QLatin1String("convert")) {
        if (args.size() < 4)
            return usage();
        QString metaJson;
        const int metaIdx = args.indexOf(QStringLiteral("--meta"));
        if (metaIdx >= 0 && metaIdx + 1 < args.size())
            metaJson = args[metaIdx + 1];
        return cmdConvert(args[2], args[3], metaJson);
    }
    if (cmd == QLatin1String("channels")) {
        if (args.size() < 4)
            return usage();
        return cmdChannels(args[2], args[3].toInt());
    }
    return usage();
}
