# Changelog

## v1.10.0

Core DSP engine upgrades across resampling, filtering, dynamics, and timing:

- **Kaiser-windowed polyphase sinc resampler:** Suppresses downward imaging and upward sidelobe aliasing during pitch-shifted playback.
- **Octave mip-map pyramid:** Pre-computed anti-aliased octave mip levels in `WavReader`/`AudioSampleBuffer` with automatic octave selection in `SampleVoice`.
- **12 dB/oct ZDF TPT SVF:** Per-voice zero-delay-feedback state-variable filters with resonance, pitch tracking, and live tail updates.
- **ADAA saturation:** Antiderivative antialiased `tanh` saturation per voice to suppress high-frequency harmonic foldover.
- **Exponential RC ADSR envelope:** Convex attack and concave decay/release curves modeled on analog RC behavior.
- **Click-free choke & voice stealing:** Cosine crossfade tails on voice choke and stealing.
- **Trim-boundary micro-fades:** Raised-cosine micro-fades and clamped interpolation at non-zero sample trim boundaries.
- **Key-normalized pitch mode:** Automatic tuning compensation using `SonicAnalyzer` root-note offset and `detuneCents`.
- **Sub-sample ratchets & Gaussian humanize:** Sub-sample fractional ratchet interpolation and Gaussian micro-timing and micro-cent pitch jitter.
- **Sample-smoothed master volume & 4x oversampled limiter:** Per-sample `masterVolume` smoothing and 2 ms lookahead 4x-oversampled master bus limiting.
