/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include "ometiffreader.h"

namespace Ui {
class MicroscopeParamsWidget;
}

class MicroscopeParamsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MicroscopeParamsWidget(QWidget *parent = nullptr);
    ~MicroscopeParamsWidget();

    /**
     * @brief Load metadata into the widget for display/editing
     * @param metadata The image metadata to display
     */
    void setMetadata(const ImageMetadata &metadata);

    /**
     * @brief Get the current metadata values from the widget
     * @return ImageMetadata with current values from UI
     */
    ImageMetadata getMetadata() const;

    /**
     * @brief Clear all metadata fields
     */
    void clearMetadata();

    /**
     * @brief Check if metadata has been modified
     * @return true if user has made changes
     */
    bool isModified() const;

    /**
     * @brief Reset modified state
     */
    void resetModified();

signals:
    /**
     * @brief Emitted when user modifies any metadata field
     */
    void metadataModified();

private slots:
    void onChannelSelectionChanged(int row);
    void onMetadataFieldChanged();
    void updateChannelInList();

private:
    void updateChannelUI(int channelIndex);
    void saveCurrentChannelData();
    QString formatDataSize(size_t bytes) const;

    Ui::MicroscopeParamsWidget *ui;
    ImageMetadata m_metadata;
    int m_currentChannel = -1;
    bool m_modified = false;
    bool m_updatingUI = false;  // Prevent recursive updates
};
