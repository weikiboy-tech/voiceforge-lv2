#pragma once

#include "dsp/core/parameter_math.hpp"
#include "dsp/mastering/loudness.hpp"

#include <array>
#include <numbers>

namespace voiceforge {

struct VoiceReport {
    float progress = 0.0f, peak = -120.0f, rms = -120.0f, floor = -120.0f;
    float range = 0.0f, speech = 0.0f, low = 0.0f, high = 0.0f, sibilance = 0.0f;
    float highPass = 0.0f, compressor = -20.0f, deEsser = 6500.0f, noise = 0.0f;
    float loudness = -120.0f;
    bool clipped = false;
};

class VoiceAnalysis {
public:
    explicit VoiceAnalysis(double rate) noexcept
        : limit_(static_cast<unsigned>(rate * 8)),
          windowSize_(static_cast<unsigned>(std::round(rate / 10.0))), loudness_(rate, false) {
        for (unsigned i = 0; i < steps_.size(); ++i) {
            const std::array<double, 3> frequencies{160.0, 4300.0, 6500.0};
            steps_[i] = -std::expm1(-2.0 * std::numbers::pi * frequencies[i] / rate);
        }
    }

    void command(bool start, bool reset) noexcept {
        if (reset && !reset_)
            clear(false);
        else if (start && !start_)
            clear(true);
        start_ = start;
        reset_ = reset;
    }

    void clear(bool active = false) noexcept {
        loudness_.reset();
        report_ = {};
        bands_ = {};
        filters_ = {};
        count_ = windowCount_ = vocal_ = 0;
        power_ = peak_ = windowPower_ = quietPower_ = 0.0;
        loudWindow_ = lowWindow_ = 0.0;
        quietCount_ = 0;
        active_ = active;
    }

    void process(const std::array<double, 2>& frame, unsigned channels, bool voice) noexcept {
        if (!active_)
            return;
        loudness_.process({static_cast<float>(frame[0]), static_cast<float>(frame[1])}, channels);
        double channelPower = 0.0;
        for (unsigned ch = 0; ch < channels; ++ch) {
            const double x = frame[ch];
            peak_ = std::max(peak_, std::abs(x));
            channelPower += x * x;
            auto& state = filters_[ch];
            for (unsigned k = 0; k < state.size(); ++k)
                state[k] += steps_[k] * (x - state[k]);
            bands_[0] += state[0] * state[0];
            bands_[1] += (x - state[1]) * (x - state[1]);
            bands_[2] += (x - state[2]) * (x - state[2]);
        }
        power_ += channelPower;
        windowPower_ += channelPower / channels;
        ++windowCount_;
        if (voice) {
            ++vocal_;
        } else {
            quietPower_ += channelPower / channels;
            ++quietCount_;
        }
        ++count_;
        if (windowCount_ == windowSize_ || count_ == limit_) {
            const double average = windowPower_ / windowCount_;
            loudWindow_ = std::max(loudWindow_, average);
            if (average > 1e-5 && (lowWindow_ == 0.0 || average < lowWindow_))
                lowWindow_ = average;
            report_.progress = static_cast<float>(count_) / limit_;
            windowCount_ = 0;
            windowPower_ = 0.0;
        }
        if (count_ == limit_)
            complete(channels);
    }

    const VoiceReport& report() const noexcept { return report_; }

private:
    void complete(unsigned channels) noexcept {
        active_ = false;
        const double full = std::max(power_, 1e-20);
        report_.peak = static_cast<float>(std::max(-120.0, decibels(peak_)));
        report_.clipped = peak_ >= 1.0;
        report_.rms = static_cast<float>(std::max(-120.0, decibels(std::sqrt(power_ / (count_ * channels)))));
        report_.speech = static_cast<float>(vocal_) / count_;
        report_.loudness = loudness_.integrated();
        if (quietCount_ > windowSize_ && quietPower_ > 1e-16)
            report_.floor = static_cast<float>(std::max(-120.0, decibels(std::sqrt(quietPower_ / quietCount_))));
        if (lowWindow_ > 0.0)
            report_.range = static_cast<float>(10.0 * std::log10(loudWindow_ / lowWindow_));
        report_.low = static_cast<float>(std::clamp(bands_[0] / full, 0.0, 1.0));
        report_.high = static_cast<float>(std::clamp(bands_[1] / full, 0.0, 1.0));
        report_.sibilance = static_cast<float>(std::clamp(bands_[2] / full, 0.0, 1.0));
        if (report_.speech < 0.05f)
            return;
        report_.highPass = report_.low > 0.48f ? 95.0f : 65.0f;
        report_.compressor = std::clamp(report_.rms + 5.0f, -32.0f, -12.0f);
        report_.deEsser = report_.sibilance > 0.08f ? 7400.0f : 5700.0f;
        report_.noise = report_.floor > -55.0f ? 6.0f : 0.0f;
    }

    unsigned limit_, windowSize_;
    std::array<double, 3> steps_{}, bands_{};
    std::array<std::array<double, 3>, 2> filters_{};
    unsigned count_ = 0, windowCount_ = 0, vocal_ = 0, quietCount_ = 0;
    double power_ = 0.0, peak_ = 0.0, windowPower_ = 0.0, quietPower_ = 0.0;
    double loudWindow_ = 0.0, lowWindow_ = 0.0;
    bool active_ = false, start_ = false, reset_ = false;
    Loudness loudness_;
    VoiceReport report_;
};

} // namespace voiceforge
