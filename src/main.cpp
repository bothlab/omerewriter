/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mainwindow.h"

#include <QApplication>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // prefer dark color scheme
    a.styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    MainWindow w;
    w.show();
    return a.exec();
}
