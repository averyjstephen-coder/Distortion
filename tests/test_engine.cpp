#include <cmath>
#include <cstdio>
#include <vector>

#include "Engine.h"
#include "Synth.h"

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (! (cond)) { std::printf ("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } \
    } while (0)

static const double kSr = 176400.0;
static const double kTwoPi = 6.283185307179586;

static double harmonicMag (const std::vector<float>& y, double f, int h)
{
    double re = 0, im = 0;
    for (size_t n = 0; n < y.size(); ++n)
    {
        const double ph = kTwoPi * f * h * (double) n / kSr;
        re += y[n] * std::cos (ph);
        im += y[n] * std::sin (ph);
    }
    return 2.0 * std::hypot (re, im) / (double) y.size();
}

static std::vector<float> sine (double f, double amp, double seconds)
{
    std::vector<float> x ((size_t) (seconds * kSr));
    for (size_t n = 0; n < x.size(); ++n)
        x[n] = (float) (amp * std::sin (kTwoPi * f * (double) n / kSr));
    return x;
}

static double rms (const std::vector<float>& y, size_t from = 0)
{
    double s = 0;
    for (size_t n = from; n < y.size(); ++n)
        s += (double) y[n] * y[n];
    return std::sqrt (s / (double) (y.size() - from));
}

static double gainDbAt (rid::Biquad b, double f)
{
    auto x = sine (f, 1.0, 0.5);
    std::vector<float> y (x.size());
    for (size_t n = 0; n < x.size(); ++n)
        y[n] = b.process (x[n]);
    return 20.0 * std::log10 (rms (y, y.size() / 2) / rms (x, x.size() / 2));
}

int main()
{
    // 1. Filters hit their targets
    {
        rid::Biquad b;
        b.highshelf (kSr, 2500.0, 0.707, 6.0);
        CHECK (std::fabs (gainDbAt (b, 15000.0) - 6.0) < 0.4);
        CHECK (std::fabs (gainDbAt (b, 100.0)) < 0.3);

        rid::Biquad lp;
        lp.lowpass (kSr, 5000.0, 0.707);
        CHECK (std::fabs (gainDbAt (lp, 5000.0) + 3.0) < 0.3);
        CHECK (std::fabs (gainDbAt (lp, 200.0)) < 0.1);

        rid::Biquad pk;
        pk.peak (kSr, 1000.0, 1.0, 5.0);
        CHECK (std::fabs (gainDbAt (pk, 1000.0) - 5.0) < 0.2);
    }

    // 2. Bias creates even harmonics; symmetric shapers do not
    {
        auto x = sine (220.0, 0.8, 0.5); // 110 whole cycles
        auto run = [&] (rid::Clip c, float bias)
        {
            std::vector<float> y (x.size());
            for (size_t n = 0; n < x.size(); ++n)
                y[n] = rid::shape (c, x[n] * 3.0f, bias);
            return harmonicMag (y, 220.0, 2) / harmonicMag (y, 220.0, 1);
        };
        CHECK (run (rid::Clip::Tube, 0.0f) < 1e-3);
        CHECK (run (rid::Clip::Tube, 0.3f) > 1e-2);
        CHECK (run (rid::Clip::Hard, 0.0f) < 1e-3);
        CHECK (run (rid::Clip::Fuzz, 0.0f) > 1e-2);
    }

    // 3. Every model, extreme settings: finite and bounded; silence stays silent
    {
        const auto chord = synth::powerChord (kSr, 1.0);
        for (int m = 0; m < rid::kNumModels; ++m)
            for (float drive : { 0.0f, 1.0f })
                for (float tone : { -1.0f, 1.0f })
                    for (bool cab : { true, false })
                    {
                        rid::Engine e;
                        e.prepare (kSr);
                        e.configure (m, drive, tone, cab);
                        e.reset();
                        float peak = 0;
                        bool finite = true;
                        for (float s : chord)
                        {
                            const float y = e.process (s);
                            finite = finite && std::isfinite (y);
                            peak = std::max (peak, std::fabs (y));
                        }
                        if (! finite || peak > 4.0f)
                            std::printf ("  model %d drive %.0f tone %.0f cab %d -> finite %d peak %.2f\n",
                                         m, drive, tone, (int) cab, (int) finite, peak);
                        CHECK (finite);
                        CHECK (peak < 4.0f);

                        e.reset();
                        float quiet = 0;
                        for (int n = 0; n < 20000; ++n)
                            quiet = std::max (quiet, std::fabs (e.process (0.0f)));
                        CHECK (quiet < 1e-6f);
                    }
    }

    // 4. More drive means more distortion (harmonic content relative to the fundamental)
    {
        auto thd = [&] (int model, float drive)
        {
            rid::Engine e;
            e.prepare (kSr);
            e.configure (model, drive, 0.0f, true);
            auto x = sine (220.0, 0.3, 1.0);
            std::vector<float> y (x.size());
            for (size_t n = 0; n < x.size(); ++n)
                y[n] = e.process (x[n]);
            y.erase (y.begin(), y.begin() + (long) (y.size() / 2)); // settled half, whole cycles
            double h = 0;
            for (int k = 2; k <= 8; ++k)
                h += std::pow (harmonicMag (y, 220.0, k), 2.0);
            return std::sqrt (h) / harmonicMag (y, 220.0, 1);
        };
        for (int m : { 1, 2, 5, 8 })
            CHECK (thd (m, 1.0f) > thd (m, 0.1f));
    }

    // 5. Tone knob moves the top end
    {
        const auto chord = synth::powerChord (kSr, 1.0);
        auto hf = [&] (float tone)
        {
            rid::Engine e;
            e.prepare (kSr);
            e.configure (1, 0.6f, tone, true);
            rid::Biquad hp;
            hp.highpass (kSr, 3000.0, 0.707);
            std::vector<float> y (chord.size());
            for (size_t n = 0; n < chord.size(); ++n)
                y[n] = hp.process (e.process (chord[n]));
            return rms (y);
        };
        CHECK (hf (1.0f) > 1.5 * hf (-1.0f));
    }

    if (failures == 0)
        std::printf ("All engine tests passed.\n");
    return failures == 0 ? 0 : 1;
}
