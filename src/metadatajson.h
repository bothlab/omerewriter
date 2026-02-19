/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QJsonObject>
#include <expected>
#include "ometiffimage.h"

/**
 * @brief Utility functions for serializing/deserializing ImageMetadata to/from JSON
 */
namespace MetadataJson
{

/**
 * @brief Convert ImageMetadata to a QJsonObject
 */
QJsonObject toJson(const ImageMetadata &metadata);

/**
 * @brief Convert a QJsonObject to ImageMetadata
 * @return ImageMetadata on success, error message on failure
 */
std::expected<ImageMetadata, QString> fromJson(const QJsonObject &json);

/**
 * @brief Save ImageMetadata to a JSON file
 * @param metadata The metadata to save
 * @param filename Path to the output JSON file
 * @return true on success, error message on failure
 */
std::expected<bool, QString> saveToFile(const ImageMetadata &metadata, const QString &filename);

/**
 * @brief Load ImageMetadata from a JSON file
 * @param filename Path to the JSON file
 * @return ImageMetadata on success, error message on failure
 */
std::expected<ImageMetadata, QString> loadFromFile(const QString &filename);

} // namespace MetadataJson
