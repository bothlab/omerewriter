/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ometiffreader.h"
#include "microscopeparamswidget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_reader(std::make_unique<OMETiffReader>(this))
{
    ui->setupUi(this);

    setupConnections();
    setNavigationEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // Menu actions
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveFile);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::saveFileAs);

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
    connect(ui->imageMetaWidget, &MicroscopeParamsWidget::metadataModified,
            this, &MainWindow::onMetadataModified);
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Open OME-TIFF Image"),
        QString(),
        tr("OME-TIFF Files (*.ome.tiff *.ome.tif *.tiff *.tif);;All Files (*)"));

    if (filename.isEmpty())
        return;

    if (!m_reader->open(filename)) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to open file:\n%1").arg(filename));
        return;
    }

    // Update window title
    QFileInfo fileInfo(filename);
    setWindowTitle(tr("OME-TIFF Viewer - %1").arg(fileInfo.fileName()));

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
    ImageMetadata metadata = m_reader->extractMetadata(0);
    if (metadata.imageName.isEmpty()) {
        metadata.imageName = fileInfo.fileName();
    }
    ui->imageMetaWidget->setMetadata(metadata);

    // Update status bar
    statusBar()->showMessage(
        tr("Loaded: %1 - Size: %2x%3, Z:%4 T:%5 C:%6")
            .arg(fileInfo.fileName())
            .arg(m_reader->sizeX())
            .arg(m_reader->sizeY())
            .arg(m_reader->sizeZ())
            .arg(m_reader->sizeT())
            .arg(m_reader->sizeC()));

    ui->actionSave->setEnabled(true);
    ui->actionSaveAs->setEnabled(true);
}

void MainWindow::updateSliderRanges()
{
    if (!m_reader->isOpen())
        return;

    auto sizeZ = m_reader->sizeZ();
    auto sizeT = m_reader->sizeT();
    auto sizeC = m_reader->sizeC();

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
    if (!m_reader->isOpen())
        return;

    RawImage image = m_reader->readPlane(m_currentZ, m_currentC, m_currentT);

    if (image.isEmpty()) {
        qWarning() << "Failed to read plane at Z=" << m_currentZ
                   << "T=" << m_currentT << "C=" << m_currentC;
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

void MainWindow::saveFile()
{
    if (!m_reader->isOpen()) {
        QMessageBox::warning(this, tr("Warning"), tr("No file is currently open."));
        return;
    }

    // For save (not save-as), we need to save to a temporary file and then replace
    // because we can't write to a file that's currently open for reading
    QString originalFile = m_reader->filename();
    QString tempFile = originalFile + ".tmp.ome.tiff";

    ImageMetadata metadata = ui->imageMetaWidget->getMetadata();

    if (m_reader->saveWithMetadata(tempFile, metadata)) {
        // Close the original file
        m_reader->close();

        // Replace original with temp file
        QFile::remove(originalFile);
        if (QFile::rename(tempFile, originalFile)) {
            // Reopen the file
            if (m_reader->open(originalFile)) {
                ui->imageMetaWidget->resetModified();
                statusBar()->showMessage(tr("Saved: %1").arg(originalFile), 5000);
            } else {
                QMessageBox::critical(this, tr("Error"), tr("Failed to reopen the saved file."));
            }
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Failed to replace the original file."));
            // Try to recover by opening the temp file
            if (QFile::exists(tempFile)) {
                m_reader->open(tempFile);
            }
        }
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save the file."));
        QFile::remove(tempFile);
    }
}

void MainWindow::saveFileAs()
{
    if (!m_reader->isOpen()) {
        QMessageBox::warning(this, tr("Warning"), tr("No file is currently open."));
        return;
    }

    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save OME-TIFF As"),
        QString(),
        tr("OME-TIFF Files (*.ome.tiff *.ome.tif);;All Files (*)"));

    if (filename.isEmpty())
        return;

    // Ensure proper extension
    if (!filename.endsWith(".ome.tiff", Qt::CaseInsensitive) &&
        !filename.endsWith(".ome.tif", Qt::CaseInsensitive)) {
        filename += ".ome.tiff";
    }

    ImageMetadata metadata = ui->imageMetaWidget->getMetadata();

    if (m_reader->saveWithMetadata(filename, metadata)) {
        ui->imageMetaWidget->resetModified();
        statusBar()->showMessage(tr("Saved as: %1").arg(filename), 5000);

        // Ask if user wants to open the newly saved file
        auto result = QMessageBox::question(this, tr("Open Saved File"),
            tr("Do you want to open the newly saved file?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        if (result == QMessageBox::Yes) {
            m_reader->close();
            if (m_reader->open(filename)) {
                QFileInfo fileInfo(filename);
                setWindowTitle(tr("OME-TIFF Viewer - %1").arg(fileInfo.fileName()));
                updateSliderRanges();
                updateImage();
                ImageMetadata newMeta = m_reader->extractMetadata(0);
                if (newMeta.imageName.isEmpty()) {
                    newMeta.imageName = fileInfo.fileName();
                }
                ui->imageMetaWidget->setMetadata(newMeta);
            }
        }
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save the file."));
    }
}

void MainWindow::onMetadataModified()
{
    // Update window title to indicate unsaved changes
    QString title = windowTitle();
    if (!title.endsWith(" *")) {
        setWindowTitle(title + " *");
    }
}
