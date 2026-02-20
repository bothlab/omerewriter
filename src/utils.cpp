/*
 * Copyright (C) 2022-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "utils.h"

#include <QDir>
#include <QRandomGenerator>

QString createRandomString(int len)
{
    const auto possibleChars = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    QString str;
    for (int i = 0; i < len; i++) {
        const auto index = QRandomGenerator::global()->generate() % possibleChars.length();
        QChar nextChar = possibleChars.at(index);
        str.append(nextChar);
    }

    return str;
}

QString formatDataSize(size_t bytes)
{
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    const double TB = GB * 1024.0;

    if (bytes >= TB)
        return QString("%1 TiB").arg(bytes / TB, 0, 'f', 2);
    if (bytes >= GB)
        return QString("%1 GiB").arg(bytes / GB, 0, 'f', 2);
    if (bytes >= MB)
        return QString("%1 MiB").arg(bytes / MB, 0, 'f', 1);
    if (bytes >= KB)
        return QString("%1 KiB").arg(bytes / KB, 0, 'f', 1);

    return QString("%1 B").arg(bytes);
}
