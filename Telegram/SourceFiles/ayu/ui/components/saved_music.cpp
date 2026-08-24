// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/components/saved_music.h"

#include "ayu/ayu_settings.h"
#include "ayu/ui/utils/color_utils.h"
#include "ayu/ui/utils/itunes_search.h"
#include "ayu/ui/utils/palette.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "info/profile/info_profile_music_button.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "styles/palette.h"
#include "styles/style_info.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/image/image.h"
#include "ui/widgets/labels.h"
#include "window/themes/window_theme.h"

#include <QSvgRenderer>

namespace Info::Profile {

namespace {

constexpr auto kCoverTransitionDuration = crl::time(180);

QColor performerColor(255, 255, 255, 153); // white 60%

QRgb AdjustHsl(QRgb color, float luminance, float saturation = -1.0f) {
	auto hsl = Ayu::Ui::ColorUtils::colorToHSL(color);

	if (saturation > 0.0f) {
		hsl[1] = std::min(hsl[1] * saturation, 1.0f);
	}

	hsl[2] = std::min(hsl[2] * luminance, 1.0f);

	return Ayu::Ui::ColorUtils::HSLToRGB(hsl);
}

QRgb BlendARGB(QRgb color1, QRgb color2, float ratio) {
	const auto inverseRatio = 1.0f - ratio;
	const auto r = static_cast<int>(qRed(color1) * inverseRatio + qRed(color2) * ratio);
	const auto g = static_cast<int>(qGreen(color1) * inverseRatio + qGreen(color2) * ratio);
	const auto b = static_cast<int>(qBlue(color1) * inverseRatio + qBlue(color2) * ratio);
	const auto a = static_cast<int>(qAlpha(color1) * inverseRatio + qAlpha(color2) * ratio);
	return qRgba(r, g, b, a);
}

QColor GetNoCoverBgColor(std::optional<QColor> overrideBg) {
	if (overrideBg) {
		return Ui::BlendColors(
			*overrideBg,
			Qt::black,
			st::infoProfileTopBarActionButtonBgOpacity);
	}

	return st::shadowFg->c;
}

void PaintCoverBackground(
		Painter &p,
		const QRect &clip,
		const QRect &bounds,
		const ResultCover &cover,
		bool adaptive,
		float64 opacity) {
	p.setOpacity(opacity);
	if (cover.noCover || !adaptive) {
		p.fillRect(clip, cover.bg);
		return;
	}
	auto gradient = QRadialGradient(
		bounds.topRight(),
		bounds.width() * 2.0);
	gradient.setColorAt(0, cover.bg);
	gradient.setColorAt(
		1,
		QColor::fromRgb(AdjustHsl(cover.bg.rgb(), 1.5f)));
	p.fillRect(bounds, gradient);
}

void PaintCoverImage(
		Painter &p,
		const ResultCover &cover,
		int size,
		float64 opacity) {
	if (cover.pix.isNull()) {
		return;
	}
	p.setOpacity(opacity);
	auto hq = PainterHighQualityEnabler(p);
	const auto coverRect = QRect(
		st::infoMusicButtonPadding.left(),
		st::infoMusicButtonPadding.top(),
		size,
		size);
	p.drawPixmap(coverRect, cover.pix);
}

struct Cover
{
	QPixmap pixToDraw;
	QPixmap pixToBg;
	bool noCover = false;
};

QPixmap MakeNoCoverImage(const QSize &size) {
	static QPixmap result;
	static auto resultTheme = Window::Theme::Background()->id();
	if (!result.isNull() && result.size() == size && resultTheme == Window::Theme::Background()->id()) {
		return result;
	}
	resultTheme = Window::Theme::Background()->id();

	auto image = QImage(size, QImage::Format_ARGB32);
	{
		auto p = Painter(&image);
		auto hq = PainterHighQualityEnabler(p);

		const auto bgColor = Window::Theme::IsNightMode()
								 ? st::windowBoldFg->c.darker()
								 : st::windowBoldFg->c.lighter();
		image.fill(bgColor);

		auto svgIcon = QSvgRenderer(u":/gui/icons/ayu/nocover.svg"_q);
		p.setPen(st::windowBoldFg->p);
		svgIcon.render(&p, QRect(0, 0, size.width(), size.height()));
	}
	const auto img = Image(std::move(image));
	result = img.pix(size, Images::PrepareArgs{.options = Images::Option::RoundSmall});
	return result;
}

int MusicButtonCoverSize() {
	const auto &font = st::infoMusicButtonTitle.style.font;
	return font->height + (st::normalFont->spacew / 2) + font->height;
}

ResultCover MakePlaceholderCover(std::optional<QColor> overrideBg) {
	const auto size = MusicButtonCoverSize();
	return {
		.pix = MakeNoCoverImage(QSize(size, size)),
		.bg = GetNoCoverBgColor(overrideBg),
		.noCover = true,
	};
}

bool SameCover(const ResultCover &a, const ResultCover &b) {
	return (a.noCover == b.noCover)
		&& (a.bg == b.bg)
		&& (a.pix.cacheKey() == b.pix.cacheKey());
}

} // namespace

Cover GetCurrentCover(
	const std::shared_ptr<Data::DocumentMedia> &dataMedia,
	const QSize &size) {
	if (!dataMedia) {
		return {
			.pixToDraw = MakeNoCoverImage(size),
			.pixToBg = MakeNoCoverImage(size),
			.noCover = true
		};
	}

	auto cover = QPixmap();
	const auto scaled = [&](not_null<Image*> image)
	{
		const auto aspectRatio = Qt::KeepAspectRatioByExpanding;
		const auto targetSize = size * style::DevicePixelRatio();
		return image->size().scaled(targetSize, aspectRatio);
	};
	const auto args = Images::PrepareArgs{
		.options = Images::Option::RoundSmall,
		.outer = size,
	};
	if (const auto normal = dataMedia->thumbnail()) {
		return {
			.pixToDraw = normal->pixNoCache(scaled(normal), args),
			.pixToBg = normal->pixNoCache(),
			.noCover = false
		};
	}

	return {
		.pixToDraw = MakeNoCoverImage(size),
		.pixToBg = MakeNoCoverImage(size),
		.noCover = true
	};
}

std::optional<QRgb> ExtractColorFromCover(const QPixmap &cover) {
	const auto palette = Ayu::Ui::Palette::from(cover).generate();

	const auto *swatch = palette.darkVibrantSwatch();
	if (!swatch) {
		swatch = palette.mutedSwatch();
	}
	if (!swatch) {
		swatch = palette.darkMutedSwatch();
	}
	if (!swatch) {
		swatch = palette.dominantSwatch();
	}

	if (!swatch) {
		return std::nullopt;
	}

	const auto extractedColor = swatch->rgb();

	constexpr auto whiteColor = qRgb(255, 255, 255);
	const auto contrast = Ayu::Ui::ColorUtils::calculateContrast(whiteColor, extractedColor);

	auto adjustedColor = extractedColor;
	if (contrast > 15.0f) {
		adjustedColor = AdjustHsl(extractedColor, 2.0f);
	} else if (contrast < 10.0f) {
		adjustedColor = AdjustHsl(extractedColor, 0.5f);
	}

	if (Ayu::Ui::ColorUtils::calculateContrast(whiteColor, adjustedColor) < 3.0f) {
		adjustedColor = BlendARGB(adjustedColor, qRgb(0, 0, 0), 0.3f);
	}

	return adjustedColor;
}

AyuMusicButton::AyuMusicButton(
	QWidget *parent,
	MusicButtonData data,
	std::optional<QColor> overrideBg,
	Fn<void()> handler)
	: RippleButton(parent, st::infoMusicButtonRipple)
	  , _performer(std::make_unique<Ui::FlatLabel>(
		  this,
		  data.performer,
		  st::infoMusicButtonPerformer))
	  , _title(std::make_unique<Ui::FlatLabel>(
		  this,
		  data.title,
		  st::infoMusicButtonTitle))
	  , _mediaView(data.mediaView)
	  , _overrideBg(overrideBg) {
	_performerText = data.performer;
	_titleText = data.title;
	_currentCover = MakePlaceholderCover(overrideBg);
	applyTextColors(*_currentCover);
	rpl::combine(
		_title->naturalWidthValue(),
		_performer->naturalWidthValue()
	) | rpl::on_next([=]
							 {
								 resizeToWidth(widthNoMargins());
							 },
							 lifetime());

	_title->setAttribute(Qt::WA_TransparentForMouseEvents);
	_performer->setAttribute(Qt::WA_TransparentForMouseEvents);

	downloadAndMakeCover(data.msgId);

	setClickedCallback(std::move(handler));
}

AyuMusicButton::~AyuMusicButton() = default;

void AyuMusicButton::updateData(MusicButtonData data) {
	_performer->setText(data.performer);
	_title->setText(data.title);
	_performerText = data.performer;
	_titleText = data.title;
	_mediaView = data.mediaView;
	_coverAnimation.stop();
	_previousCover.reset();
	_currentCover = MakePlaceholderCover(_overrideBg);
	applyTextColors(*_currentCover);
	update();
	downloadAndMakeCover(data.msgId);

	resizeToWidth(widthNoMargins());
}

void AyuMusicButton::downloadAndMakeCover(FullMsgId msgId) {
	const auto requestId = ++_coverRequestId;
	const auto mediaView = _mediaView;
	if (mediaView
		&& mediaView->owner()->isSongWithCover()
		&& !mediaView->thumbnail()) {
		const auto settings = &mediaView->owner()->session().settings().autoDownload();
		// Data::AutoDownload::Type::Music always returns false
		if (settings->shouldDownload(
				Data::AutoDownload::Source::User,
				Data::AutoDownload::Type::File,
				mediaView->owner()->size)) {
			mediaView->thumbnailWanted(Data::FileOrigin(msgId));
			mediaView->owner()->owner().session().downloaderTaskFinished(
			) | rpl::take_while([=]
			{
				if (requestId != _coverRequestId) {
					return false;
				}
				if (mediaView->thumbnail()) {
					makeCover(requestId);
				}
				return !mediaView->thumbnail();
			}) | rpl::start(lifetime());
			return;
		}
	}

	makeCover(requestId);
}

void AyuMusicButton::makeCover(uint64 requestId) {
	const auto weak = base::make_weak(this);
	crl::async([
		weak,
		mediaView = _mediaView,
		performerText = _performerText,
		titleText = _titleText,
		overrideBg = _overrideBg,
		requestId
	]() {
		const auto &settings = AyuSettings::getInstance();
		const auto size = MusicButtonCoverSize();

		auto cover = GetCurrentCover(mediaView, QSize(size, size));

		if (cover.noCover) {
			const auto pix = Ayu::Ui::Itunes::FetchCover(
				performerText,
				titleText,
				size);
			if (!pix.isNull()) {
				const auto img = Image(pix.toImage());
				const auto args = Images::PrepareArgs{
					.options = Images::Option::RoundSmall,
					.outer = QSize(size, size),
				};
				cover.pixToDraw = img.pix(QSize(size, size), args);
				cover.pixToBg = pix;
				cover.noCover = false;
			}
		}

		QColor bgColor;
		if (cover.noCover || !settings.adaptiveCoverColor()) {
			bgColor = GetNoCoverBgColor(overrideBg);
		} else {
			if (const auto extractedColor = ExtractColorFromCover(
					cover.pixToBg)) {
				bgColor = QColor::fromRgb(*extractedColor);
			} else {
				// example: fully black image
				cover.noCover = true;
				bgColor = GetNoCoverBgColor(overrideBg);
			}
		}

		crl::on_main([
			weak,
			cover = std::move(cover),
			bgColor,
			requestId
		]() mutable {
			const auto strong = weak.get();
			if (!strong || requestId != strong->_coverRequestId) {
				return;
			}

			strong->applyCover({
				.pix = cover.pixToDraw,
				.bg = bgColor,
				.noCover = cover.noCover,
			});

			strong->_onReady.fire({});
		});
	});
}

void AyuMusicButton::applyCover(ResultCover cover) {
	if (_currentCover && SameCover(*_currentCover, cover)) {
		_previousCover.reset();
		_currentCover = std::move(cover);
		applyTextColors(*_currentCover);
		update();
		_title->update();
		_performer->update();
		return;
	}
	_previousCover = std::move(_currentCover);
	_currentCover = std::move(cover);
	applyTextColors(*_currentCover);
	_coverAnimation.stop();
	_coverAnimation.start(
		[=] { update(); },
		0.,
		1.,
		kCoverTransitionDuration);
	_title->update();
	_performer->update();
}

void AyuMusicButton::applyTextColors(const ResultCover &cover) {
	const auto &settings = AyuSettings::getInstance();
	if (!cover.noCover
		&& settings.adaptiveCoverColor()
		&& !cover.pix.isNull()) {
		_title->setTextColorOverride(Qt::white);
		_performer->setTextColorOverride(performerColor);
	} else {
		const auto color = _overrideBg
			? st::groupCallMembersFg->c
			: st::windowBoldFg->c;
		_title->setTextColorOverride(color);
		_performer->setTextColorOverride(color);
	}
}

void AyuMusicButton::paintEvent(QPaintEvent *e) {
	if (!_currentCover) {
		return;
	}

	auto p = Painter(this);

	const auto size = MusicButtonCoverSize();

	const auto &settings = AyuSettings::getInstance();
	const auto adaptive = settings.adaptiveCoverColor();

	const auto progress = _coverAnimation.value(1.);
	if (_previousCover && progress < 1.) {
		PaintCoverBackground(
			p,
			e->rect(),
			rect(),
			*_previousCover,
			adaptive,
			1.);
		PaintCoverBackground(
			p,
			e->rect(),
			rect(),
			*_currentCover,
			adaptive,
			progress);
	} else {
		_previousCover.reset();
		PaintCoverBackground(
			p,
			e->rect(),
			rect(),
			*_currentCover,
			adaptive,
			1.);
	}
	p.setOpacity(1.);
	if (_currentCover->noCover || !adaptive) {
		paintRipple(p, QPoint());
	}
	if (_previousCover && progress < 1.) {
		PaintCoverImage(p, *_previousCover, size, 1.);
		PaintCoverImage(p, *_currentCover, size, progress);
	} else {
		PaintCoverImage(p, *_currentCover, size, 1.);
	}
	p.setOpacity(1.);
}

int AyuMusicButton::resizeGetHeight(int newWidth) {
	const auto padding = st::infoMusicButtonPadding;
	const auto &font = st::infoMusicButtonTitle.style.font;

	const auto top = padding.top();
	const auto lineSkip = st::normalFont->spacew / 2;
	const auto textSkip = st::infoMusicButtonTextSkip;

	const auto coverSize = MusicButtonCoverSize();

	const auto available = std::max(
		newWidth
			- padding.left()
			- padding.right()
			- coverSize
			- textSkip,
		0);
	_title->resizeToWidth(std::min(_title->naturalWidth(), available));
	_title->moveToLeft(padding.left() + coverSize + textSkip, top);
	_performer->resizeToWidth(std::min(_performer->naturalWidth(), available));
	_performer->moveToLeft(
		padding.left() + coverSize + textSkip,
		top + font->height + lineSkip);

	return padding.top() + coverSize + padding.bottom();
}

} // namespace Info::Profile
