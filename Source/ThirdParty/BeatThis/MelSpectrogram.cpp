#include <stdexcept>
#include <limits>
#include <cstdint>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include "MelSpectrogram.h"
#include <vector>
#include <complex>
#include <cmath>
#include <numeric>
#include <algorithm>
#include "pocketfft_hdronly.h"

// Constructor
MelSpectrogram::MelSpectrogram() {
    create_mel_filterbank();
}

// Destructor
MelSpectrogram::~MelSpectrogram() {
    // Clean up FFTW plans if any were created dynamically
}

// Helper function: Convert Hz to Mel scale (Slaney)
float MelSpectrogram::hz_to_mel(float hz) {
    // Slaney scale
    double minimum_frequency = 0.0;
    double f_sp = 200.0 / 3.0;

    double mels = (hz - minimum_frequency) / f_sp;

    double min_log_hz = 1000.0;
    double min_log_mel = (min_log_hz - minimum_frequency) / f_sp;
    double logstep = std::log(6.4) / 27.0;

    if (hz >= min_log_hz) {
        mels = min_log_mel + std::log(hz / min_log_hz) / logstep;
    }
    return static_cast<float>(mels);
}

// Helper function: Convert Mel to Hz scale (Slaney)
float MelSpectrogram::mel_to_hz(float mel) {
    // Slaney scale
    double minimum_frequency = 0.0;
    double f_sp = 200.0 / 3.0;
    double freqs = minimum_frequency + f_sp * mel;

    double min_log_hz = 1000.0;
    double min_log_mel = (min_log_hz - minimum_frequency) / f_sp;
    double logstep = std::log(6.4) / 27.0;

    if (mel >= min_log_mel) {
        freqs = min_log_hz * std::exp(logstep * (mel - min_log_mel));
    }
    return static_cast<float>(freqs);
}

// Helper function: Create a Hann window
std::vector<float> MelSpectrogram::create_hann_window(int size) {
    std::vector<float> window(size);
    for (int i = 0; i < size; ++i) {
        window[i] = static_cast<float>(0.5f * (1.0f - std::cos(2.0f * M_PI * i / size)));
    }
    return window;
}

// Helper function: Create Mel filterbank
void MelSpectrogram::create_mel_filterbank() {
    // Calculate Mel frequencies for f_min, f_max
    double mel_min = hz_to_mel(static_cast<float>(f_min));
    double mel_max = hz_to_mel(static_cast<float>(f_max));

    // Create n_mels + 2 evenly spaced Mel points
    std::vector<double> mel_points(n_mels + 2);
    for (int i = 0; i < n_mels + 2; ++i) {
        mel_points[i] = mel_min + (mel_max - mel_min) * i / (n_mels + 1);
    }

    // Convert Mel points back to Hz
    std::vector<double> hz_points(n_mels + 2);
    for (int i = 0; i < n_mels + 2; ++i) {
        hz_points[i] = mel_to_hz(static_cast<float>(mel_points[i]));
    }

    // Convert Hz points to FFT bin numbers
    std::vector<double> freqs(n_fft / 2 + 1);
    for (int i = 0; i < n_fft / 2 + 1; ++i) {
        freqs[i] = static_cast<double>(i) * sample_rate / n_fft;
    }

    mel_filters.resize(n_mels);

    // Outer loop for frequency bins (rows)
    for (int i = 0; i < n_fft / 2 + 1; ++i) {
        double hz_i = freqs[i];
        // Inner loop for mel filters (columns)
        for (int j = 0; j < n_mels; ++j) {
            double hz_left = hz_points[j];
            double hz_center = hz_points[j + 1];
            double hz_right = hz_points[j + 2];

            double left_slope_val = 0.0;
            if (hz_center - hz_left != 0) {
                left_slope_val = (hz_i - hz_left) / (hz_center - hz_left);
            }

            double right_slope_val = 0.0;
            if (hz_right - hz_center != 0) {
                right_slope_val = (hz_right - hz_i) / (hz_right - hz_center);
            }
            
            const double weight = std::max(0.0, std::min(left_slope_val, right_slope_val));
            if (weight != 0.0) mel_filters[j].push_back({i, weight});
        }
    }
}

std::vector<std::vector<float>> MelSpectrogram::compute(const std::vector<float>& audio, const std::function<bool()>& cancelled) {
    // Reflect the same centered STFT indices directly; do not duplicate the
    // entire resampled signal just to prepend/append 512 samples.
    const int pad_size = n_fft / 2;
    if (cancelled && cancelled()) throw std::runtime_error("Rhythm analysis cancelled.");
    if (audio.size() <= (size_t) pad_size || audio.size() > (size_t) std::numeric_limits<int>::max())
        throw std::invalid_argument("Spectrogram input is outside the supported reflection/index range.");
    const auto window = create_hann_window(win_length);
    const int num_frames = (int) (audio.size() / hop_length + 1);
    std::vector<std::vector<float>> mel_spectrogram_output(num_frames, std::vector<float>(n_mels));

    // PocketFFT setup
    std::vector<std::complex<float>> out_pocket(n_fft / 2 + 1);
    std::vector<float> in_pocket(n_fft);
    pocketfft::shape_t shape = { (size_t)n_fft };
    pocketfft::stride_t stride_in = { sizeof(float) };
    pocketfft::stride_t stride_out = { sizeof(std::complex<float>) };

    std::vector<double> amplitude_spectrum(n_fft / 2 + 1);
    for (int i = 0; i < num_frames; ++i) {
        if (cancelled && cancelled()) throw std::runtime_error("Rhythm analysis cancelled.");
        // Extract frame and apply window
        int start_idx = i * hop_length;
        for (int j = 0; j < n_fft; ++j) {
            const int64_t position = (int64_t) start_idx + j - pad_size;
            const auto reflected = position < 0 ? -position : position >= (int64_t) audio.size() ? 2 * (int64_t) audio.size() - 2 - position : position;
            in_pocket[j] = audio[(size_t) reflected] * window[j];
        }

        // Execute FFT
        pocketfft::r2c(shape, stride_in, stride_out, 0, true, in_pocket.data(), out_pocket.data(), 1.0f);


        // Compute amplitude spectrum and apply normalization
        double normalization_factor = std::sqrt(static_cast<double>(win_length));
        for (int j = 0; j < n_fft / 2 + 1; ++j) {
            double real = out_pocket[j].real();
            double imag = out_pocket[j].imag();
            amplitude_spectrum[j] = std::sqrt(real * real + imag * imag) / normalization_factor;
        }
        
        // Apply Mel filterbank and log scaling
        for (int m = 0; m < n_mels; ++m) {
            double mel_energy = 0.0;
            for (const auto& item : mel_filters[m]) {
                mel_energy += amplitude_spectrum[item.bin] * item.weight;
            }
            mel_spectrogram_output[i][m] = static_cast<float>(std::log1p(log_multiplier * std::max(mel_energy, amin)));
        }
    }

    // No explicit cleanup needed for PocketFFT with std::vector

    return mel_spectrogram_output;
}
