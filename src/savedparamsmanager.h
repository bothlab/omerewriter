/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>

/**
 * @brief Manages a list of saved Microscope parameter files.
 */
class SavedParamsManager : public QObject
{
    Q_OBJECT

public:
    explicit SavedParamsManager(QObject *parent = nullptr);
    ~SavedParamsManager() override;

    /**
     * @brief Add a parameter file to the saved list
     * @param filePath Absolute path to the JSON parameter file
     * @return true if added, false if already in list
     */
    bool addFile(const QString &filePath);

    /**
     * @brief Remove a file from the saved list
     * @param filePath Path to remove
     * @return true if removed, false if not found
     */
    bool removeFile(const QString &filePath);

    /**
     * @brief Get all saved parameter file paths
     * @return List of existing file paths
     */
    [[nodiscard]] QStringList getFiles() const;

    /**
     * @brief Get user-friendly display names for saved files
     * @return List of display names (filename + partial path)
     */
    [[nodiscard]] QStringList getDisplayNames() const;

    /**
     * @brief Clear all saved parameter files from the list
     */
    void clear();

signals:
    /**
     * @brief Emitted when the list of saved files changes
     */
    void filesChanged();

private:
    QStringList validateFiles();
    void loadFromSettings();
    void saveToSettings();

    QStringList m_files;
    QSettings m_settings;
};
