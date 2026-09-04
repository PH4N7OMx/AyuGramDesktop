#pragma once

#include "ayu/ayu_settings.h"

#include <QtGui/QPainter>
#include <QtWidgets/QWidget>

namespace AyuLiquidGlass {

[[nodiscard]] bool isEnabled(LiquidGlassMode mode = LiquidGlassMode::ChatBars);
[[nodiscard]] bool isFull();

void paintGlass(
	QPainter &p,
	const QRect &rect,
	const QColor &tintColor,
	QWidget *widget = nullptr,
	bool topRim = true,
	bool bottomRim = true);

} // namespace AyuLiquidGlass
