/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ometiffimage.h"
#include "microscopeparamswidget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_tiffImage(std::make_unique<OMETiffImage>(this))
{
    ui->setupUi(this);

    // Menu actions
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(ui->btnLoadTiff, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(ui->btnQuickSave, &QPushButton::clicked, this, &MainWindow::quickSaveFile);

    // Slider connections
    connect(ui->sliderZ, &QSlider::valueChanged, this, &MainWindow::onSliderZChanged);
    connect(ui->sliderT, &QSlider::valueChanged, this, &MainWindow::onSliderTChanged);
    connect(ui->sliderC, &QSlider::valueChanged, this, &MainWindow::onSliderCChanged);

    // Sync spinboxes with sliders
    connect(ui->sliderZ, &QSlider::valueChanged, ui->spinBoxZ, &QSpinBox::setValue);
    connect(ui->sliderT, &QSlider::valueChanged, ui->spinBoxT, &QSpinBox::setValue);
    connect(ui->sliderC, &QSlider::valueChanged, ui->spinBoxC, &QSpinBox::setValue);

    connect(ui->spinBoxZ, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderZ, &QSlider::setValue);
    connect(ui->spinBoxT, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderT, &QSlider::setValue);
    connect(ui->spinBoxC, QOverload<int>::of(&QSpinBox::valueChanged), ui->sliderC, &QSlider::setValue);

    // Metadata modification tracking
    connect(ui->imageMetaWidget, &MicroscopeParamsWidget::metadataModified, this, &MainWindow::onMetadataModified);

    // TIFF interpretation controls
    connect(
        ui->spinCInterleaveCount,
        QOverload<int>::of(&QSpinBox::valueChanged),
        this,
        &MainWindow::onInterleavedChannelsChanged);

    // Default state
    setNavigationEnabled(false);
    ui->groupTiffInterpretation->setEnabled(false);

    // Set default range for interleave count (1 = no interleaving)
    ui->spinCInterleaveCount->setRange(1, 32);
    ui->spinCInterleaveCount->setValue(1);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open TIFF Image"),
        QString(),
        QStringLiteral(
            "All TIFF Files (*.ome.tiff *.ome.tif *.tiff *.tif);;OME-TIFF Files (*.ome.tiff *.ome.tif);;TIFF Files "
            "(*.tiff *.tif);;All Files (*)"));

    if (filename.isEmpty())
        return;

    if (!m_tiffImage->open(filename)) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Failed to open file:\n%1").arg(filename));
        return;
    }

    // Update window title
    QFileInfo fileInfo(filename);
    setWindowTitle(QStringLiteral("OMERewriter - %1").arg(fileInfo.fileName()));

    // Update slider ranges based on the opened file
    updateSliderRanges();

    // Reset position to origin
    m_currentZ = 0;
    m_currentT = 0;
    m_currentC = 0;

    // Block signals while resetting sliders
    ui->sliderZ->blockSignals(true);
    ui->sliderT->blockSignals(true);
    ui->sliderC->blockSignals(true);
    ui->spinBoxZ->blockSignals(true);
    ui->spinBoxT->blockSignals(true);
    ui->spinBoxC->blockSignals(true);

    ui->sliderZ->setValue(0);
    ui->sliderT->setValue(0);
    ui->sliderC->setValue(0);
    ui->spinBoxZ->setValue(0);
    ui->spinBoxT->setValue(0);
    ui->spinBoxC->setValue(0);

    ui->sliderZ->blockSignals(false);
    ui->sliderT->blockSignals(false);
    ui->sliderC->blockSignals(false);
    ui->spinBoxZ->blockSignals(false);
    ui->spinBoxT->blockSignals(false);
    ui->spinBoxC->blockSignals(false);

    // Enable navigation controls
    setNavigationEnabled(true);

    // Display the first image
    updateImage();

    // Load and display metadata in the params widget
    ImageMetadata metadata = m_tiffImage->extractMetadata(0);
    if (metadata.imageName.isEmpty()) {
        metadata.imageName = fileInfo.fileName();
    }
    ui->imageMetaWidget->setMetadata(metadata);

    // Update status bar
    statusBar()->showMessage(QStringLiteral("Loaded: %1 - Size: %2x%3, Z:%4 T:%5 C:%6")
                                 .arg(fileInfo.fileName())
                                 .arg(m_tiffImage->sizeX())
                                 .arg(m_tiffImage->sizeY())
                                 .arg(m_tiffImage->sizeZ())
                                 .arg(m_tiffImage->sizeT())
                                 .arg(m_tiffImage->sizeC()));

    ui->actionSave->setEnabled(true);
    ui->actionSaveAs->setEnabled(true);

    // we only allow rewriting / deinterleave if we *didn't* load an OME-TIFF
    ui->groupTiffInterpretation->setEnabled(!m_tiffImage->isOmeTiff());
}

void MainWindow::updateSliderRanges()
{
    if (!m_tiffImage->isOpen())
        return;

    auto sizeZ = m_tiffImage->sizeZ();
    auto sizeT = m_tiffImage->sizeT();
    auto sizeC = m_tiffImage->sizeC();

    // Set slider ranges (0-based indexing)
    ui->sliderZ->setRange(0, qMax(0, static_cast<int>(sizeZ) - 1));
    ui->sliderT->setRange(0, qMax(0, static_cast<int>(sizeT) - 1));
    ui->sliderC->setRange(0, qMax(0, static_cast<int>(sizeC) - 1));

    ui->spinBoxZ->setRange(0, qMax(0, static_cast<int>(sizeZ) - 1));
    ui->spinBoxT->setRange(0, qMax(0, static_cast<int>(sizeT) - 1));
    ui->spinBoxC->setRange(0, qMax(0, static_cast<int>(sizeC) - 1));

    // Show/hide controls based on dimension size
    bool hasZ = sizeZ > 1;
    bool hasT = sizeT > 1;
    bool hasC = sizeC > 1;

    ui->labelZ->setVisible(hasZ);
    ui->sliderZ->setVisible(hasZ);
    ui->spinBoxZ->setVisible(hasZ);

    ui->labelT->setVisible(hasT);
    ui->sliderT->setVisible(hasT);
    ui->spinBoxT->setVisible(hasT);

    ui->labelC->setVisible(hasC);
    ui->sliderC->setVisible(hasC);
    ui->spinBoxC->setVisible(hasC);

    // Hide the navigation group if there's nothing to navigate
    ui->navigationGroup->setVisible(hasZ || hasT || hasC);
}

void MainWindow::setNavigationEnabled(bool enabled)
{
    ui->sliderZ->setEnabled(enabled);
    ui->sliderT->setEnabled(enabled);
    ui->sliderC->setEnabled(enabled);
    ui->spinBoxZ->setEnabled(enabled);
    ui->spinBoxT->setEnabled(enabled);
    ui->spinBoxC->setEnabled(enabled);
}

void MainWindow::updateImage()
{
    if (!m_tiffImage->isOpen())
        return;

    RawImage image = m_tiffImage->readPlane(m_currentZ, m_currentC, m_currentT);

    if (image.isEmpty()) {
        qWarning() << "Failed to read plane at Z=" << m_currentZ << "T=" << m_currentT << "C=" << m_currentC;
        return;
    }

    ui->imageView->showImage(image);
}

void MainWindow::onSliderZChanged(int value)
{
    if (value == m_currentZ)
        return;

    m_currentZ = value;
    updateImage();
}

void MainWindow::onSliderTChanged(int value)
{
    if (value == m_currentT)
        return;

    m_currentT = value;
    updateImage();
}

void MainWindow::onSliderCChanged(int value)
{
    if (value == m_currentC)
        return;

    m_currentC = value;
    updateImage();
}

void MainWindow::saveCurrentFile(bool quicksave)
{
    if (!m_tiffImage->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Warning"), QStringLiteral("No file is currently open."));
        return;
    }

    // The quicksave action does not delete the source image if it wasn't an OME-TIFF,
    // but instead saves a new OME-TIFF alongside it.
    // The regular save action replaces the original file.
    // On quicksave, we also immediately load the saved file.

    QFileInfo fi(m_tiffImage->filename());
    const auto tiffBasename = fi.baseName();
    const auto tiffDir = fi.absoluteDir();

    const auto wasOmeTiff = m_tiffImage->isOmeTiff();
    const auto origFilename = m_tiffImage->filename();

    QString destFilename;
    QString tempFile;
    if (wasOmeTiff) {
        destFilename = m_tiffImage->filename();
        tempFile = tiffDir.absoluteFilePath(tiffBasename + ".tmp.ome.tiff");
    } else {
        // For raw TIFF, we save the modified OME-TIFF alongside the original, with a modified name
        destFilename = tiffDir.absoluteFilePath(tiffBasename + ".ome.tiff");
        tempFile = tiffDir.absoluteFilePath(tiffBasename + ".tmp.ome.tiff");

        if (QFile::exists(destFilename)) {
            auto result = QMessageBox::question(
                this,
                QStringLiteral("File Exists"),
                QStringLiteral("A file named '%1' already exists. Do you want to overwrite it?").arg(destFilename),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (result != QMessageBox::Yes)
                return;
        }
    }

    ImageMetadata metadata = ui->imageMetaWidget->getMetadata();
    auto r = m_tiffImage->saveWithMetadata(tempFile, metadata);
    if (r) {
        // Close the original file
        m_tiffImage->close();

        // Replace original with temp file
        QFile::remove(destFilename);
        if (QFile::rename(tempFile, destFilename)) {
            // Reopen the file
            if (m_tiffImage->open(destFilename)) {
                ui->imageMetaWidget->resetModified();
                statusBar()->showMessage(QStringLiteral("Saved: %1").arg(destFilename), 5000);
            } else {
                QMessageBox::critical(
                    this, QStringLiteral("Error"), QStringLiteral("Failed to reopen the saved file."));
            }
        } else {
            QMessageBox::critical(
                this, QStringLiteral("Error"), QStringLiteral("Failed to replace the original file."));
            // Try to recover by opening the temp file
            if (QFile::exists(tempFile))
                m_tiffImage->open(tempFile);
        }

        if (!wasOmeTiff && !quicksave) {
            // For regular save of raw TIFF, we delete the original file after saving the new OME-TIFF
            if (QFile::exists(origFilename)) {
                if (QFile::remove(origFilename)) {
                    statusBar()->showMessage(
                        QStringLiteral("Original file '%1' has been deleted.").arg(origFilename), 5000);
                } else {
                    QMessageBox::warning(
                        this,
                        QStringLiteral("Warning"),
                        QStringLiteral(
                            "Failed to delete original file '%1'. Please check if it can be deleted manually.")
                            .arg(origFilename));
                }
            }
        }
    } else {
        QMessageBox::critical(this, QStringLiteral("Failed to save TIFF file"), r.error());
        QFile::remove(tempFile);
    }
}

void MainWindow::saveFile()
{
    saveCurrentFile(false);
}

void MainWindow::quickSaveFile()
{
    saveCurrentFile(true);
}

void MainWindow::saveFileAs()
{
    if (!m_tiffImage->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Warning"), QStringLiteral("No file is currently open."));
        return;
    }

    QString filename = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save OME-TIFF As"),
        QString(),
        QStringLiteral("OME-TIFF Files (*.ome.tiff *.ome.tif);;All Files (*)"));

    if (filename.isEmpty())
        return;

    // Ensure proper extension
    if (!filename.endsWith(".ome.tiff", Qt::CaseInsensitive) && !filename.endsWith(".ome.tif", Qt::CaseInsensitive))
        filename += ".ome.tiff";

    ImageMetadata metadata = ui->imageMetaWidget->getMetadata();

    auto r = m_tiffImage->saveWithMetadata(filename, metadata);
    if (r) {
        ui->imageMetaWidget->resetModified();
        statusBar()->showMessage(QStringLiteral("Saved as: %1").arg(filename), 5000);

        // Ask if user wants to open the newly saved file
        auto result = QMessageBox::question(
            this,
            QStringLiteral("Open Saved File"),
            QStringLiteral("Do you want to open the newly saved file?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (result == QMessageBox::Yes) {
            m_tiffImage->close();
            if (m_tiffImage->open(filename)) {
                QFileInfo fileInfo(filename);
                setWindowTitle(QStringLiteral("OMERewriter - %1").arg(fileInfo.fileName()));
                updateSliderRanges();
                updateImage();
                ImageMetadata newMeta = m_tiffImage->extractMetadata(0);
                if (newMeta.imageName.isEmpty()) {
                    newMeta.imageName = fileInfo.fileName();
                }
                ui->imageMetaWidget->setMetadata(newMeta);
            }
        }
    } else {
        QMessageBox::critical(this, QStringLiteral("Failed to save TIFF file"), r.error());
    }
}

void MainWindow::onMetadataModified()
{
    // Update window title to indicate unsaved changes
    QString title = windowTitle();
    if (!title.endsWith(" *"))
        setWindowTitle(title + " *");
}

void MainWindow::onInterleavedChannelsChanged(int count)
{
    if (!m_tiffImage->isOpen() || m_tiffImage->isOmeTiff())
        return;

    // Apply the new interleaved channel count
    auto r = m_tiffImage->setInterleavedChannelCount(static_cast<OMETiffImage::dimension_size_type>(count));
    if (!r) {
        QMessageBox::warning(this, QStringLiteral("Invalid Interleaved Channel Count"), r.error());
        // Reset to previous valid value
        ui->spinCInterleaveCount->blockSignals(true);
        ui->spinCInterleaveCount->setValue(m_tiffImage->interleavedChannelCount());
        ui->spinCInterleaveCount->blockSignals(false);
        return;
    }

    // Reset current position
    m_currentZ = 0;
    m_currentT = 0;
    m_currentC = 0;

    // Update slider ranges based on new interpretation
    updateSliderRanges();

    // Reset sliders
    ui->sliderZ->blockSignals(true);
    ui->sliderT->blockSignals(true);
    ui->sliderC->blockSignals(true);
    ui->spinBoxZ->blockSignals(true);
    ui->spinBoxT->blockSignals(true);
    ui->spinBoxC->blockSignals(true);

    ui->sliderZ->setValue(0);
    ui->sliderT->setValue(0);
    ui->sliderC->setValue(0);
    ui->spinBoxZ->setValue(0);
    ui->spinBoxT->setValue(0);
    ui->spinBoxC->setValue(0);

    ui->sliderZ->blockSignals(false);
    ui->sliderT->blockSignals(false);
    ui->sliderC->blockSignals(false);
    ui->spinBoxZ->blockSignals(false);
    ui->spinBoxT->blockSignals(false);
    ui->spinBoxC->blockSignals(false);

    // Update metadata widget with new dimensions
    ImageMetadata metadata = m_tiffImage->extractMetadata(0);
    QFileInfo fileInfo(m_tiffImage->filename());
    if (metadata.imageName.isEmpty())
        metadata.imageName = fileInfo.fileName();
    ui->imageMetaWidget->setMetadata(metadata);

    statusBar()->showMessage(QStringLiteral("Reinterpreted with %1 channels - Size: %2x%3, Z:%4 T:%5 C:%6")
                                 .arg(count)
                                 .arg(m_tiffImage->sizeX())
                                 .arg(m_tiffImage->sizeY())
                                 .arg(m_tiffImage->sizeZ())
                                 .arg(m_tiffImage->sizeT())
                                 .arg(m_tiffImage->sizeC()));

    // Update display
    updateImage();
}
