/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("OMERewriter"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/appicon")));

    // prefer dark color scheme
    a.styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    MainWindow w;
    w.show();
    return a.exec();
}
