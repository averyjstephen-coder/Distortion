# Rock Icons Drive

A guitar distortion plugin (AU / VST3 / Standalone) with 11 voicings inspired by classic rock tones.
Pick a model, then use **Drive**, **Tone**, **Level**, **Mix** and the **Cab sim** switch.

| Model | Aiming at (from memory, approximate) |
|---|---|
| Hendrix - Fuzz into Plexi | Fuzz Face-style fuzz into a cranked Marshall |
| Page - Plexi crunch | Les Paul into a cranked Plexi |
| Van Halen - Brown sound | Hot-rodded Plexi, tight and saturated |
| Angus - Cranked Plexi | Raw, bright, medium-gain Marshall |
| Richards - Maestro fuzz | Thin, raspy early silicon fuzz |
| Slash - Cranked JCM800 | Hot Marshall with an 800 Hz mid hump |
| Iommi - Treble booster stack | Treble booster into a dark, heavy stack |
| May - Treble booster AC30 | Treble booster into a cranked AC30 |
| Gilmour - Big Muff sustain | Muff-style fuzz with scooped mids |
| Young - Ragged tweed | Saggy, uneven cranked tweed Deluxe |
| Nebraska - Lo-fi tape | Gentle cassette-style saturation (not a heavy distortion) |

These are voicings built from generic waveshapers, filters and amp-style sag. They are **not circuit-accurate
models** of the original pedals or amps, and they will not sound identical to the recordings. Part of every classic
tone is the guitar, the hands, the speakers and the room.

## Signal flow
pre low-cut (and treble-booster shelf) -> drive gain with sag -> 1 to 3 clipping stages with coupling filters ->
bass / mid / treble voicing -> speaker-style low-pass (Cab sim) -> Level -> Mix. 4x oversampling in the plugin.
For best results use it before a real cab/IR, or leave Cab sim on.

## Hear it before you build it
`demo_all_models.wav` is a synthesized plucked power chord through each model in order (Drive 0.6,
loudness-matched). It only shows the distortion character; a real guitar will differ.

## Build (macOS, for Logic)
    cmake -B build -G Xcode
    cmake --build build --config Release
JUCE 8.0.4 is fetched automatically. Or push to GitHub with `./push-to-github.sh` and download the build from the
Actions tab (`gh run download -n RockIconsDrive-macOS`), then install with
`sudo ./install-mac.sh ~/Downloads/RockIconsDrive-macOS.zip`. Run `auval -v aufx Rdrv Pers` if Logic rejects it.

## Tests
    g++ -O2 -std=c++17 -ISource -Itests tests/test_engine.cpp -o engine_tests && ./engine_tests
Checks filter accuracy, even-harmonic behaviour, finite and bounded output for every model at extreme settings,
silence-in/silence-out, drive vs distortion, and the tone control.

## Status
Engine: unit-tested, rendered and loudness-matched. Plugin wrapper/editor: syntax-checked against the real
JUCE 8.0.4 headers, but never linked, run in a host, or listened to in real time.
