#pragma once

#include <algorithm>
#include <cmath>

namespace voiceforge {

class PeakLimiter {
public:
    explicit PeakLimiter(double sampleRate) noexcept
        : release_(-std::expm1(-1.0 / (0.080 * sampleRate))) {}

    void reset() noexcept { gain_ = 1.0; }

    double process(double peak, double ceiling) noexcept {
        const double target = peak > ceiling ? ceiling / peak : 1.0;
        if (target < gain_) {
            gain_ = target;
        } else {
            gain_ += release_ * (target - gain_);
            if (target == 1.0 && gain_ > 1.0 - 1e-6)
                gain_ = 1.0;
        }
        return gain_;
    }

private:
    double release_;
    double gain_ = 1.0;
};

} // namespace voiceforge
