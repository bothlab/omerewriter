/*
 * Copyright (C) 2022-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>

/**
 * @brief Create a random alphanumeric string with the given length.
 */
QString createRandomString(int len);

/**
 * @brief Format a byte count into a human-readable string with appropriate units (KB, MB, GB, etc.)
 */
QString formatDataSize(size_t bytes);
