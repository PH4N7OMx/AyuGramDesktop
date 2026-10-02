#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Ayu::RegistrationModel {

struct Sample {
	std::uint64_t id;
	std::int64_t day;
};

inline double Median(std::vector<double> values) {
	std::sort(values.begin(), values.end());
	const auto middle = values.size() / 2;
	return (values.size() % 2) ? values[middle]
		: (values[middle - 1] + values[middle]) / 2.;
}

// Robust local regression over server-confirmed months, not inferred observations.
inline std::optional<std::int64_t> Estimate(
		std::uint64_t id,
		std::vector<Sample> samples,
		std::int64_t earliestDay,
		std::int64_t latestDay) {
	const auto distance = [id](std::uint64_t value) {
		return (value > id) ? value - id : id - value;
	};
	const auto radius = std::max(std::uint64_t(50000000), id / 10);
	samples.erase(std::remove_if(samples.begin(), samples.end(), [&](const auto &sample) {
		return distance(sample.id) > radius
			|| sample.day < earliestDay || sample.day > latestDay;
	}), samples.end());
	std::sort(samples.begin(), samples.end(), [&](const auto &a, const auto &b) {
		return distance(a.id) < distance(b.id);
	});
	if (samples.size() > 16) samples.resize(16);
	if (samples.size() < 5) return {};
	std::sort(samples.begin(), samples.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
	// Do not extrapolate a global trend from a few locally observed accounts.
	if (samples.front().id >= id || samples.back().id <= id) return {};
	auto slopes = std::vector<double>();
	for (auto i = std::size_t(0); i < samples.size(); ++i) {
		for (auto j = i + 1; j < samples.size(); ++j) {
			if (samples[j].id != samples[i].id) {
				slopes.push_back((double(samples[j].day) - samples[i].day)
					/ double(samples[j].id - samples[i].id));
			}
		}
	}
	if (slopes.empty()) return {};
	const auto slope = Median(std::move(slopes));
	if (slope < 0.) return {};
	auto predictions = std::vector<double>();
	for (const auto &sample : samples) {
		const auto offset = (id >= sample.id)
			? double(id - sample.id) : -double(sample.id - id);
		predictions.push_back(sample.day + slope * offset);
	}
	const auto prediction = Median(predictions);
	auto deviations = std::vector<double>();
	for (const auto day : predictions) deviations.push_back(std::abs(day - prediction));
	if (Median(std::move(deviations)) > 62.
		|| !std::isfinite(prediction)
		|| prediction < earliestDay || prediction > latestDay) return {};
	return std::llround(prediction);
}

} // namespace Ayu::RegistrationModel
