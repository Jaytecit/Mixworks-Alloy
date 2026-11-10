#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace mixworks
{
// Consumed by the visual worker. The callback only publishes PCM and its clock.
class VisualKickDetector
{
public:
    void push(float left, float right, double time, double sampleRate, std::uint64_t revision)
    {
        if (time < 0.)
        {
            if (rate > 0.) reset();
            return;
        }
        if (rate != sampleRate || generation != revision || std::abs(time - nextTime) > .5 / sampleRate)
        {
            reset();
            rate = sampleRate;
            generation = revision;
            hop = std::max(1, (int) std::lround(rate * .010));
            for (auto& channel : filters)
            {
                channel[0].tune(rate, 64., 1.05);
                channel[1].tune(rate, 155., 1.25);
                channel[2].tune(rate, 900., .75);
            }
        }
        nextTime = time + 1. / rate;
        if (samples == 0) windowStart = time;
        for (int band = 0; band < 3; ++band)
        {
            const auto l = filters[0][band].process(left);
            const auto r = filters[1][band].process(right);
            energy[band] += (l * l + r * r) * .5;
        }
        if (++samples == hop) finishWindow();
    }

    const std::array<double, 3>& events() const { return recent; }
    std::uint64_t revision() const { return generation; }

private:
    struct Band
    {
        double b = 0., a1 = 0., a2 = 0., z1 = 0., z2 = 0.;
        void tune(double rate, double centre, double q)
        {
            const double w = 6.283185307179586 * centre / rate;
            const double alpha = std::sin(w) / (2. * q);
            b = alpha / (1. + alpha);
            a1 = -2. * std::cos(w) / (1. + alpha);
            a2 = (1. - alpha) / (1. + alpha);
        }
        double process(float value)
        {
            const double x = std::isfinite(value) ? std::clamp(value, -64.f, 64.f) : 0.;
            const double y = b * x + z1;
            z1 = z2 - a1 * y;
            z2 = -b * x - a2 * y;
            return y;
        }
    };
    using Bands = std::array<double, 3>;
    static constexpr int observationHops = 5;
    std::array<std::array<Band, 3>, 2> filters {};
    std::array<Bands, observationHops> observations {};
    Bands energy {}, floor {}, reference {};
    int samples = 0, hop = 1, priming = 6, observed = 0;
    double rate = 0., windowStart = 0., nextTime = 0., attackTime = 0.;
    std::uint64_t generation = 0;
    std::array<double, 3> recent {-1., -1., -1.};

    void reset() { *this = VisualKickDetector{}; }

    void finishWindow()
    {
        Bands rms {};
        for (int band = 0; band < 3; ++band) rms[band] = std::sqrt(energy[band] / hop);
        energy = {};
        samples = 0;

        if (priming > 0)
        {
            --priming;
            floor = rms;
            return;
        }

        const bool ready = recent.back() < 0. || windowStart - recent.back() >= .145;
        if (observed == 0 && ready
            && rms[0] > floor[0] * 1.20 + .008 && rms[1] > floor[1] * 1.28 + .010)
        {
            attackTime = windowStart;
            reference = floor;
            observations[observed++] = excess(rms);
        }
        else if (observed > 0)
        {
            observations[observed++] = excess(rms);
            if (observed == observationHops)
            {
                if (kickShape()) recent = {recent[1], recent[2], attackTime};
                observed = 0;
            }
        }

        for (int band = 0; band < 3; ++band)
            floor[band] += (rms[band] - floor[band]) * (rms[band] > floor[band] ? .10 : .19);
    }

    Bands excess(const Bands& rms) const
    {
        Bands result {};
        for (int band = 0; band < 3; ++band)
            result[band] = std::sqrt(std::max(0., rms[band] * rms[band] - reference[band] * reference[band]));
        return result;
    }

    bool kickShape() const
    {
        const double headLow = std::max(observations[0][0], observations[1][0]);
        const double headBody = std::max(observations[0][1], observations[1][1]);
        const double headAir = std::max(observations[0][2], observations[1][2]);
        const double peakLow = std::max({headLow, observations[2][0], observations[3][0]});
        const double tailLow = (observations[3][0] + observations[4][0]) * .5;
        const double tailBody = (observations[3][1] + observations[4][1]) * .5;
        const double fall = headBody / std::max(.014, headLow) - tailBody / std::max(.014, tailLow);
        // A kick needs a short low tail and a downward shift through the
        // upper bass. A rising sub envelope or a bright attack alone is weak evidence.
        return peakLow > .025 && headBody > .020 && headBody > peakLow * .40
            && headBody > tailBody * 1.55 + .006 && fall > .25
            && tailLow < peakLow * .98 && headAir < headBody * 2.5 + peakLow;
    }
};
}
