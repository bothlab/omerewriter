/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "elidedlabel.h"

#include <QTextLayout>

ElidedLabel::ElidedLabel(QWidget *parent)
    : ElidedLabel(QString(), parent)
{
}

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
    : QLabel(parent),
      m_elideMode(Qt::ElideMiddle),
      m_realMinWidth(0)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setWordWrap(true);
    setText(text);
}

void ElidedLabel::setText(const QString &newText)
{
    m_rawText = newText;
    m_realMinWidth = minimumWidth();
    updateGeometry(); // Notify layout system that our size requirements changed
    updateElision();
}

void ElidedLabel::resizeEvent(QResizeEvent *)
{
    updateElision();
}

QSize ElidedLabel::sizeHint() const
{
    QFontMetrics metrics(font());
    int lineHeight = metrics.lineSpacing();

    // Always request 2 lines if there's a newline in the text
    int numLines = m_rawText.contains('\n') ? 2 : 1;
    int height = lineHeight * numLines;

    // For width, request enough for a reasonable amount of text
    int width = qMax(200, metrics.horizontalAdvance(m_rawText.left(40)));

    return QSize(width, height);
}

QSize ElidedLabel::minimumSizeHint() const
{
    QFontMetrics metrics(font());
    int lineHeight = metrics.lineSpacing();

    // Minimum must accommodate 2 lines if there's a newline
    int numLines = m_rawText.contains('\n') ? 2 : 1;
    int height = lineHeight * numLines;

    return QSize(50, height);
}

void ElidedLabel::updateElision()
{
    if (m_rawText.isEmpty()) {
        QLabel::setText(QString());
        return;
    }

    QFontMetrics metrics(font());
    const auto lineHeight = metrics.lineSpacing();
    const auto currentWidth = width();
    const auto currentHeight = height();

    // If width is too small (not yet laid out), don't do anything yet
    if (currentWidth < 10) {
        QLabel::setText(m_rawText);
        return;
    }

    // Calculate available lines based on height
    // During initial layout, allow up to 2 lines by default
    int availableLines = 2;
    if (currentHeight > lineHeight)
        availableLines = qMax(1, qMin(2, currentHeight / lineHeight)); // Cap at 2 lines

    // Split by explicit newlines first
    QStringList paragraphs = m_rawText.split('\n');
    QStringList lines;

    // Process each paragraph - for each paragraph, elide it if it's too long
    for (const QString &paragraph : paragraphs) {
        if (paragraph.isEmpty()) {
            // Preserve empty lines
            lines.append(QString());
            continue;
        }

        if (metrics.horizontalAdvance(paragraph) <= currentWidth) {
            // Fits without elision
            lines.append(paragraph);
        } else {
            // Too long - elide it
            lines.append(metrics.elidedText(paragraph, m_elideMode, currentWidth));
        }
    }

    // If we have too many lines, truncate and elide the last one
    if (lines.count() > availableLines) {
        lines = lines.mid(0, availableLines);
        if (availableLines > 0) {
            // Make sure the last line is elided if needed
            if (metrics.horizontalAdvance(lines[availableLines - 1]) > currentWidth)
                lines[availableLines - 1] = metrics.elidedText(lines[availableLines - 1], m_elideMode, currentWidth);
        }
    }

    QString elidedText = lines.join('\n');
    QLabel::setText(elidedText);

    if (!elidedText.isEmpty())
        setMinimumWidth((m_realMinWidth == 0) ? 1 : m_realMinWidth);
}
