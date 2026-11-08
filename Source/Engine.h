#pragma once
// Rock Icons Drive: plain C++ DSP engine (no JUCE), so it can be unit-tested and rendered offline.
//
// Each "model" is an approximation of the character of the rig behind a classic rock tone:
//   pre low-cut / treble-booster shelf -> drive gain (with amp-style sag) -> 1..3 clipping stages
//   (with coupling filters between them) -> amp-style voicing (bass / mid / treble) -> speaker-ish low-pass.
// These are voicings built from generic waveshapers, NOT circuit-accurate models of the original gear.
// Run it at an oversampled rate (the plugin uses 4x) to keep aliasing down.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace rid
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToLin (float db) { return std::pow (10.0f, db / 20.0f); }

// ------------------------------------------------------------------ filters (RBJ cookbook)

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }

    void lowpass (double sr, double f, double q)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double sr, double f, double q)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowshelf (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double beta = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c),
             A * ((A + 1) - (A - 1) * c - beta), (A + 1) + (A - 1) * c + beta,
             -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
    }
    void highshelf (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double beta = 2.0 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c),
             A * ((A + 1) + (A - 1) * c - beta), (A + 1) - (A - 1) * c + beta,
             2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
    }

private:
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = (float) (nb0 / a0);
        b1 = (float) (nb1 / a0);
        b2 = (float) (nb2 / a0);
        a1 = (float) (na1 / a0);
        a2 = (float) (na2 / a0);
    }
};

// One-pole DC / coupling high-pass: y[n] = x[n] - x[n-1] + R * y[n-1]
struct Coupler
{
    float x1 = 0.0f, y1 = 0.0f;
    float process (float x, float R)
    {
        const float y = x - x1 + R * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.0f; }
};

// ------------------------------------------------------------------ waveshapers

enum class Clip { Tube, Fuzz, Hard, Tape };

inline float fuzzCurve (float v)
{
    // Asymmetric exponential soft clip: positive side saturates at +1, negative at -0.6 (steeper).
    return v >= 0.0f ? 1.0f - std::exp (-v) : -0.6f * (1.0f - std::exp (v * 1.8f));
}

inline float tapeCurve (float v) { return v / (1.0f + std::fabs (v)); }

// bias adds asymmetry (even harmonics). The static offset is removed so silence stays silent.
inline float shape (Clip c, float x, float bias)
{
    switch (c)
    {
        case Clip::Tube: return std::tanh (x + bias) - std::tanh (bias);
        case Clip::Fuzz: return fuzzCurve (x + bias) - fuzzCurve (bias);
        case Clip::Tape: return tapeCurve (x + bias) - tapeCurve (bias);
        case Clip::Hard:
        {
            float a = x * x;
            a *= a;
            return x / std::sqrt (std::sqrt (1.0f + a));
        }
    }
    return x;
}

// ------------------------------------------------------------------ models

struct Model
{
    const char* name;
    const char* note;
    Clip clip;      // first clipping stage
    Clip clipPost;  // later stages
    int stages;     // 1..3
    float hpHz;     // pre-clip low cut (tightness)
    float preShelfHz, preShelfDb; // pre-clip high shelf (treble booster)
    float gainLoDb, gainHiDb;     // drive knob range
    float interDb;  // gain between stages
    float bias;     // asymmetry
    float sag;      // 0..1, amp-style compression of the drive
    float bassDb;   // post voicing: low shelf @120 Hz
    float midDb, midHz, midQ;
    float trebleDb; // post voicing: high shelf @3.5 kHz
    float cabLpHz;  // speaker-ish low-pass
    float outDb;    // loudness match
};

// Descriptions are from memory and approximate; they say what the voicing is aiming at.
inline constexpr Model kModels[] = {
    { "Hendrix - Fuzz into Plexi", "Fuzz Face-style fuzz into a cranked Marshall: thick, singing, woolly.",
      Clip::Fuzz, Clip::Tube, 2, 90, 0, 0, 14, 44, 8, 0.25f, 0.25f, -1, 3, 1000, 1.0f, 2, 5200, -18.1f },
    { "Page - Plexi crunch", "Les Paul into a cranked Plexi: open, mid-forward crunch.",
      Clip::Tube, Clip::Tube, 2, 80, 0, 0, 10, 38, 6, 0.10f, 0.30f, 1, 2.5f, 1200, 1.0f, 1.5f, 5500, -17.6f },
    { "Van Halen - Brown sound", "Hot-rodded Plexi: tight, saturated, bright with a vocal mid.",
      Clip::Tube, Clip::Tube, 3, 110, 0, 0, 16, 48, 8, 0.05f, 0.20f, 0, 3, 1500, 0.9f, 3, 6000, -19.3f },
    { "Angus - Cranked Plexi", "Raw, bright, medium-gain Marshall: loud amp, not much compression.",
      Clip::Tube, Clip::Tube, 2, 100, 2000, 3, 12, 40, 4, 0.08f, 0.15f, -1, 2, 1800, 1.0f, 3.5f, 6200, -17.2f },
    { "Richards - Maestro fuzz", "Early silicon fuzz: thin, raspy and aggressive, with the lows rolled off.",
      Clip::Fuzz, Clip::Fuzz, 1, 200, 0, 0, 18, 42, 0, 0.30f, 0.10f, -3, 4, 1300, 1.2f, 0, 4500, -16.1f },
    { "Slash - Cranked JCM800", "Les Paul into a hot Marshall: big 800 Hz mid hump, presence on top.",
      Clip::Tube, Clip::Tube, 3, 90, 0, 0, 14, 46, 7, 0.10f, 0.25f, 1.5f, 4, 800, 1.0f, 2, 5600, -18.9f },
    { "Iommi - Treble booster stack", "Treble booster into a dark, heavy stack: big lows, rolled-off top.",
      Clip::Tube, Clip::Tube, 2, 70, 1500, 10, 10, 36, 6, 0.15f, 0.30f, 3, 0, 700, 1.0f, -3, 4200, -14.8f },
    { "May - Treble booster AC30", "Treble booster into a cranked AC30: nasal, singing midrange.",
      Clip::Tube, Clip::Tube, 2, 120, 1200, 12, 8, 32, 5, 0.20f, 0.35f, -2, 5, 1600, 1.5f, 1, 5800, -17.9f },
    { "Gilmour - Big Muff sustain", "Muff-style fuzz: long creamy sustain with scooped mids.",
      Clip::Hard, Clip::Hard, 2, 60, 0, 0, 18, 50, 10, 0.0f, 0.10f, 3, -8, 900, 0.6f, -1, 5000, -14.2f },
    { "Young - Ragged tweed", "Cranked tweed Deluxe: saggy, uneven, falling-apart crunch.",
      Clip::Tube, Clip::Tube, 2, 70, 0, 0, 14, 40, 5, 0.30f, 0.60f, 2, 1, 600, 1.0f, -2, 4800, -16.3f },
    { "Nebraska - Lo-fi tape", "Warm bedroom-cassette saturation: soft squash, dulled top. Not a heavy distortion.",
      Clip::Tape, Clip::Tape, 1, 90, 0, 0, 0, 14, 0, 0.10f, 0.0f, 1, 1, 1000, 1.0f, -4, 4500, -1.5f },
};
inline constexpr int kNumModels = (int) (sizeof (kModels) / sizeof (kModels[0]));

// ------------------------------------------------------------------ engine

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        atk = 1.0f - std::exp ((float) (-1.0 / (0.002 * sr)));
        rel = 1.0f - std::exp ((float) (-1.0 / (0.120 * sr)));
        gainSmooth = 1.0f - std::exp ((float) (-1.0 / (0.010 * sr)));
        coupleR = (float) std::exp (-2.0 * kPi * 50.0 / sr);
        dcR = (float) std::exp (-2.0 * kPi * 20.0 / sr);
        reset();
        configure (1, 0.5f, 0.0f, true);
        gainCur = gainTarget;
    }

    void reset()
    {
        hp.reset(); preShelf.reset(); bass.reset(); mid.reset(); tone.reset();
        cab1.reset(); cab2.reset();
        for (auto& c : coup) c.reset();
        dc.reset();
        env = 0.0f;
    }

    // Cheap enough to call once per block. Does not reset filter state.
    void configure (int modelIndex, float drive01, float toneM1to1, bool cabOn)
    {
        m = &kModels[std::clamp (modelIndex, 0, kNumModels - 1)];
        drive01 = std::clamp (drive01, 0.0f, 1.0f);
        toneM1to1 = std::clamp (toneM1to1, -1.0f, 1.0f);

        hp.highpass (sr, m->hpHz, 0.707);
        hasPre = m->preShelfDb != 0.0f;
        if (hasPre)
            preShelf.highshelf (sr, m->preShelfHz, 0.707, m->preShelfDb);

        gainTarget = dbToLin (m->gainLoDb + drive01 * (m->gainHiDb - m->gainLoDb));
        interGain = dbToLin (m->interDb);

        bass.lowshelf (sr, 120.0, 0.707, m->bassDb);
        mid.peak (sr, m->midHz, m->midQ, m->midDb);
        tone.highshelf (sr, 3500.0, 0.707, toneM1to1 * 6.0f);

        cab = cabOn;
        cab1.lowpass (sr, m->cabLpHz, 0.6);
        cab2.lowpass (sr, m->cabLpHz, 0.8);
        outGain = dbToLin (m->outDb);
    }

    float process (float x)
    {
        x = hp.process (x);
        if (hasPre)
            x = preShelf.process (x);

        const float a = std::fabs (x);
        env += (a > env ? atk : rel) * (a - env);
        gainCur += gainSmooth * (gainTarget - gainCur);
        const float sagGain = 1.0f / (1.0f + m->sag * env * 3.0f);

        float v = x * gainCur * sagGain;
        for (int s = 0; s < m->stages; ++s)
        {
            v = shape (s == 0 ? m->clip : m->clipPost, v, m->bias);
            if (s < m->stages - 1)
                v = coup[(size_t) s].process (v, coupleR) * interGain;
        }

        v = dc.process (v, dcR);
        v = bass.process (v);
        v = mid.process (v);
        v = tone.process (v);
        if (cab)
        {
            v = cab1.process (v);
            v = cab2.process (v);
        }
        return v * outGain;
    }

private:
    double sr = 44100.0;
    const Model* m = &kModels[1];
    Biquad hp, preShelf, bass, mid, tone, cab1, cab2;
    std::array<Coupler, 2> coup;
    Coupler dc;
    bool hasPre = false, cab = true;
    float env = 0.0f, atk = 0.01f, rel = 0.0001f;
    float gainCur = 1.0f, gainTarget = 1.0f, gainSmooth = 0.001f;
    float interGain = 1.0f, outGain = 1.0f, coupleR = 0.99f, dcR = 0.999f;
};
} // namespace rid
