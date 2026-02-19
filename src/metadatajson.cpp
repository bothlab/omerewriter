/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "metadatajson.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace MetadataJson
{

QJsonObject toJson(const ImageMetadata &metadata)
{
    QJsonObject root;

    // Image info (dimensions, pixel type, etc.) are intentionally omitted,
    // as those come directly from the image and will never be loaded back.

    // Physical sizes
    root["physSizeXNm"] = metadata.physSizeXNm;
    root["physSizeYNm"] = metadata.physSizeYNm;
    root["physSizeZNm"] = metadata.physSizeZNm;

    // Optical parameters
    root["numericalAperture"] = metadata.numericalAperture;
    root["lensImmersion"] = QString::fromStdString(std::string(metadata.lensImmersion));
    root["embeddingMedium"] = QString::fromStdString(std::string(metadata.embeddingMedium));
    root["immersionRI"] = metadata.immersionRI;

    // Channel parameters
    QJsonArray channelsArray;
    for (const auto &ch : metadata.channels) {
        QJsonObject chObj;
        chObj["name"] = ch.name;
        chObj["acquisitionMode"] = QString::fromStdString(std::string(ch.acquisitionMode));
        chObj["exWavelengthNm"] = ch.exWavelengthNm;
        chObj["emWavelengthNm"] = ch.emWavelengthNm;
        chObj["pinholeSizeNm"] = ch.pinholeSizeNm;
        chObj["photonCount"] = ch.photonCount;
        channelsArray.append(chObj);
    }
    root["channels"] = channelsArray;

    return root;
}

std::expected<ImageMetadata, QString> fromJson(const QJsonObject &json)
{
    ImageMetadata metadata;

    // Physical sizes
    if (json.contains("physSizeXNm"))
        metadata.physSizeXNm = json["physSizeXNm"].toDouble();
    if (json.contains("physSizeYNm"))
        metadata.physSizeYNm = json["physSizeYNm"].toDouble();
    if (json.contains("physSizeZNm"))
        metadata.physSizeZNm = json["physSizeZNm"].toDouble();

    // Optical parameters
    if (json.contains("numericalAperture"))
        metadata.numericalAperture = json["numericalAperture"].toDouble();

    if (json.contains("lensImmersion")) {
        const auto immersionStr = json["lensImmersion"].toString();
        metadata.lensImmersion = ome::xml::model::enums::Immersion(immersionStr.toStdString());
    }

    if (json.contains("embeddingMedium")) {
        const auto mediumStr = json["embeddingMedium"].toString();
        metadata.embeddingMedium = ome::xml::model::enums::Medium(mediumStr.toStdString());
    }

    if (json.contains("immersionRI"))
        metadata.immersionRI = json["immersionRI"].toDouble();

    // Channel parameters
    if (json.contains("channels") && json["channels"].isArray()) {
        const auto channelsArray = json["channels"].toArray();
        for (const auto &chVal : channelsArray) {
            if (!chVal.isObject())
                continue;

            QJsonObject chObj = chVal.toObject();
            ChannelParams ch;

            if (chObj.contains("name"))
                ch.name = chObj["name"].toString();

            if (chObj.contains("acquisitionMode")) {
                QString modeStr = chObj["acquisitionMode"].toString();
                ch.acquisitionMode = ome::xml::model::enums::AcquisitionMode(modeStr.toStdString());
            }

            if (chObj.contains("exWavelengthNm"))
                ch.exWavelengthNm = chObj["exWavelengthNm"].toDouble();
            if (chObj.contains("emWavelengthNm"))
                ch.emWavelengthNm = chObj["emWavelengthNm"].toDouble();
            if (chObj.contains("pinholeSizeNm"))
                ch.pinholeSizeNm = chObj["pinholeSizeNm"].toDouble();
            if (chObj.contains("photonCount"))
                ch.photonCount = chObj["photonCount"].toInt();

            metadata.channels.push_back(ch);
        }
    }

    return metadata;
}

std::expected<bool, QString> saveToFile(const ImageMetadata &metadata, const QString &filename)
{
    const auto json = toJson(metadata);
    QJsonDocument doc(json);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return std::unexpected(QStringLiteral("Failed to open file for writing: %1").arg(filename));

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Saved metadata to JSON file:" << filename;
    return true;
}

std::expected<ImageMetadata, QString> loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::unexpected(QStringLiteral("Failed to open file for reading: %1").arg(filename));

    const auto data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return std::unexpected(
            QStringLiteral("JSON parse error: %1 at offset %2").arg(parseError.errorString()).arg(parseError.offset));
    }

    if (!doc.isObject())
        return std::unexpected("JSON document is not an object");

    auto result = fromJson(doc.object());
    if (result)
        qDebug() << "Loaded metadata from JSON file:" << filename;

    return result;
}

} // namespace MetadataJson
