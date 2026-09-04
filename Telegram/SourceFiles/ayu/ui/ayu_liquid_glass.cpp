#include "ayu/ui/ayu_liquid_glass.h"

#include "ayu/ayu_settings.h"
#include "window/themes/window_theme.h"
#include "ui/image/image_prepare.h"

#include <algorithm>
#include <QtGui/QLinearGradient>
#include <QtGui/QPainter>

namespace AyuLiquidGlass {

bool isEnabled(LiquidGlassMode mode) {
	const auto current = AyuSettings::getInstance().liquidGlassMode();
	if (current == LiquidGlassMode::Disabled) {
		return false;
	}
	if (mode == LiquidGlassMode::Full) {
		return current == LiquidGlassMode::Full;
	}
	return true;
}

bool isFull() {
	return isEnabled(LiquidGlassMode::Full);
}

void paintGlass(
		QPainter &p,
		const QRect &rect,
		const QColor &tintColor,
		QWidget *widget,
		bool topRim,
		bool bottomRim) {
	if (!isEnabled()) {
		p.fillRect(rect, tintColor);
		return;
	}

	const auto isLight = (tintColor.lightness() > 128);

	const auto bg = Window::Theme::Background();
	if (bg && widget) {
		const auto &prepared = bg->prepared();
		if (!prepared.isNull()) {
			const auto globalPos = widget->mapToGlobal(rect.topLeft());
			const auto bgRect = QRect(
				std::clamp(std::abs(globalPos.x()) % std::max(prepared.width(), 1), 0, std::max(prepared.width() - 1, 0)),
				std::clamp(std::abs(globalPos.y()) % std::max(prepared.height(), 1), 0, std::max(prepared.height() - 1, 0)),
				std::min(rect.width(), prepared.width()),
				std::min(rect.height(), prepared.height()));
			if (bgRect.width() > 0 && bgRect.height() > 0) {
				auto cropped = prepared.copy(bgRect);
				if (!cropped.isNull()) {
					auto blurred = Images::Blur(std::move(cropped));
					p.drawImage(rect, blurred);
				}
			}
		} else if (const auto fill = bg->colorForFill()) {
			p.fillRect(rect, *fill);
		}
	}

	const auto alpha = isLight ? 200 : 185;
	const auto glassTint = QColor(
		tintColor.red(),
		tintColor.green(),
		tintColor.blue(),
		alpha);
	p.fillRect(rect, glassTint);

	QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
	if (isLight) {
		gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
		gradient.setColorAt(0.15, QColor(255, 255, 255, 30));
		gradient.setColorAt(0.85, QColor(255, 255, 255, 10));
		gradient.setColorAt(1.0, QColor(0, 0, 0, 15));
	} else {
		gradient.setColorAt(0.0, QColor(255, 255, 255, 35));
		gradient.setColorAt(0.15, QColor(255, 255, 255, 15));
		gradient.setColorAt(0.85, QColor(255, 255, 255, 5));
		gradient.setColorAt(1.0, QColor(0, 0, 0, 30));
	}
	p.fillRect(rect, gradient);

	if (topRim) {
		const auto rimColor = isLight
			? QColor(255, 255, 255, 160)
			: QColor(255, 255, 255, 55);
		p.fillRect(QRect(rect.x(), rect.y(), rect.width(), 1), rimColor);
	}

	if (bottomRim) {
		const auto dividerColor = isLight
			? QColor(0, 0, 0, 30)
			: QColor(255, 255, 255, 20);
		p.fillRect(QRect(rect.x(), rect.bottom(), rect.width(), 1), dividerColor);
	}
}

} // namespace AyuLiquidGlass
