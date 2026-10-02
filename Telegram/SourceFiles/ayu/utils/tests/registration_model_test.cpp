#include "registration_model.h"
#include <cassert>
#include <limits>

int main() {
	using Ayu::RegistrationModel::Sample;
	using Ayu::RegistrationModel::Estimate;
	auto samples = std::vector<Sample>{
		{980000000, 1000}, {990000000, 1010}, {1000000000, 1020},
		{1010000000, 1030}, {1020000000, 1040},
	};
	assert(Estimate(1005000000, samples, 1, 2000) == 1025);
	samples.push_back({1007000000, 1800});
	assert(Estimate(1005000000, samples, 1, 2000) == 1025);
	assert(!Estimate(1030000000, samples, 1, 2000));
	assert(!Estimate(1005000000, samples, 1, 1024));
	assert(!Estimate(5000000000, samples, 1, 2000));
	assert(!Estimate(std::numeric_limits<std::uint64_t>::max(), samples, 1, 2000));
	samples.resize(4);
	assert(!Estimate(1005000000, samples, 1, 2000));
	samples = {{980000000,1040}, {990000000,1030}, {1000000000,1020},
			   {1010000000,1010}, {1020000000,1000}};
	assert(!Estimate(1005000000, samples, 1, 2000));
	for (auto &sample : samples) sample.day = 1000;
	assert(Estimate(1005000000, samples, 1, 2000) == 1000);
	samples = {{980000000,100}, {990000000,1000}, {1000000000,800},
			   {1010000000,200}, {1020000000,1200}};
	assert(!Estimate(1005000000, samples, 1, 2000));
}
