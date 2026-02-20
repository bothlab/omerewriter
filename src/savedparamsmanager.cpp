/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "savedparamsmanager.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>

SavedParamsManager::SavedParamsManager(QObject *parent)
    : QObject(parent),
      m_settings("OMERewriter", "OMERewriter")
{
    loadFromSettings();
}

SavedParamsManager::~SavedParamsManager() = default;

bool SavedParamsManager::addFile(const QString &filePath)
{
    const auto absolutePath = QFileInfo(filePath).absoluteFilePath();
    if (m_files.contains(absolutePath)) {
        qDebug() << "File already in saved params list:" << absolutePath;
        return false;
    }

    if (!QFileInfo::exists(absolutePath)) {
        qWarning() << "Cannot add non-existent file to saved params:" << absolutePath;
        return false;
    }

    m_files.append(absolutePath);
    saveToSettings();
    emit filesChanged();

    qDebug() << "Added file to saved params list:" << absolutePath;
    return true;
}

bool SavedParamsManager::removeFile(const QString &filePath)
{
    const auto absolutePath = QFileInfo(filePath).absoluteFilePath();

    const auto removed = m_files.removeAll(absolutePath);
    if (removed <= 0)
        return false;

    saveToSettings();
    emit filesChanged();
    qDebug() << "Removed file from saved params list:" << absolutePath;
    return true;
}

QStringList SavedParamsManager::getFiles() const
{
    return m_files;
}

QStringList SavedParamsManager::getDisplayNames() const
{
    QStringList names;

    for (const QString &path : m_files) {
        QFileInfo info(path);
        QString displayName = info.fileName();

        // Add partial path info for disambiguation
        QString dirName = info.dir().dirName();
        if (!dirName.isEmpty() && dirName != ".")
            displayName = dirName + "/" + displayName;

        names.append(displayName);
    }

    return names;
}

QStringList SavedParamsManager::validateFiles()
{
    QStringList removedFiles;

    for (auto i = m_files.size() - 1; i >= 0; --i) {
        if (QFileInfo::exists(m_files[i]))
            continue;

        qDebug() << "Removing non-existent file from saved params:" << m_files[i];
        removedFiles.append(m_files[i]);
        m_files.removeAt(i);
    }

    if (!removedFiles.isEmpty()) {
        saveToSettings();
        emit filesChanged();
    }

    return removedFiles;
}

void SavedParamsManager::clear()
{
    if (m_files.isEmpty())
        return;

    m_files.clear();
    saveToSettings();
    emit filesChanged();
    qDebug() << "Cleared all saved parameter files";
}

void SavedParamsManager::loadFromSettings()
{
    m_files = m_settings.value("parameters/files", QStringList()).toStringList();
    qDebug() << "Loaded" << m_files.size() << "saved parameter file(s) from settings";

    // immediately drop any file that has been removed and save the result
    validateFiles();
}

void SavedParamsManager::saveToSettings()
{
    // don't save invalid paths
    validateFiles();

    m_settings.setValue("parameters/files", m_files);
    m_settings.sync();
}
