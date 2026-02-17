/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ometiffreader.h"

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

    // Update status bar
    statusBar()->showMessage(
        tr("Loaded: %1 - Size: %2x%3, Z:%4 T:%5 C:%6")
            .arg(fileInfo.fileName())
            .arg(m_reader->sizeX())
            .arg(m_reader->sizeY())
            .arg(m_reader->sizeZ())
            .arg(m_reader->sizeT())
            .arg(m_reader->sizeC()));
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
    if (value != m_currentZ) {
        m_currentZ = value;
        updateImage();
    }
}

void MainWindow::onSliderTChanged(int value)
{
    if (value != m_currentT) {
        m_currentT = value;
        updateImage();
    }
}

void MainWindow::onSliderCChanged(int value)
{
    if (value != m_currentC) {
        m_currentC = value;
        updateImage();
    }
}
