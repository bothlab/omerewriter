/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "omexml.h"

#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include <QUuid>

namespace omr::omexml
{

namespace
{

const QString kOmeNs = QStringLiteral("http://www.openmicroscopy.org/Schemas/OME/2016-06");
const QString kXsiNs = QStringLiteral("http://www.w3.org/2001/XMLSchema-instance");

// --- small QDom helpers ----------------------------------------------------

QDomElement firstChildByTag(const QDomElement &parent, const QString &tag)
{
    for (QDomNode n = parent.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement() && n.toElement().tagName() == tag)
            return n.toElement();
    }
    return {};
}

std::vector<QDomElement> childrenByTag(const QDomElement &parent, const QString &tag)
{
    std::vector<QDomElement> out;
    for (QDomNode n = parent.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement() && n.toElement().tagName() == tag)
            out.push_back(n.toElement());
    }
    return out;
}

void removeChildrenByTag(QDomElement &parent, const QString &tag)
{
    for (const auto &e : childrenByTag(parent, tag)) {
        QDomNode node = e;
        parent.removeChild(node);
    }
}

QString numStr(double v)
{
    return QString::number(v, 'g', 12);
}

QString makeUuidUrn()
{
    return QStringLiteral("urn:uuid:") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// Read a length attribute together with its unit and return nanometres.
double lengthAttrNm(const QDomElement &e, const QString &attr, const QString &unitAttr, bool defaultMicrometre)
{
    if (!e.hasAttribute(attr))
        return 0.0;
    const double v = e.attribute(attr).toDouble();
    const QString unit = e.attribute(unitAttr);
    return lengthToNm(v, unit, defaultMicrometre);
}

} // namespace

int linearPlaneIndex(DimensionOrder order, int z, int c, int t, int sizeZ, int sizeC, int sizeT)
{
    switch (order) {
    case DimensionOrder::XYZCT: return z + sizeZ * (c + sizeC * t);
    case DimensionOrder::XYZTC: return z + sizeZ * (t + sizeT * c);
    case DimensionOrder::XYCZT: return c + sizeC * (z + sizeZ * t);
    case DimensionOrder::XYCTZ: return c + sizeC * (t + sizeT * z);
    case DimensionOrder::XYTCZ: return t + sizeT * (c + sizeC * z);
    case DimensionOrder::XYTZC: return t + sizeT * (z + sizeZ * c);
    }
    return z + sizeZ * (c + sizeC * t);
}

std::vector<PlaneCoord> planeOrderXYZCT(int sizeZ, int sizeC, int sizeT)
{
    std::vector<PlaneCoord> order;
    order.reserve(static_cast<size_t>(sizeZ) * sizeC * sizeT);
    for (int t = 0; t < sizeT; ++t)
        for (int c = 0; c < sizeC; ++c)
            for (int z = 0; z < sizeZ; ++z)
                order.push_back({z, c, t});
    return order;
}

OmeXmlInfo parse(const QString &xml)
{
    OmeXmlInfo info;
    if (xml.isEmpty())
        return info;

    QDomDocument doc;
    if (!doc.setContent(xml))
        return info;

    const QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("OME"))
        return info;

    const QDomElement image = firstChildByTag(root, QStringLiteral("Image"));
    if (image.isNull())
        return info;
    QDomElement pixels = firstChildByTag(image, QStringLiteral("Pixels"));
    if (pixels.isNull())
        return info;

    ImageMetadata &meta = info.metadata;

    // Image name
    meta.imageName = image.attribute(QStringLiteral("Name"));

    // Pixels dimensions / type
    info.sizeX = pixels.attribute(QStringLiteral("SizeX"), QStringLiteral("0")).toInt();
    info.sizeY = pixels.attribute(QStringLiteral("SizeY"), QStringLiteral("0")).toInt();
    info.sizeZ = pixels.attribute(QStringLiteral("SizeZ"), QStringLiteral("1")).toInt();
    info.sizeC = pixels.attribute(QStringLiteral("SizeC"), QStringLiteral("1")).toInt();
    info.sizeT = pixels.attribute(QStringLiteral("SizeT"), QStringLiteral("1")).toInt();
    info.pixelType = pixelTypeFromString(pixels.attribute(QStringLiteral("Type")));
    info.dimensionOrder =
        dimensionOrderFromString(pixels.attribute(QStringLiteral("DimensionOrder"), QStringLiteral("XYZCT")));
    info.significantBits = pixels.attribute(QStringLiteral("SignificantBits"), QStringLiteral("0")).toInt();
    info.bigEndian = pixels.attribute(QStringLiteral("BigEndian")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    info.interleaved = pixels.attribute(QStringLiteral("Interleaved")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;

    meta.sizeX = info.sizeX;
    meta.sizeY = info.sizeY;
    meta.sizeZ = info.sizeZ;
    meta.sizeC = info.sizeC;
    meta.sizeT = info.sizeT;
    meta.pixelType = pixelTypeToString(info.pixelType);

    // Physical sizes (-> nm)
    meta.physSizeXNm = lengthAttrNm(pixels, QStringLiteral("PhysicalSizeX"), QStringLiteral("PhysicalSizeXUnit"), true);
    meta.physSizeYNm = lengthAttrNm(pixels, QStringLiteral("PhysicalSizeY"), QStringLiteral("PhysicalSizeYUnit"), true);
    meta.physSizeZNm = lengthAttrNm(pixels, QStringLiteral("PhysicalSizeZ"), QStringLiteral("PhysicalSizeZUnit"), true);

    // Objective settings
    const QDomElement objSettings = firstChildByTag(image, QStringLiteral("ObjectiveSettings"));
    QString objectiveId;
    if (!objSettings.isNull()) {
        objectiveId = objSettings.attribute(QStringLiteral("ID"));
        if (objSettings.hasAttribute(QStringLiteral("RefractiveIndex")))
            meta.immersionRI = objSettings.attribute(QStringLiteral("RefractiveIndex")).toDouble();
        if (objSettings.hasAttribute(QStringLiteral("Medium")))
            meta.embeddingMedium = mediumFromString(objSettings.attribute(QStringLiteral("Medium")));
    }

    // Objective (matched by ID under any Instrument)
    const QDomNodeList objectives = root.elementsByTagName(QStringLiteral("Objective"));
    for (int i = 0; i < objectives.size(); ++i) {
        const QDomElement obj = objectives.at(i).toElement();
        if (!objectiveId.isEmpty() && obj.attribute(QStringLiteral("ID")) != objectiveId)
            continue;
        if (obj.hasAttribute(QStringLiteral("LensNA")))
            meta.numericalAperture = obj.attribute(QStringLiteral("LensNA")).toDouble();
        if (obj.hasAttribute(QStringLiteral("Immersion")))
            meta.lensImmersion = immersionFromString(obj.attribute(QStringLiteral("Immersion")));
        break;
    }

    // Channels
    const auto channelElems = childrenByTag(pixels, QStringLiteral("Channel"));
    if (!channelElems.empty())
        info.samplesPerPixel =
            static_cast<unsigned int>(channelElems.front().attribute(QStringLiteral("SamplesPerPixel"), QStringLiteral("1")).toInt());
    for (const auto &ch : channelElems) {
        ChannelParams cp;
        cp.name = ch.attribute(QStringLiteral("Name"));
        if (cp.name.isEmpty())
            cp.name = QStringLiteral("Channel %1").arg(meta.channels.size());
        cp.acquisitionMode = acquisitionModeFromString(ch.attribute(QStringLiteral("AcquisitionMode")));
        if (cp.acquisitionMode == AcquisitionMode::MultiPhotonMicroscopy)
            cp.photonCount = 2;
        cp.exWavelengthNm =
            lengthAttrNm(ch, QStringLiteral("ExcitationWavelength"), QStringLiteral("ExcitationWavelengthUnit"), false);
        cp.emWavelengthNm =
            lengthAttrNm(ch, QStringLiteral("EmissionWavelength"), QStringLiteral("EmissionWavelengthUnit"), false);
        cp.pinholeSizeNm = lengthAttrNm(ch, QStringLiteral("PinholeSize"), QStringLiteral("PinholeSizeUnit"), false);
        meta.channels.push_back(cp);
    }

    // TiffData plane map
    for (const auto &td : childrenByTag(pixels, QStringLiteral("TiffData"))) {
        TiffDataEntry entry;
        entry.firstZ = td.attribute(QStringLiteral("FirstZ"), QStringLiteral("0")).toInt();
        entry.firstC = td.attribute(QStringLiteral("FirstC"), QStringLiteral("0")).toInt();
        entry.firstT = td.attribute(QStringLiteral("FirstT"), QStringLiteral("0")).toInt();
        entry.ifd = td.attribute(QStringLiteral("IFD"), QStringLiteral("0")).toInt();
        entry.planeCount = td.attribute(QStringLiteral("PlaneCount"), QStringLiteral("0")).toInt();
        const QDomElement uuid = firstChildByTag(td, QStringLiteral("UUID"));
        if (!uuid.isNull())
            entry.fileName = uuid.attribute(QStringLiteral("FileName"));
        info.tiffData.push_back(entry);
    }

    info.valid = true;
    return info;
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

namespace
{

QDomElement addChild(QDomDocument &doc, QDomElement &parent, const QString &tag)
{
    QDomElement e = doc.createElement(tag);
    parent.appendChild(e);
    return e;
}

void appendTiffData(
    QDomDocument &doc,
    QDomElement &pixels,
    const std::vector<PlaneCoord> &planeOrder,
    const QString &fileName,
    const QString &uuidUrn)
{
    for (size_t i = 0; i < planeOrder.size(); ++i) {
        const PlaneCoord &p = planeOrder[i];
        QDomElement td = addChild(doc, pixels, QStringLiteral("TiffData"));
        td.setAttribute(QStringLiteral("FirstC"), p.c);
        td.setAttribute(QStringLiteral("FirstT"), p.t);
        td.setAttribute(QStringLiteral("FirstZ"), p.z);
        td.setAttribute(QStringLiteral("IFD"), static_cast<int>(i));
        td.setAttribute(QStringLiteral("PlaneCount"), 1);
        QDomElement uuid = addChild(doc, td, QStringLiteral("UUID"));
        uuid.setAttribute(QStringLiteral("FileName"), fileName);
        uuid.appendChild(doc.createTextNode(uuidUrn));
    }
}

void setChannelOptics(QDomElement &ch, const ChannelParams &cp)
{
    ch.setAttribute(QStringLiteral("Name"), cp.name);
    ch.setAttribute(QStringLiteral("AcquisitionMode"), acquisitionModeToString(cp.acquisitionMode));
    if (cp.exWavelengthNm > 0) {
        ch.setAttribute(QStringLiteral("ExcitationWavelength"), numStr(cp.exWavelengthNm));
        ch.setAttribute(QStringLiteral("ExcitationWavelengthUnit"), QStringLiteral("nm"));
    } else {
        ch.removeAttribute(QStringLiteral("ExcitationWavelength"));
        ch.removeAttribute(QStringLiteral("ExcitationWavelengthUnit"));
    }
    if (cp.emWavelengthNm > 0) {
        ch.setAttribute(QStringLiteral("EmissionWavelength"), numStr(cp.emWavelengthNm));
        ch.setAttribute(QStringLiteral("EmissionWavelengthUnit"), QStringLiteral("nm"));
    } else {
        ch.removeAttribute(QStringLiteral("EmissionWavelength"));
        ch.removeAttribute(QStringLiteral("EmissionWavelengthUnit"));
    }
    if (cp.pinholeSizeNm > 0) {
        ch.setAttribute(QStringLiteral("PinholeSize"), numStr(cp.pinholeSizeNm));
        ch.setAttribute(QStringLiteral("PinholeSizeUnit"), QStringLiteral("nm"));
    } else {
        ch.removeAttribute(QStringLiteral("PinholeSize"));
        ch.removeAttribute(QStringLiteral("PinholeSizeUnit"));
    }
}

void setPhysicalSizes(QDomElement &pixels, const ImageMetadata &meta)
{
    const struct {
        const char *attr;
        const char *unit;
        double nm;
    } sizes[] = {
        {"PhysicalSizeX", "PhysicalSizeXUnit", meta.physSizeXNm},
        {"PhysicalSizeY", "PhysicalSizeYUnit", meta.physSizeYNm},
        {"PhysicalSizeZ", "PhysicalSizeZUnit", meta.physSizeZNm},
    };
    for (const auto &s : sizes) {
        if (s.nm > 0) {
            pixels.setAttribute(QString::fromLatin1(s.attr), numStr(s.nm / 1000.0));
            pixels.setAttribute(QString::fromLatin1(s.unit), QString::fromUtf8("\xC2\xB5m")); // µm
        } else {
            pixels.removeAttribute(QString::fromLatin1(s.attr));
            pixels.removeAttribute(QString::fromLatin1(s.unit));
        }
    }
}

/**
 * Ensure the objective metadata (NA, immersion, medium, refractive index) the UI
 * exposes is written. Updates an existing ObjectiveSettings/Objective pair, and
 * creates the Instrument/Objective/InstrumentRef/ObjectiveSettings chain when the
 * source lacks it but the user supplied a numerical aperture.
 *
 * @p pixels must be a child of @p image (used as the insertion anchor so the new
 * elements land in their schema-correct position, just before <Pixels>).
 */
void applyObjective(QDomDocument &doc, QDomElement &ome, QDomElement &image, QDomElement &pixels, const ImageMetadata &meta)
{
    const bool hasOptics = meta.numericalAperture > 0;

    QDomElement objSettings = firstChildByTag(image, QStringLiteral("ObjectiveSettings"));
    if (objSettings.isNull() && !hasOptics)
        return; // No objective info in the source and none supplied: nothing to do.

    if (objSettings.isNull()) {
        objSettings = doc.createElement(QStringLiteral("ObjectiveSettings"));
        image.insertBefore(objSettings, pixels);
    }

    objSettings.setAttribute(QStringLiteral("Medium"), mediumToString(meta.embeddingMedium));
    if (meta.immersionRI > 0)
        objSettings.setAttribute(QStringLiteral("RefractiveIndex"), numStr(meta.immersionRI));

    QString objectiveId = objSettings.attribute(QStringLiteral("ID"));

    // Locate the Objective the settings point at (or the first one, if ID is empty).
    QDomElement objective;
    const QDomNodeList objectives = ome.elementsByTagName(QStringLiteral("Objective"));
    for (int i = 0; i < objectives.size(); ++i) {
        QDomElement o = objectives.at(i).toElement();
        if (!objectiveId.isEmpty() && o.attribute(QStringLiteral("ID")) != objectiveId)
            continue;
        objective = o;
        break;
    }

    // Create the Instrument/Objective chain if it is missing and we have a value to store.
    if (objective.isNull() && hasOptics) {
        if (objectiveId.isEmpty()) {
            objectiveId = QStringLiteral("Objective:0:0");
            objSettings.setAttribute(QStringLiteral("ID"), objectiveId);
        }

        QDomElement instrument = firstChildByTag(ome, QStringLiteral("Instrument"));
        if (instrument.isNull()) {
            instrument = doc.createElement(QStringLiteral("Instrument"));
            instrument.setAttribute(QStringLiteral("ID"), QStringLiteral("Instrument:0"));
            ome.insertBefore(instrument, image); // Instrument precedes Image in the schema.
        }
        objective = doc.createElement(QStringLiteral("Objective"));
        objective.setAttribute(QStringLiteral("ID"), objectiveId);
        instrument.appendChild(objective);

        if (firstChildByTag(image, QStringLiteral("InstrumentRef")).isNull()) {
            QDomElement iref = doc.createElement(QStringLiteral("InstrumentRef"));
            iref.setAttribute(QStringLiteral("ID"), instrument.attribute(QStringLiteral("ID")));
            image.insertBefore(iref, objSettings);
        }
    }

    if (!objective.isNull()) {
        if (meta.numericalAperture > 0)
            objective.setAttribute(QStringLiteral("LensNA"), numStr(meta.numericalAperture));
        objective.setAttribute(QStringLiteral("Immersion"), immersionToString(meta.lensImmersion));
    }
}

QString serialize(QDomDocument &doc)
{
    QString out = doc.toString(2);
    // Guarantee exactly one XML declaration. QDom only emits one when the parsed
    // source already had it, so add it ourselves otherwise. This is precisely the
    // declaration that ome-files used to omit.
    if (!out.trimmed().startsWith(QLatin1String("<?xml")))
        out.prepend(QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"));
    return out;
}

} // namespace

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
    const QString &fileName)
{
    QDomDocument doc;
    const QString uuidUrn = makeUuidUrn();

    QDomElement ome = doc.createElement(QStringLiteral("OME"));
    // Declare namespaces as literal attributes so QDom keeps the output clean
    // (single declaration on the root, children inherit the default namespace).
    ome.setAttribute(QStringLiteral("xmlns"), kOmeNs);
    ome.setAttribute(QStringLiteral("xmlns:xsi"), kXsiNs);
    ome.setAttribute(
        QStringLiteral("xsi:schemaLocation"),
        kOmeNs + QLatin1Char(' ') + kOmeNs + QStringLiteral("/ome.xsd"));
    ome.setAttribute(QStringLiteral("UUID"), uuidUrn);
    doc.appendChild(ome);

    QDomElement image = addChild(doc, ome, QStringLiteral("Image"));
    image.setAttribute(QStringLiteral("ID"), QStringLiteral("Image:0"));
    image.setAttribute(QStringLiteral("Name"), meta.imageName.isEmpty() ? QFileInfo(fileName).fileName() : meta.imageName);

    QDomElement pixels = addChild(doc, image, QStringLiteral("Pixels"));
    pixels.setAttribute(QStringLiteral("BigEndian"), QStringLiteral("false"));
    pixels.setAttribute(QStringLiteral("DimensionOrder"), QStringLiteral("XYZCT"));
    pixels.setAttribute(QStringLiteral("ID"), QStringLiteral("Pixels:0"));
    pixels.setAttribute(QStringLiteral("Interleaved"), samplesPerPixel > 1 ? QStringLiteral("true") : QStringLiteral("false"));
    pixels.setAttribute(QStringLiteral("SignificantBits"), static_cast<int>(pixelTypeBits(pixelType)));
    pixels.setAttribute(QStringLiteral("SizeC"), sizeC);
    pixels.setAttribute(QStringLiteral("SizeT"), sizeT);
    pixels.setAttribute(QStringLiteral("SizeX"), sizeX);
    pixels.setAttribute(QStringLiteral("SizeY"), sizeY);
    pixels.setAttribute(QStringLiteral("SizeZ"), sizeZ);
    pixels.setAttribute(QStringLiteral("Type"), pixelTypeToString(pixelType));
    setPhysicalSizes(pixels, meta);

    applyObjective(doc, ome, image, pixels, meta);

    for (int c = 0; c < sizeC; ++c) {
        QDomElement ch = addChild(doc, pixels, QStringLiteral("Channel"));
        ch.setAttribute(QStringLiteral("ID"), QStringLiteral("Channel:0:%1").arg(c));
        ChannelParams cp;
        if (c < static_cast<int>(meta.channels.size()))
            cp = meta.channels[c];
        else
            cp.name = QStringLiteral("Channel %1").arg(c);
        setChannelOptics(ch, cp);
        ch.setAttribute(QStringLiteral("SamplesPerPixel"), static_cast<int>(samplesPerPixel));
    }

    appendTiffData(doc, pixels, planeOrder, fileName, uuidUrn);

    return serialize(doc);
}

QString patch(
    const QString &srcXml,
    const ImageMetadata &meta,
    const std::vector<PlaneCoord> &planeOrder,
    const QString &fileName)
{
    QDomDocument doc;
    if (!doc.setContent(srcXml))
        return {};

    QDomElement ome = doc.documentElement();
    if (ome.tagName() != QLatin1String("OME"))
        return {};

    const QString uuidUrn = makeUuidUrn();
    ome.setAttribute(QStringLiteral("UUID"), uuidUrn);

    // Drop any whole-file binary-only marker.
    removeChildrenByTag(ome, QStringLiteral("BinaryOnly"));

    QDomElement image = firstChildByTag(ome, QStringLiteral("Image"));
    if (image.isNull())
        return {};
    QDomElement pixels = firstChildByTag(image, QStringLiteral("Pixels"));
    if (pixels.isNull())
        return {};

    // Remove generated / binary plane data; we re-create TiffData below.
    removeChildrenByTag(pixels, QStringLiteral("TiffData"));
    removeChildrenByTag(pixels, QStringLiteral("BinData"));
    removeChildrenByTag(pixels, QStringLiteral("MetadataOnly"));

    if (!meta.imageName.isEmpty())
        image.setAttribute(QStringLiteral("Name"), meta.imageName);

    setPhysicalSizes(pixels, meta);
    applyObjective(doc, ome, image, pixels, meta);

    // Channels (matched by document order).
    const auto channelElems = childrenByTag(pixels, QStringLiteral("Channel"));
    for (size_t i = 0; i < channelElems.size() && i < meta.channels.size(); ++i) {
        QDomElement ch = channelElems[i];
        setChannelOptics(ch, meta.channels[i]);
    }

    appendTiffData(doc, pixels, planeOrder, fileName, uuidUrn);

    return serialize(doc);
}

} // namespace omr::omexml
