/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QMainWindow>
#include <QThread>
#include <QProgressDialog>
#include <memory>

class OMETiffImage;
class SavedParamsManager;
struct ImageMetadata;

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void openFile();
    void saveFile();
    void saveFileAs();
    void quickSaveFile();
    void updateImage();
    void onSliderZChanged(int value);
    void onSliderTChanged(int value);
    void onSliderCChanged(int value);
    void onMetadataModified();
    void onInterleavedChannelsChanged(int count);

    void onSaveParamsClicked();
    void onLoadParamsClicked();
    void onQuickLoadParamsClicked();
    void onRemoveParamsFromListClicked();

private:
    void resetSliderValues();
    void updateSliderRanges();
    void setNavigationEnabled(bool enabled);
    void updateContrastSliderRange(const ImageMetadata &metadata);
    void saveCurrentFile(bool quicksave);
    bool performSaveWithProgress(const QString &filename, const ImageMetadata &metadata);
    void updateSavedParamsList();
    void loadParametersFromFile(const QString &filePath);

    void saveWindowState();
    void restoreWindowState();

    Ui::MainWindow *ui;
    std::unique_ptr<OMETiffImage> m_tiffImage;
    std::unique_ptr<SavedParamsManager> m_savedParamsManager;

    // Current position in the image stack
    int m_currentZ = 0;
    int m_currentT = 0;
    int m_currentC = 0;
};
