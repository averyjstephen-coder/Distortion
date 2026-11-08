// Renders every model on a synthesized power chord into one WAV, and reports loudness for calibration.
//   g++ -O2 -std=c++17 -ISource -Itests tools/render_demo.cpp -o render_demo && ./render_demo demo.wav
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Engine.h"
#include "Synth.h"

int main (int argc, char** argv)
{
    const std::string outPath = argc > 1 ? argv[1] : "demo_all_models.wav";
    const double sr = 176400.0, seconds = 3.2;
    const float drive = 0.6f;
    const double targetRmsDb = -20.0;

    const auto chord = synth::powerChord (sr, seconds);
    std::vector<float> all;
    double t = 0.0;

    for (int m = 0; m < rid::kNumModels; ++m)
    {
        rid::Engine e;
        e.prepare (sr);
        e.configure (m, drive, 0.0f, true);
        std::vector<float> y (chord.size());
        for (size_t n = 0; n < chord.size(); ++n)
            y[n] = e.process (chord[n]);

        auto d = synth::decimate4 (y);

        double s = 0;
        float peak = 0;
        for (float v : d) { s += (double) v * v; peak = std::max (peak, std::fabs (v)); }
        const double rmsDb = 20.0 * std::log10 (std::sqrt (s / (double) d.size()) + 1e-12);
        const double suggested = rid::kModels[m].outDb + (targetRmsDb - rmsDb);

        std::printf ("%5.1fs  %-30s  rms %6.1f dBFS  peak %.2f  outDb now %6.1f  suggested %6.1f\n",
                     t, rid::kModels[m].name, rmsDb, peak, rid::kModels[m].outDb, suggested);

        // Loudness-match the audition and keep it clear of clipping.
        float g = (float) std::pow (10.0, (targetRmsDb - rmsDb) / 20.0);
        if (peak * g > 0.9f)
            g = 0.9f / peak;
        for (auto& v : d) v *= g;

        all.insert (all.end(), d.begin(), d.end());
        all.insert (all.end(), (size_t) (0.7 * 44100.0), 0.0f);
        t += (double) d.size() / 44100.0 + 0.7;
    }

    synth::writeWav16 (outPath, all, 44100);
    std::printf ("Wrote %s (%.1f s)\n", outPath.c_str(), (double) all.size() / 44100.0);
}
