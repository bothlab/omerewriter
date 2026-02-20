/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTemporaryDir>
#include <QThread>
#include <QSettings>
#include <QDebug>
#include <atomic>

#include "ometiffimage.h"
#include "microscopeparamswidget.h"
#include "metadatajson.h"
#include "savedparamsmanager.h"
#include "rangeslider.h"
#include "utils.h"

/**
 * @brief Worker class for saving OME-TIFF files in a background thread
 */
class TiffSaveWorker : public QObject
{
    Q_OBJECT

public:
    TiffSaveWorker(
        OMETiffImage *tiffImage,
        const QString &outputPath,
        const ImageMetadata &metadata,
        QObject *parent = nullptr)
        : QObject(parent),
          m_tiffImage(tiffImage),
          m_outputPath(outputPath),
          m_metadata(metadata),
          m_cancelled(false)
    {
    }

    void cancel()
    {
        m_cancelled.store(true);
    }

public slots:
    void doSave()
    {
        auto result = m_tiffImage->saveWithMetadata(
            m_outputPath,
            m_metadata,
            [this](OMETiffImage::dimension_size_type current, OMETiffImage::dimension_size_type total) {
                emit progressChanged(current, total);
                return !m_cancelled.load();
            });

        if (result)
            emit finished(true, QString());
        else
            emit finished(false, result.error());
    }

signals:
    void progressChanged(uint current, uint total);
    void finished(bool success, const QString &errorMessage);

private:
    OMETiffImage *m_tiffImage;
    QString m_outputPath;
    ImageMetadata m_metadata;
    std::atomic_bool m_cancelled;
};

#include "mainwindow.moc"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_tiffImage(std::make_unique<OMETiffImage>(this)),
      m_savedParamsManager(std::make_unique<SavedParamsManager>(this))
{
    ui->setupUi(this);

    // Menu actions
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenFile);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::onSaveFile);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::onSaveFileAs);
    connect(ui->actionLoadParams, &QAction::triggered, this, &MainWindow::onLoadParamsClicked);
    connect(ui->btnLoadTiff, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(ui->btnQuickSave, &QPushButton::clicked, this, &MainWindow::quickSaveFile);

    // Parameter management
    connect(ui->btnSaveParams, &QPushButton::clicked, this, &MainWindow::onSaveParamsClicked);
    connect(ui->btnQuickLoadParams, &QPushButton::clicked, this, &MainWindow::onQuickLoadParamsClicked);
    connect(ui->btnRemoveParamsFromList, &QPushButton::clicked, this, &MainWindow::onRemoveParamsFromListClicked);
    connect(ui->listSavedParams, &QListWidget::itemDoubleClicked, this, &MainWindow::onQuickLoadParamsClicked);
    connect(m_savedParamsManager.get(), &SavedParamsManager::filesChanged, this, &MainWindow::updateSavedParamsList);

    // Slider connections
    connect(ui->sliderZ, &QSlider::valueChanged, this, &MainWindow::onSliderZChanged);
    connect(ui->sliderT, &QSlider::valueChanged, this, &MainWindow::onSliderTChanged);
    connect(ui->sliderC, &QSlider::valueChanged, this, &MainWindow::onSliderCChanged);
    connect(ui->contrastSlider, &RangeSlider::valuesChanged, ui->imageView, &ImageViewWidget::setPixelRange);

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

    // Initialize saved params list
    updateSavedParamsList();

    // Restore window geometry and state from previous session
    restoreWindowState();
}

MainWindow::~MainWindow()
{
    saveWindowState();
    delete ui;
}

bool MainWindow::openFile(const QString &filename)
{
    if (filename.isEmpty())
        return false;

    if (!m_tiffImage->open(filename)) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Failed to open file:\n%1").arg(filename));
        return false;
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

    resetSliderValues();

    // Enable navigation controls
    setNavigationEnabled(true);

    // Load and display metadata in the params widget
    ImageMetadata metadata = m_tiffImage->extractMetadata(0);
    if (metadata.imageName.isEmpty())
        metadata.imageName = fileInfo.fileName();

    // Initialize contrast slider BEFORE displaying the image
    updateContrastSliderRange(metadata);

    // Display the first image
    updateImage();

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

    return true;
}

void MainWindow::onOpenFile()
{
    const auto lastDir = getLastDirectory("openTiff");

    QString filename = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open TIFF Image"),
        lastDir,
        QStringLiteral(
            "All TIFF Files (*.ome.tiff *.ome.tif *.tiff *.tif);;OME-TIFF Files (*.ome.tiff *.ome.tif);;TIFF Files "
            "(*.tiff *.tif);;All Files (*)"));

    if (!filename.isEmpty()) {
        setLastDirectory("openTiff", filename);
        openFile(filename);
    }
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

void MainWindow::updateContrastSliderRange(const ImageMetadata &metadata)
{
    // Determine maximum pixel value based on image bit depth
    int maxPixelValue = 255; // Default to 8-bit

    if (metadata.pixelType.contains("uint16", Qt::CaseInsensitive)
        || metadata.pixelType.contains("int16", Qt::CaseInsensitive)) {
        maxPixelValue = 65535;
    } else if (
        metadata.pixelType.contains("uint32", Qt::CaseInsensitive)
        || metadata.pixelType.contains("int32", Qt::CaseInsensitive)) {
        // 32-bit images are typically normalized to 16-bit in the reader
        maxPixelValue = 65535;
    } else if (metadata.pixelType.contains("float", Qt::CaseInsensitive)) {
        // Float images are normalized to 16-bit in the reader
        maxPixelValue = 65535;
    }

    qDebug() << "Initializing contrast slider for pixel type:" << metadata.pixelType << "with range 0 -"
             << maxPixelValue;

    // Block signals temporarily to avoid triggering updates during setup
    ui->contrastSlider->blockSignals(true);
    ui->contrastSlider->setRange(0, maxPixelValue);
    ui->contrastSlider->setValues(0, maxPixelValue);
    ui->contrastSlider->blockSignals(false);

    // Always set the pixel range explicitly to ensure it's applied
    ui->imageView->setPixelRange(0, maxPixelValue);
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
    if (wasOmeTiff) {
        destFilename = m_tiffImage->filename();
    } else {
        // For raw TIFF, we save the modified OME-TIFF alongside the original, with a modified name
        destFilename = tiffDir.absoluteFilePath(tiffBasename + ".ome.tiff");

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

    // Create a temporary directory in the same location as the destination
    // This ensures: 1) disk space available, 2) same filesystem for atomic move
    // 3) correct filename in OME-XML metadata (no temp filename warnings)
    QTemporaryDir tempDir(tiffDir.absoluteFilePath("_temp-omewrite"));
    if (!tempDir.isValid()) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Failed to create temporary directory."));
        return;
    }
    tempDir.setAutoRemove(true);

    // Write to temp directory using the final filename
    // This way the OME-XML metadata will contain the correct filename
    QFileInfo destFi(destFilename);
    QString tempFile = tempDir.filePath(destFi.fileName());

    ImageMetadata metadata = ui->imageMetaWidget->getMetadata();

    bool success = performSaveWithProgress(tempFile, metadata);
    if (!success)
        return;

    // Close the original file
    m_tiffImage->close();

    // Move from temp directory to final destination
    QFile::remove(destFilename);
    if (QFile::rename(tempFile, destFilename)) {
        // Reopen the file
        if (openFile(destFilename)) {
            ui->imageMetaWidget->resetModified();
            statusBar()->showMessage(QStringLiteral("Saved: %1").arg(destFilename), 5000);
        } else {
            QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Failed to reopen the saved file."));
        }
    } else {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Failed to replace the original file."));
        // Try to recover by opening the temp file
        if (QFile::exists(tempFile))
            openFile(tempFile);
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
                    QStringLiteral("Failed to delete original file '%1'. Please check if it can be deleted manually.")
                        .arg(origFilename));
            }
        }
    }
}

void MainWindow::onSaveFile()
{
    saveCurrentFile(false);
}

void MainWindow::quickSaveFile()
{
    saveCurrentFile(true);
}

void MainWindow::onSaveFileAs()
{
    if (!m_tiffImage->isOpen()) {
        QMessageBox::warning(this, QStringLiteral("Warning"), QStringLiteral("No file is currently open."));
        return;
    }

    const auto lastDir = getLastDirectory("saveTiff", getLastDirectory("openTiff"));
    QString filename = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save OME-TIFF As"),
        lastDir,
        QStringLiteral("OME-TIFF Files (*.ome.tiff *.ome.tif);;All Files (*)"));

    if (filename.isEmpty())
        return;
    setLastDirectory("saveTiff", filename);

    // Ensure proper extension
    if (!filename.endsWith(".ome.tiff", Qt::CaseInsensitive) && !filename.endsWith(".ome.tif", Qt::CaseInsensitive))
        filename += ".ome.tiff";

    ImageMetadata metadata = ui->imageMetaWidget->getMetadata();

    bool success = performSaveWithProgress(filename, metadata);
    if (!success)
        return;

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
        openFile(filename);
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
    resetSliderValues();

    // Update metadata widget with new dimensions
    ImageMetadata metadata = m_tiffImage->extractMetadata(0);
    QFileInfo fileInfo(m_tiffImage->filename());
    if (metadata.imageName.isEmpty())
        metadata.imageName = fileInfo.fileName();
    ui->imageMetaWidget->setMetadata(metadata);

    // Reset contrast slider in case the bit depth or interpretation changed
    updateContrastSliderRange(metadata);

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

void MainWindow::resetSliderValues()
{
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
}

bool MainWindow::performSaveWithProgress(const QString &filename, const ImageMetadata &metadata)
{
    // Create worker and thread
    auto thread = new QThread(this);
    auto worker = new TiffSaveWorker(m_tiffImage.get(), filename, metadata);
    worker->moveToThread(thread);

    // Create progress dialog
    auto progressDlg = new QProgressDialog(
        QStringLiteral("Saving TIFF planes..."), QStringLiteral("Cancel"), 0, 100, this);
    progressDlg->setWindowTitle(QStringLiteral("Saving OME-TIFF file..."));
    progressDlg->setMinimumWidth(400);
    progressDlg->setWindowModality(Qt::WindowModal);
    progressDlg->setMinimumDuration(0);
    progressDlg->setValue(0);

    bool success = false;
    QString errorMessage;

    connect(thread, &QThread::started, worker, &TiffSaveWorker::doSave);
    connect(worker, &TiffSaveWorker::progressChanged, this, [progressDlg](int current, int total) {
        if (total <= 0)
            return;

        int percentage = (current * 100) / total;
        progressDlg->setValue(percentage);
        progressDlg->setLabelText(QStringLiteral("Writing plane %1 of %2").arg(current + 1).arg(total));
    });

    connect(
        worker,
        &TiffSaveWorker::finished,
        this,
        [&success, &errorMessage, progressDlg, worker, thread](bool ok, const QString &error) {
            success = ok;
            errorMessage = error;
            progressDlg->setValue(progressDlg->maximum());
            progressDlg->close();
            thread->quit();
        });

    connect(progressDlg, &QProgressDialog::canceled, worker, &TiffSaveWorker::cancel, Qt::DirectConnection);

    // cleanup everything later
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, progressDlg, &QObject::deleteLater);

    thread->start();
    while (thread->isRunning()) {
        QApplication::processEvents();
        QThread::msleep(10);
    }

    if (!success)
        QMessageBox::critical(
            this, QStringLiteral("Failed to save TIFF file"), errorMessage.isEmpty() ? "Unknown error!" : errorMessage);

    return success;
}

void MainWindow::onSaveParamsClicked()
{
    const auto metadata = ui->imageMetaWidget->getMetadata();

    const auto lastDir = getLastDirectory("saveParams");
    auto filename = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Microscope Parameters"),
        lastDir,
        QStringLiteral("JSON Files (*.json);;All Files (*)"));

    if (filename.isEmpty())
        return;
    setLastDirectory("saveParams", filename);

    if (!filename.endsWith(".json", Qt::CaseInsensitive))
        filename += ".json";

    const auto result = MetadataJson::saveToFile(metadata, filename);
    if (!result) {
        QMessageBox::critical(
            this, QStringLiteral("Save Failed"), QStringLiteral("Failed to save parameters:\n%1").arg(result.error()));
        return;
    }

    // Add to saved list
    m_savedParamsManager->addFile(filename);
    statusBar()->showMessage(tr("Parameters saved to: %1").arg(filename), 5000);
}

void MainWindow::onLoadParamsClicked()
{
    const auto lastDir = getLastDirectory("loadParams", getLastDirectory("saveParams"));
    const auto filename = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Load Microscope Parameters"),
        lastDir,
        QStringLiteral("JSON Files (*.json);;All Files (*)"));

    if (filename.isEmpty())
        return;
    setLastDirectory("loadParams", filename);

    loadParametersFromFile(filename);

    // Add to saved list for future quick access
    m_savedParamsManager->addFile(filename);
}

void MainWindow::onQuickLoadParamsClicked()
{
    auto selectedItems = ui->listSavedParams->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("No Selection"), QStringLiteral("Please select a parameter file from the list."));
        return;
    }

    int selectedRow = ui->listSavedParams->row(selectedItems.first());
    QStringList files = m_savedParamsManager->getFiles();

    if (selectedRow < 0 || selectedRow >= files.size()) {
        QMessageBox::warning(this, QStringLiteral("Invalid Selection"), QStringLiteral("Selected item is invalid."));
        return;
    }

    loadParametersFromFile(files[selectedRow]);
}

void MainWindow::onRemoveParamsFromListClicked()
{
    auto selectedItems = ui->listSavedParams->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("No Selection"),
            QStringLiteral("Please select a parameter file to remove from the list."));
        return;
    }

    int selectedRow = ui->listSavedParams->row(selectedItems.first());
    QStringList files = m_savedParamsManager->getFiles();

    if (selectedRow < 0 || selectedRow >= files.size())
        return;

    // Unregister with the manager
    m_savedParamsManager->removeFile(files[selectedRow]);
}

void MainWindow::updateSavedParamsList()
{
    ui->listSavedParams->clear();

    // Populate list with display names
    ui->listSavedParams->addItems(m_savedParamsManager->getDisplayNames());

    // Set tooltips with full paths
    QStringList files = m_savedParamsManager->getFiles();
    for (int i = 0; i < ui->listSavedParams->count(); ++i) {
        if (i < files.size())
            ui->listSavedParams->item(i)->setToolTip(files[i]);
    }
}

void MainWindow::loadParametersFromFile(const QString &filePath)
{
    // Check if file exists
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(
            this,
            QStringLiteral("File Not Found"),
            QStringLiteral("The file does not exist:\n%1\n\nIt will be removed from the list.").arg(filePath));
        m_savedParamsManager->removeFile(filePath);
        return;
    }

    auto result = MetadataJson::loadFromFile(filePath);
    if (!result) {
        QMessageBox::critical(
            this, QStringLiteral("Load Failed"), QStringLiteral("Failed to load parameters:\n%1").arg(result.error()));
        return;
    }

    auto loadedMeta = result.value();

    // Preserve the current image dimensions and name
    const auto currentMeta = ui->imageMetaWidget->getMetadata();
    loadedMeta.sizeX = currentMeta.sizeX;
    loadedMeta.sizeY = currentMeta.sizeY;
    loadedMeta.sizeZ = currentMeta.sizeZ;
    loadedMeta.sizeC = currentMeta.sizeC;
    loadedMeta.sizeT = currentMeta.sizeT;
    loadedMeta.pixelType = currentMeta.pixelType;
    loadedMeta.dataSizeBytes = currentMeta.dataSizeBytes;
    loadedMeta.imageName = currentMeta.imageName;

    // Adjust channels to match current image
    if (loadedMeta.channels.size() != currentMeta.channels.size()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Channel Count Mismatch"),
            QStringLiteral(
                "The loaded parameters have %1 channel(s), but the current image has %2 channel(s).\n"
                "Only the overlapping channels will be updated.")
                .arg(loadedMeta.channels.size())
                .arg(currentMeta.channels.size()));

        // Resize to match current image
        if (loadedMeta.channels.size() > currentMeta.channels.size()) {
            loadedMeta.channels.resize(currentMeta.channels.size());
        } else {
            // Keep existing channel data for channels beyond what's loaded
            size_t loadedSize = loadedMeta.channels.size();
            loadedMeta.channels.resize(currentMeta.channels.size());
            for (size_t i = loadedSize; i < currentMeta.channels.size(); ++i)
                loadedMeta.channels[i] = currentMeta.channels[i];
        }
    }

    // Apply loaded metadata
    ui->imageMetaWidget->setMetadata(loadedMeta);

    QFileInfo fileInfo(filePath);
    statusBar()->showMessage(QStringLiteral("Parameters loaded from: %1").arg(fileInfo.fileName()), 5000);
}

void MainWindow::saveWindowState()
{
    QSettings settings("OMERewriter", "OMERewriter");

    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());

    settings.sync();
}

void MainWindow::restoreWindowState()
{
    QSettings settings("OMERewriter", "OMERewriter");

    const QByteArray geometry = settings.value("window/geometry").toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    // Restore window state (includes dock widget positions and visibility)
    const QByteArray state = settings.value("window/state").toByteArray();
    if (!state.isEmpty())
        restoreState(state);
}

QString MainWindow::getLastDirectory(const QString &key, const QString &defaultDir) const
{
    QSettings settings("OMERewriter", "OMERewriter");
    QString lastDir = settings.value("directories/" + key).toString();

    if (lastDir.isEmpty())
        return defaultDir;
    if (!QDir(lastDir).exists())
        return QDir::homePath();

    return lastDir;
}

void MainWindow::setLastDirectory(const QString &key, const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    QFileInfo fileInfo(filePath);
    QString dirPath = fileInfo.isDir() ? fileInfo.absoluteFilePath() : fileInfo.absolutePath();

    if (QDir(dirPath).exists()) {
        QSettings settings("OMERewriter", "OMERewriter");
        settings.setValue("directories/" + key, dirPath);
        settings.sync();
    }
}
