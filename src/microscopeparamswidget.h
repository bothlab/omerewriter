#ifndef MICROSCOPEPARAMSWIDGET_H
#define MICROSCOPEPARAMSWIDGET_H

#include <QWidget>

namespace Ui {
class MicroscopeParamsWidget;
}

class MicroscopeParamsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MicroscopeParamsWidget(QWidget *parent = nullptr);
    ~MicroscopeParamsWidget();

private:
    Ui::MicroscopeParamsWidget *ui;
};

#endif // MICROSCOPEPARAMSWIDGET_H
