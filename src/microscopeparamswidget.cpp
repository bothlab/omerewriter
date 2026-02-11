#include "microscopeparamswidget.h"
#include "ui_microscopeparamswidget.h"

MicroscopeParamsWidget::MicroscopeParamsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MicroscopeParamsWidget)
{
    ui->setupUi(this);
}

MicroscopeParamsWidget::~MicroscopeParamsWidget()
{
    delete ui;
}
