/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QMainWindow>
#include <memory>

class OMETiffReader;

QT_BEGIN_NAMESPACE
namespace Ui {
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
    void updateImage();
    void onSliderZChanged(int value);
    void onSliderTChanged(int value);
    void onSliderCChanged(int value);

private:
    void setupConnections();
    void updateSliderRanges();
    void setNavigationEnabled(bool enabled);

    Ui::MainWindow *ui;
    std::unique_ptr<OMETiffReader> m_reader;

    // Current position in the image stack
    int m_currentZ = 0;
    int m_currentT = 0;
    int m_currentC = 0;
};
