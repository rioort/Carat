#pragma once
// Carat - motore DSP (C++ puro, nessuna dipendenza da JUCE, testabile da solo).
//
// Catena ispirata al comportamento documentato dei convertitori A/D da mastering:
//   Input -> clip pulito a 0 dBFS (il convertitore) -> soft saturation
//   (soglia + guadagno digitale) -> Ceiling -> limiter True Peak.
// Tutti i tempi sono espressi in secondi e convertiti con il sample rate:
// il comportamento e' identico a qualsiasi frequenza di campionamento.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace carat
{

inline float sanitize (float x) { return std::isfinite (x) ? x : 0.0f; }
inline float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }
inline float linToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

inline float onePoleCoeff (double seconds, double sampleRate)
{
    if (seconds <= 0.0) return 0.0f;
    return (float) std::exp (-1.0 / (seconds * sampleRate));
}

//==============================================================================
// Soft saturation in stile convertitore da mastering.
// Sotto la soglia il segnale passa intatto; sopra, i picchi vengono arrotondati
// in modo progressivo (pendenza continua, nessun tempo di attacco/rilascio) e
// non superano mai "ceiling". Dopo la curva si applica il guadagno digitale.
//
//   amountDb = 3 -> soglia -6 dBFS,  picchi max -3 dBFS, guadagno +3 dB
//   amountDb = 6 -> soglia -12 dBFS, picchi max -6 dBFS, guadagno +6 dB
struct SoftSat
{
    static float curve (float x, float threshold, float ceiling)
    {
        const float a = std::abs (x);
        if (a <= threshold) return x;
        const float span = ceiling - threshold;
        const float y = threshold + span * std::tanh ((a - threshold) / span);
        return std::copysign (y, x);
    }

    static float thresholdFor (float amountDb) { return dbToLin (-2.0f * amountDb); }
    static float ceilingFor   (float amountDb) { return dbToLin (-amountDb); }
    static float makeupFor    (float amountDb) { return dbToLin (amountDb); }
};

//==============================================================================
// Stadio "convertitore": gira al sample rate sovracampionato.
class ConverterStage
{
public:
    void prepare (double oversampledRate)
    {
        smooth = onePoleCoeff (0.02, oversampledRate);
        initialised = false;
    }

    // drive lineare, satAmountDb (0..9), satOn
    void setTargets (float driveLin, float satAmountDb, bool satOn)
    {
        tDrive = driveLin;
        tAmount = satAmountDb;
        tSatMix = satOn ? 1.0f : 0.0f;
        if (! initialised)
        {
            sDrive = tDrive; sAmount = tAmount; sSatMix = tSatMix;
            initialised = true;
        }
    }

    // Elabora un campione (tutti i canali) in place. Ritorna la riduzione di
    // picco (dB) applicata da clip + saturazione su quel campione.
    float processFrame (float* const* data, int numCh, int index)
    {
        sDrive  = tDrive  + (sDrive  - tDrive)  * smooth;
        sAmount = tAmount + (sAmount - tAmount) * smooth;
        sSatMix = tSatMix + (sSatMix - tSatMix) * smooth;

        const float thr = SoftSat::thresholdFor (sAmount);
        const float cel = SoftSat::ceilingFor (sAmount);
        const float mk  = SoftSat::makeupFor (sAmount);
        const float refGain = 1.0f + (mk - 1.0f) * sSatMix;

        float maxRed = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            const float xd = sanitize (data[c][index]) * sDrive;
            const float xc = std::clamp (xd, -1.0f, 1.0f);           // clip del convertitore
            const float ys = SoftSat::curve (xc, thr, cel) * mk;     // soft saturation + guadagno
            const float y  = xc + (ys - xc) * sSatMix;
            data[c][index] = y;

            const float ref = std::abs (xd) * refGain;
            const float ay = std::abs (y);
            if (ref > 1.0e-6f && ay > 1.0e-9f && ref > ay)
                maxRed = std::max (maxRed, linToDb (ref / ay));
        }
        return maxRed;
    }

    // Curva statica completa (per il grafico dell'interfaccia)
    static float staticTransfer (float x, float driveLin, float satAmountDb, bool satOn)
    {
        const float xc = std::clamp (x * driveLin, -1.0f, 1.0f);
        if (! satOn) return xc;
        return SoftSat::curve (xc, SoftSat::thresholdFor (satAmountDb), SoftSat::ceilingFor (satAmountDb))
               * SoftSat::makeupFor (satAmountDb);
    }

private:
    float smooth = 0.0f;
    bool initialised = false;
    float tDrive = 1, tAmount = 3, tSatMix = 0;
    float sDrive = 1, sAmount = 3, sSatMix = 0;
};

//==============================================================================
// Filtro passa-basso FIR a fase lineare, piatto fino a 0.45 fs (19.8 kHz a
// 44.1 kHz) e chiuso a 0.5 fs. Toglie solo la fascia vicinissima a Nyquist che
// il clipping riempie di armoniche e dove i picchi reali non sono misurabili.
class BandLimiter
{
public:
    static constexpr int half = 47;
    static constexpr int taps = half * 2 + 1;
    static constexpr int latency = half;

    BandLimiter()
    {
        const double pi = 3.14159265358979323846, fc = 0.475, beta = 8.0;
        auto i0 = [] (double x) { double sum = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); sum += t; } return sum; };
        double sum = 0.0;
        for (int k = 0; k < taps; ++k)
        {
            const double t = k - half, u = t / half;
            const double sinc = k == half ? 2 * fc : std::sin (2 * pi * fc * t) / (pi * t);
            coeff[(size_t) k] = sinc * i0 (beta * std::sqrt (std::max (0.0, 1.0 - u * u))) / i0 (beta);
            sum += coeff[(size_t) k];
        }
        for (auto& c : coeff) c /= sum;
        reset();
    }

    void reset() { for (auto& h : hist) h.fill (0.0f); pos = 0; }

    void process (float* const* data, int numCh, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh && c < 2; ++c)
            {
                auto& h = hist[(size_t) c];
                h[(size_t) pos] = h[(size_t) (pos + taps)] = sanitize (data[c][i]); // buffer doppio: lettura contigua
                double acc = 0.0;
                const float* x = h.data() + pos + 1;   // dal piu' vecchio al piu' recente
                for (int k = 0; k < taps; ++k) acc += coeff[(size_t) k] * x[k];
                data[c][i] = (float) acc;
            }
            pos = (pos + 1) % taps;
        }
    }

private:
    std::array<double, taps> coeff {};
    std::array<std::array<float, taps * 2>, 2> hist {};
    int pos = 0;
};

//==============================================================================
// Stima del picco reale (inter-sample) con interpolazione 8x a sinc finestrata.
// Restituisce il picco del campione con ritardo "delay" campioni.
class TruePeakEstimator
{
public:
    static constexpr int half = 32;            // campioni per lato
    static constexpr int phases = 7;           // posizioni intermedie (8x)
    static constexpr int taps = half * 2;
    static constexpr int delay = half;

    TruePeakEstimator()
    {
        const double pi = 3.14159265358979323846;
        for (int p = 0; p < phases; ++p)
        {
            const double f = (p + 1) / (double) (phases + 1);
            double sum = 0.0;
            for (int k = 0; k < taps; ++k)
            {
                // posizione relativa al campione centrale (indice half-1 .. )
                const double t = (double) (k - (half - 1)) - f;
                const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
                // finestra di Kaiser (beta 8) su +-half
                const double u = t / half;
                const double w = std::abs (u) >= 1.0 ? 0.0 : besselI0 (8.0 * std::sqrt (1.0 - u * u)) / besselI0 (8.0);
                coeff[p][k] = sinc * w;
                sum += coeff[p][k];
            }
            for (int k = 0; k < taps; ++k) coeff[p][k] /= sum;
        }
        reset();
    }

    void reset() { hist.fill (0.0f); pos = 0; }

    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k) { term *= (x / (2 * k)) * (x / (2 * k)); sum += term; }
        return sum;
    }

    // Inserisce un campione; ritorna il picco reale stimato attorno al
    // campione inserito "delay" passi fa.
    float push (float x)
    {
        hist[(size_t) pos] = x;
        pos = (pos + 1) % taps;
        // hist ordinato dal piu' vecchio: indice logico k -> fisico (pos + k) % taps
        const float centre = hist[(size_t) ((pos + half - 1) % taps)];
        float peak = std::abs (centre);
        for (int p = 0; p < phases; ++p)
        {
            double acc = 0.0;
            for (int k = 0; k < taps; ++k)
                acc += coeff[p][k] * hist[(size_t) ((pos + k) % taps)];
            peak = std::max (peak, (float) std::abs (acc));
        }
        return peak;
    }

private:
    double coeff[phases][taps] {};
    std::array<float, taps> hist {};
    int pos = 0;
};

//==============================================================================
// Limiter True Peak con lookahead: nessun campione (ne' picco inter-sample
// stimato) supera il ceiling. Latenza fissa, anche quando e' disattivato.
class TruePeakLimiter
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate)
    {
        // window: durata della rampa di riduzione. La finestra del minimo e'
        // allargata di +-H campioni (il supporto dell'interpolatore) cosi' il
        // guadagno e' gia' costante attorno a ogni picco: anche i picchi
        // inter-sample del segnale ridotto restano sotto il ceiling.
        window = std::max (4, (int) std::lround (0.0015 * sampleRate));
        minWindow = window + 2 * TruePeakEstimator::half;
        latency = TruePeakEstimator::delay + window - 1 + TruePeakEstimator::half;
        releaseCoeff = onePoleCoeff (0.08, sampleRate);
        mixCoeff = onePoleCoeff (0.02, sampleRate);

        audioDelay.assign ((size_t) maxChannels, std::vector<float> ((size_t) latency + 1, 0.0f));
        minVals.assign ((size_t) minWindow, 1.0f);
        minIdx.assign ((size_t) minWindow, 0);
        boxRing.assign ((size_t) window, 1.0f);
        reset();
    }

    void reset()
    {
        for (auto& e : est) e.reset();
        for (auto& d : audioDelay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (boxRing.begin(), boxRing.end(), 1.0f);
        boxSum = (double) window;
        boxPos = 0;
        dqHead = dqSize = 0;
        counter = 0;
        delayPos = 0;
        gainOut = 1.0f;
        sMix = -1.0f;
        maxReductionDb = 0.0f;
    }

    int getLatency() const { return latency; }

    // Elabora in place. Se enabled=false il segnale viene solo ritardato.
    void process (float* const* data, int numCh, int n, float ceilingLin, bool enabled)
    {
        const float targetMix = enabled ? 1.0f : 0.0f;
        if (sMix < 0.0f) sMix = targetMix;

        for (int i = 0; i < n; ++i)
        {
            // 1) picco reale (per il campione di "delay" passi fa) e guadagno richiesto
            float tp = 0.0f;
            for (int c = 0; c < numCh && c < maxChannels; ++c)
                tp = std::max (tp, est[(size_t) c].push (sanitize (data[c][i])));
            // margine di 0.06 dB per l'errore residuo della stima inter-sample
            const float target = ceilingLin * 0.99312f;
            const float req = tp > target ? target / tp : 1.0f;

            // 2) minimo scorrevole sulla finestra (deque monotona)
            const long long idx = counter++;
            while (dqSize > 0 && minVals[(size_t) back()] >= req) --dqSize;
            const int slot = (dqHead + dqSize) % minWindow;
            minVals[(size_t) slot] = req;
            minIdx[(size_t) slot] = idx;
            ++dqSize;
            while (minIdx[(size_t) dqHead] <= idx - minWindow) { dqHead = (dqHead + 1) % minWindow; --dqSize; }
            const float held = minVals[(size_t) dqHead];

            // 3) media mobile: la riduzione arriva in modo morbido ma completo
            boxSum += (double) held - (double) boxRing[(size_t) boxPos];
            boxRing[(size_t) boxPos] = held;
            boxPos = (boxPos + 1) % window;
            const float box = (float) (boxSum / (double) window);

            // 4) rilascio
            gainOut = box < gainOut ? box : box + (gainOut - box) * releaseCoeff;

            sMix = targetMix + (sMix - targetMix) * mixCoeff;
            const float g = 1.0f + (gainOut - 1.0f) * sMix;
            if (g < 0.9999f) maxReductionDb = std::max (maxReductionDb, -linToDb (g));

            // 5) audio ritardato della stessa quantita'
            const int rd = (delayPos + 1) % (latency + 1);
            for (int c = 0; c < numCh && c < maxChannels; ++c)
            {
                auto& d = audioDelay[(size_t) c];
                d[(size_t) delayPos] = sanitize (data[c][i]);
                float y = d[(size_t) rd] * g;
                if (enabled) y = std::clamp (y, -ceilingLin, ceilingLin); // sicurezza sul campione
                data[c][i] = y;
            }
            delayPos = rd;
        }
    }

    float takeMaxReductionDb() { const float r = maxReductionDb; maxReductionDb = 0.0f; return r; }

private:
    int back() const { return (dqHead + dqSize - 1) % minWindow; }

    std::array<TruePeakEstimator, maxChannels> est;
    std::vector<std::vector<float>> audioDelay;
    std::vector<float> minVals;
    std::vector<long long> minIdx;
    std::vector<float> boxRing;
    double boxSum = 0.0;
    int window = 64, minWindow = 112, latency = 0, boxPos = 0, dqHead = 0, dqSize = 0, delayPos = 0;
    long long counter = 0;
    float releaseCoeff = 0.0f, mixCoeff = 0.0f, gainOut = 1.0f, sMix = -1.0f, maxReductionDb = 0.0f;
};

//==============================================================================
// Loudness secondo ITU-R BS.1770 / EBU R128: Momentary (400 ms),
// Short-term (3 s), Integrated (gating assoluto -70 LUFS e relativo -10 LU).
// Nessuna allocazione durante l'elaborazione.
class LoudnessMeter
{
public:
    static constexpr float silence = -100.0f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        subLen = std::max (1, (int) std::lround (0.1 * sr));
        // Filtro K: pre-filtro shelving + passa-alto RLB (coefficienti per qualsiasi fs)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (3.14159265358979323846 * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            s1 = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                   2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (3.14159265358979323846 * f0 / sr);
            const double d = 1.0 + K / Q + K * K;
            s2 = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / d, (1.0 - K / Q + K * K) / d };
        }
        reset();
    }

    void reset()
    {
        for (auto& c : ch) c = Chan{};
        subSum = 0.0; subCount = 0;
        ring.fill (0.0); ringCount = 0; ringPos = 0;
        hCount.fill (0); hEnergy.fill (0.0);
        momentary = shortTerm = integrated = silence;
    }

    void process (const float* const* data, int numCh, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double e = 0.0;
            for (int c = 0; c < numCh && c < 2; ++c)
            {
                double x = sanitize (data[c][i]);
                x = biquad (s1, ch[(size_t) c].z1a, ch[(size_t) c].z2a, x);
                x = biquad (s2, ch[(size_t) c].z1b, ch[(size_t) c].z2b, x);
                e += x * x;
            }
            subSum += e;
            if (++subCount >= subLen) closeSubBlock();
        }
    }

    float getMomentary() const  { return momentary; }
    float getShortTerm() const  { return shortTerm; }
    float getIntegrated() const { return integrated; }

    static float energyToLufs (double e) { return e > 1.0e-20 ? (float) (-0.691 + 10.0 * std::log10 (e)) : silence; }

private:
    struct Coeffs { double b0, b1, b2, a1, a2; };
    struct Chan { double z1a = 0, z2a = 0, z1b = 0, z2b = 0; };

    static double biquad (const Coeffs& k, double& z1, double& z2, double x)
    {
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2;
        z2 = k.b2 * x - k.a2 * y;
        if (std::abs (z1) < 1e-30) z1 = 0;
        if (std::abs (z2) < 1e-30) z2 = 0;
        return y;
    }

    void closeSubBlock()
    {
        ring[(size_t) ringPos] = subSum / (double) subLen;
        ringPos = (ringPos + 1) % 30;
        ringCount = std::min (30, ringCount + 1);
        subSum = 0.0; subCount = 0;

        auto meanLast = [this] (int k)
        {
            double s = 0.0;
            for (int j = 1; j <= k; ++j) s += ring[(size_t) ((ringPos - j + 30) % 30)];
            return s / (double) k;
        };

        if (ringCount >= 4)
        {
            const double eM = meanLast (4);
            momentary = energyToLufs (eM);

            // blocco di gating da 400 ms (sovrapposizione 75%)
            const float l = momentary;
            if (l > -70.0f)
            {
                const int b = std::clamp ((int) ((l + 70.0f) * 10.0f), 0, bins - 1);
                ++hCount[(size_t) b];
                hEnergy[(size_t) b] += eM;
            }
            updateIntegrated();
        }
        shortTerm = ringCount >= 30 ? energyToLufs (meanLast (30)) : silence;
    }

    void updateIntegrated()
    {
        long long n = 0; double e = 0.0;
        for (int b = 0; b < bins; ++b) { n += hCount[(size_t) b]; e += hEnergy[(size_t) b]; }
        if (n == 0) { integrated = silence; return; }
        const float rel = energyToLufs (e / (double) n) - 10.0f;
        const int first = std::clamp ((int) std::ceil ((rel + 70.0f) * 10.0f), 0, bins);
        n = 0; e = 0.0;
        for (int b = first; b < bins; ++b) { n += hCount[(size_t) b]; e += hEnergy[(size_t) b]; }
        integrated = n > 0 ? energyToLufs (e / (double) n) : silence;
    }

    static constexpr int bins = 800; // da -70 a +10 LUFS, passo 0.1 LU

    double sr = 48000.0;
    int subLen = 4800, subCount = 0, ringCount = 0, ringPos = 0;
    double subSum = 0.0;
    Coeffs s1 {}, s2 {};
    std::array<Chan, 2> ch {};
    std::array<double, 30> ring {};
    std::array<long long, bins> hCount {};
    std::array<double, bins> hEnergy {};
    float momentary = silence, shortTerm = silence, integrated = silence;
};

} // namespace carat
