// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QColor>
#include <algorithm>
#include <cmath>

namespace Analitza
{
inline double relativeLuminance(const QColor &color)
{
    auto linear = [](double c) {
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}
inline QColor readableColor(QColor color, const QColor &background, const QColor &text)
{
    const double base = relativeLuminance(background);
    for (int step = 0; step <= 20; ++step) {
        const double value = relativeLuminance(color);
        if ((std::max(base, value) + 0.05) / (std::min(base, value) + 0.05) >= 4.5)
            return color;
        color = QColor::fromRgbF(color.redF() * 0.8 + text.redF() * 0.2, color.greenF() * 0.8 + text.greenF() * 0.2, color.blueF() * 0.8 + text.blueF() * 0.2);
    }
    return text;
}
} // namespace Analitza
