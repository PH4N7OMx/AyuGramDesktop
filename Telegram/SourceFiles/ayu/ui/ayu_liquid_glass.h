#pragma once

#include <QtGui/QPainter>
#include <QtWidgets/QWidget>

enum class LiquidGlassMode;

namespace AyuLiquidGlass {

[[nodiscard]] bool isEnabled();
[[nodiscard]] bool isFull();

void paintGlass(
	QPainter &p,
	const QRect &rect,
	const QColor &tintColor,
	QWidget *widget = nullptr,
	bool topRim = true,
	bool bottomRim = true);

} // namespace AyuLiquidGlass
