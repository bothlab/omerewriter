/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "microscopeparamswidget.h"
#include "ui_microscopeparamswidget.h"

#include <QDebug>
#include <QFileInfo>

#include "utils.h"

using namespace ome::xml::model;

static QString acqModeDisplayName(enums::AcquisitionMode modeValue)
{
    static const QHash<int, QString> names = {
        {enums::AcquisitionMode::LASERSCANNINGCONFOCALMICROSCOPY,    QStringLiteral("Laser-Scanning Confocal")   },
        {enums::AcquisitionMode::MULTIPHOTONMICROSCOPY,              QStringLiteral("Multiphoton")               },
        {enums::AcquisitionMode::NEARFIELDSCANNINGOPTICALMICROSCOPY, QStringLiteral("Near-field scanning (NSOM)")},
        {enums::AcquisitionMode::SPINNINGDISKCONFOCAL,               QStringLiteral("Spinning Disk Confocal")    },
        {enums::AcquisitionMode::SECONDHARMONICGENERATIONIMAGING,    QStringLiteral("Second Harmonic Generation")},
    };
    if (names.contains(modeValue))
        return names.value(modeValue);
    // Fall back to the raw OME string
    return QString::fromStdString(std::string(modeValue));
}

MicroscopeParamsWidget::MicroscopeParamsWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MicroscopeParamsWidget)
{
    ui->setupUi(this);

    // Set up combo box known values
    ui->comboMicroscopeType->clear();
    for (auto v : enums::AcquisitionMode::values())
        ui->comboMicroscopeType->addItem(acqModeDisplayName(v.first), v.first);

    ui->comboLensImmersion->clear();
    for (auto v : enums::Immersion::values())
        ui->comboLensImmersion->addItem(QString::fromStdString(v.second), v.first);

    ui->comboEmbedding->clear();
    for (auto v : enums::Medium::values())
        ui->comboEmbedding->addItem(QString::fromStdString(v.second), v.first);

    // Set default (empty) values
    clearMetadata();

    // Channel selection
    connect(
        ui->listChannels, &QListWidget::currentRowChanged, this, &MicroscopeParamsWidget::onChannelSelectionChanged);

    // Sampling parameters
    connect(
        ui->spinXnm,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->spinYnm,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->spinZnm,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);

    // Optical parameters
    connect(
        ui->spinNA,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);

    // Spherical aberration parameters
    connect(
        ui->comboLensImmersion,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->spinLensRI,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->comboEmbedding,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);

    // Microscope parameters (channel-specific)
    connect(
        ui->comboMicroscopeType,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(ui->comboMicroscopeType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const bool isMultiphoton = ui->comboMicroscopeType->currentData().toInt()
                                   == enums::AcquisitionMode::MULTIPHOTONMICROSCOPY;
        ui->groupMultiphoton->setEnabled(isMultiphoton);
        if (isMultiphoton && ui->spinPhotonCount->value() < 2)
            ui->spinPhotonCount->setValue(2); // Multiphoton typically involves 2 or more photons
        else
            ui->spinPhotonCount->setValue(1);
    });
    connect(ui->editChannelLabel, &QLineEdit::textChanged, this, &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->spinPinholeNm,
        QOverload<int>::of(&QSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->spinExcitationNm,
        QOverload<int>::of(&QSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->spinEmissionNm,
        QOverload<int>::of(&QSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);
    connect(
        ui->spinPhotonCount,
        QOverload<int>::of(&QSpinBox::valueChanged),
        this,
        &MicroscopeParamsWidget::onMetadataFieldChanged);

    // Update channel label in list when changed
    connect(ui->editChannelLabel, &QLineEdit::textChanged, this, &MicroscopeParamsWidget::updateChannelInList);
    connect(
        ui->comboMicroscopeType,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &MicroscopeParamsWidget::updateChannelInList);
}

MicroscopeParamsWidget::~MicroscopeParamsWidget()
{
    delete ui;
}

void MicroscopeParamsWidget::setMetadata(const ImageMetadata &metadata)
{
    m_updatingUI = true;
    m_metadata = metadata;
    m_currentChannel = -1;

    auto imageNameShort = metadata.imageName;
    imageNameShort = QFileInfo(imageNameShort).baseName();

    // add linebreak in the middle if name is long
    if (imageNameShort.length() > 25) {
        qsizetype mid = imageNameShort.length() / 2;
        imageNameShort.insert(mid, "\n");
    }

    // Statistics group
    ui->lblImageNameVal->setText(metadata.imageName.isEmpty() ? tr("No image loaded!") : imageNameShort);
    ui->lblDimsVal->setText(QString("%1×%2×%3 (px)").arg(metadata.sizeX).arg(metadata.sizeY).arg(metadata.sizeZ));
    ui->lblChannelsVal->setText(QString::number(metadata.sizeC));
    ui->lblTypeVal->setText(metadata.pixelType);
    ui->lblSizeVal->setText(formatDataSize(metadata.dataSizeBytes));

    // Sampling parameters (nm)
    ui->spinXnm->setValue(metadata.physSizeXNm);
    ui->spinYnm->setValue(metadata.physSizeYNm);
    ui->spinZnm->setValue(metadata.physSizeZNm);

    // Optical parameters
    ui->spinNA->setValue(metadata.numericalAperture);

    // Lens immersion
    ui->comboLensImmersion->setCurrentIndex(metadata.lensImmersion);
    ui->spinLensRI->setValue(metadata.immersionRI);

    // Embedding medium
    ui->comboEmbedding->setCurrentIndex(metadata.embeddingMedium);

    // Populate channel list
    ui->listChannels->clear();
    for (size_t i = 0; i < metadata.channels.size(); ++i) {
        const auto &ch = metadata.channels[i];
        QString itemText = QString("%1: %2 - %3").arg(i).arg(acqModeDisplayName(ch.acquisitionMode)).arg(ch.name);
        ui->listChannels->addItem(itemText);
    }

    // Select first channel if available
    if (!metadata.channels.empty())
        ui->listChannels->setCurrentRow(0);

    m_updatingUI = false;
    m_modified = false;

    // update channel selection data
    onChannelSelectionChanged(0);
}

ImageMetadata MicroscopeParamsWidget::getMetadata() const
{
    ImageMetadata meta = m_metadata;

    // Update physical sizes
    meta.physSizeXNm = ui->spinXnm->value();
    meta.physSizeYNm = ui->spinYnm->value();
    meta.physSizeZNm = ui->spinZnm->value();

    // Update optical parameters
    meta.numericalAperture = ui->spinNA->value();

    // Lens immersion
    meta.lensImmersion = enums::Immersion::enum_value(ui->comboLensImmersion->currentData().toInt());
    meta.immersionRI = ui->spinLensRI->value();

    // Embedding medium
    meta.embeddingMedium = enums::Medium::enum_value(ui->comboEmbedding->currentData().toInt());

    return meta;
}

void MicroscopeParamsWidget::clearMetadata()
{
    m_updatingUI = true;
    m_metadata = ImageMetadata();
    m_currentChannel = -1;

    // Statistics
    ui->lblImageNameVal->setText(tr("No image loaded!"));
    ui->lblDimsVal->setText("0×0×0 (px)");
    ui->lblChannelsVal->setText("0");
    ui->lblTypeVal->setText("-");
    ui->lblSizeVal->setText("0 B");

    // Sampling
    ui->spinXnm->setValue(0);
    ui->spinYnm->setValue(0);
    ui->spinZnm->setValue(0);

    // Optical
    ui->spinNA->setValue(0);

    // Mediums
    ui->comboLensImmersion->setCurrentIndex(enums::Immersion::WATER);
    ui->spinLensRI->setValue(1.0);
    ui->comboEmbedding->setCurrentIndex(enums::Immersion::WATER);

    // Channels
    ui->listChannels->clear();
    ui->comboMicroscopeType->setCurrentIndex(enums::AcquisitionMode::LASERSCANNINGCONFOCALMICROSCOPY);
    ui->groupMultiphoton->setEnabled(false);
    ui->editChannelLabel->clear();
    ui->spinPinholeNm->setValue(0);
    ui->spinExcitationNm->setValue(0);
    ui->spinEmissionNm->setValue(0);
    ui->spinPhotonCount->setValue(1);

    m_updatingUI = false;
    m_modified = false;
}

bool MicroscopeParamsWidget::isModified() const
{
    return m_modified;
}

void MicroscopeParamsWidget::resetModified()
{
    m_modified = false;
}

void MicroscopeParamsWidget::onChannelSelectionChanged(int row)
{
    if (m_updatingUI || row < 0)
        return;

    // Save current channel data before switching
    if (m_currentChannel >= 0 && m_currentChannel < static_cast<int>(m_metadata.channels.size())) {
        saveCurrentChannelData();
    }

    m_currentChannel = row;
    updateChannelUI(row);
}

void MicroscopeParamsWidget::onMetadataFieldChanged()
{
    if (m_updatingUI)
        return;

    m_modified = true;

    // Save current channel data if a channel is selected
    if (m_currentChannel >= 0 && m_currentChannel < static_cast<int>(m_metadata.channels.size()))
        saveCurrentChannelData();

    emit metadataModified();
}

void MicroscopeParamsWidget::updateChannelInList()
{
    if (m_updatingUI || m_currentChannel < 0)
        return;

    int row = m_currentChannel;
    if (row >= 0 && row < ui->listChannels->count()) {
        const int modeValue = ui->comboMicroscopeType->currentData().toInt();
        const QString itemText = QString("%1: %2 - %3")
                                     .arg(row)
                                     .arg(acqModeDisplayName(enums::AcquisitionMode::enum_value(modeValue)))
                                     .arg(ui->editChannelLabel->text());
        ui->listChannels->item(row)->setText(itemText);
    }
}

void MicroscopeParamsWidget::updateChannelUI(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= static_cast<int>(m_metadata.channels.size()))
        return;

    m_updatingUI = true;

    const auto &ch = m_metadata.channels[channelIndex];

    // Microscope type
    ui->comboMicroscopeType->setCurrentIndex(ch.acquisitionMode);
    ui->groupMultiphoton->setEnabled(ch.acquisitionMode == enums::AcquisitionMode::MULTIPHOTONMICROSCOPY);
    ui->spinPhotonCount->setValue(ch.photonCount);

    // Channel label
    ui->editChannelLabel->setText(ch.name);

    // Wavelengths
    ui->spinPinholeNm->setValue(static_cast<int>(ch.pinholeSizeNm));
    ui->spinExcitationNm->setValue(static_cast<int>(ch.exWavelengthNm));
    ui->spinEmissionNm->setValue(static_cast<int>(ch.emWavelengthNm));

    m_updatingUI = false;
}

void MicroscopeParamsWidget::saveCurrentChannelData()
{
    if (m_currentChannel < 0 || m_currentChannel >= static_cast<int>(m_metadata.channels.size()))
        return;

    auto &ch = m_metadata.channels[m_currentChannel];

    // Microscope type
    ch.acquisitionMode = enums::AcquisitionMode::enum_value(ui->comboMicroscopeType->currentData().toInt());
    ch.photonCount = ui->spinPhotonCount->value();

    // Channel label
    ch.name = ui->editChannelLabel->text();

    // Wavelengths
    ch.pinholeSizeNm = ui->spinPinholeNm->value();
    ch.exWavelengthNm = ui->spinExcitationNm->value();
    ch.emWavelengthNm = ui->spinEmissionNm->value();
}
