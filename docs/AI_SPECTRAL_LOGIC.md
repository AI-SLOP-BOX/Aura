# Calculation-based Spectral Balancing (Aura SCAE Suite)

## 🎯 Goal:
Provide a data-driven "Reference Balancer" that suggests EQ curves to match 
a target genre spectrum (e.g., Pop, EDM, Rock).

## 🚀 Algorithm:
1. **FFT Analysis**: Perform a 4096-point STFT on the Master Bus.
2. **Log-Scale Magnitude Averaging**: Convert the linear FFT bins into 
   Standard Bandwidth (Bark or ERB) to match human hearing.
3. **Reference Curve Generation**: 
   - **Pink Noise Target**: Used for general balance (slope of -3dB/octave).
   - **Genre Profile**: A database of average frequency magnitudes per genre.
4. **Error Calculation**: `Delta(f) = AverageSpectrum(f) - TargetSpectrum(f)`.
5. **EQ Recommendation**:
   - Apply a smoothing function (Gaussian blur) to the Delta curve to 
     generate 'Musical' (Broad Q) EQ points.
   - Output as a set of EQ parameters (Freq, Gain, Q).

## 📊 Implementation:
The logic is implemented in `src/scae/spectral_balancer.hpp` and uses the 
`MaskingAnalyzer` data to correct frequency clashes in real-time.
This is not 'Black Box AI', but 'Calculated Signal Analysis' (SCAE).
