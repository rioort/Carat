// Test offline del motore DSP di Carat (nessuna DAW necessaria).
#include "CaratDsp.h"
#include <cstdio>
#include <random>
#include <vector>

using namespace carat;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static const double kRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

// Picco reale di riferimento: interpolazione 32x con sinc lunga (indipendente dal plugin)
static float referenceTruePeak (const std::vector<float>& x)
{
    const int L = 64, over = 32;
    const double pi = 3.14159265358979323846;
    float peak = 0.0f;
    for (size_t n = (size_t) L; n + (size_t) L < x.size(); ++n)
    {
        peak = std::max (peak, std::abs (x[n]));
        for (int p = 1; p < over; ++p)
        {
            const double f = (double) p / over;
            double acc = 0.0;
            for (int k = -L + 1; k <= L; ++k)
            {
                const double t = k - f;
                const double w = 0.5 + 0.5 * std::cos (pi * t / L);
                acc += x[n + (size_t) k] * std::sin (pi * t) / (pi * t) * w;
            }
            peak = std::max (peak, (float) std::abs (acc));
        }
    }
    return peak;
}

static std::vector<float> sine (double sr, double seconds, float amp, double f, double phase = 0.0)
{
    std::vector<float> v ((size_t) (sr * seconds));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2.0 * 3.14159265358979 * f * (double) i / sr + phase);
    return v;
}

// Rumore gaussiano filtrato passa-basso a 0.45 fs (FIR sinc finestrata, 255 tap)
static std::vector<float> bandNoise (double sr, double seconds, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> nd (0.0f, 0.7f);
    const int taps = 255, M = taps / 2;
    const double fc = 0.45, pi = 3.14159265358979323846;
    std::vector<double> h ((size_t) taps);
    for (int k = 0; k < taps; ++k)
    {
        const double t = k - M;
        const double s = t == 0 ? 2 * fc : std::sin (2 * pi * fc * t) / (pi * t);
        h[(size_t) k] = s * (0.42 - 0.5 * std::cos (2 * pi * k / (taps - 1)) + 0.08 * std::cos (4 * pi * k / (taps - 1)));
    }
    std::vector<float> w ((size_t) (sr * seconds) + (size_t) taps), y ((size_t) (sr * seconds));
    for (auto& v : w) v = nd (rng);
    for (size_t n = 0; n < y.size(); ++n)
    {
        double acc = 0.0;
        for (int k = 0; k < taps; ++k) acc += h[(size_t) k] * w[n + (size_t) k];
        y[n] = (float) acc;
    }
    return y;
}

static void runLimiter (double sr, std::vector<float>& l, std::vector<float>& r, float ceiling, int& latency)
{
    TruePeakLimiter lim; lim.prepare (sr); latency = lim.getLatency();
    for (size_t s = 0; s < l.size(); s += 333) // blocchi di dimensione "strana"
    {
        const int n = (int) std::min<size_t> (333, l.size() - s);
        float* d[2] = { l.data() + s, r.data() + s };
        lim.process (d, 2, n, ceiling, true);
    }
}

static void runLufs (double sr, const std::vector<float>& l, const std::vector<float>& r, LoudnessMeter& m)
{
    m.prepare (sr);
    for (size_t s = 0; s < l.size(); s += 512)
    {
        const int n = (int) std::min<size_t> (512, l.size() - s);
        const float* d[2] = { l.data() + s, r.data() + s };
        m.process (d, 2, n);
    }
}

int main()
{
    std::printf ("1) Soft saturation (curva documentata: +3 dB soglia -6, +6 dB soglia -12)\n");
    for (float amt : { 3.0f, 6.0f })
    {
        const float thr = SoftSat::thresholdFor (amt), cel = SoftSat::ceilingFor (amt), mk = SoftSat::makeupFor (amt);
        const float below = thr * 0.99f;
        CHECK (SoftSat::curve (below, thr, cel) == below, "sotto soglia il segnale deve passare intatto");
        const float fs = SoftSat::curve (1.0f, thr, cel) * mk;
        std::printf ("   +%.0f dB: soglia %.1f dBFS, 0 dBFS in ingresso -> %.2f dBFS in uscita\n", amt, linToDb (thr), linToDb (fs));
        CHECK (fs <= 1.0f && fs > dbToLin (-0.2f), "a fondo scala l'uscita deve stare appena sotto 0 dBFS");
        const float eps = 1.0e-4f;
        const float slope = (SoftSat::curve (thr + eps, thr, cel) - SoftSat::curve (thr, thr, cel)) / eps;
        CHECK (std::abs (slope - 1.0f) < 0.01f, "pendenza discontinua alla soglia (%.3f)", slope);
        float prev = 0.0f; bool mono = true;
        for (int i = 0; i <= 1000; ++i) { const float y = SoftSat::curve (i / 1000.0f, thr, cel); mono = mono && y >= prev; prev = y; }
        CHECK (mono, "curva non monotona");
        // appena sopra la soglia l'effetto e' minimo, poi cresce
        const float r1 = linToDb ((thr * 1.12f) / SoftSat::curve (thr * 1.12f, thr, cel));
        const float r2 = linToDb ((thr * 1.8f) / SoftSat::curve (thr * 1.8f, thr, cel));
        std::printf ("          riduzione +1 dB sopra soglia: %.2f dB, +5 dB sopra soglia: %.2f dB\n", r1, r2);
        CHECK (r1 < 0.3f && r2 > r1 + 0.4f, "progressivita' della curva errata");
    }

    std::printf ("2) Clip del convertitore\n");
    CHECK (ConverterStage::staticTransfer (0.5f, 1.0f, 3.0f, false) == 0.5f, "sotto 0 dBFS senza saturazione deve essere trasparente");
    CHECK (ConverterStage::staticTransfer (2.0f, 1.0f, 3.0f, false) == 1.0f, "sopra 0 dBFS deve tagliare a 0 dBFS");
    CHECK (ConverterStage::staticTransfer (-3.0f, 1.0f, 3.0f, false) == -1.0f, "clip negativo errato");

    std::printf ("3) Loudness BS.1770: seno 1 kHz stereo a -20 dBFS deve leggere -20 LUFS\n");
    for (double sr : kRates)
    {
        auto l = sine (sr, 5.0, 0.1f, 1000.0), r = l;
        LoudnessMeter m; runLufs (sr, l, r, m);
        std::printf ("   %6.0f Hz: M %.2f  S %.2f  I %.2f LUFS\n", sr, m.getMomentary(), m.getShortTerm(), m.getIntegrated());
        CHECK (std::abs (m.getMomentary() + 20.0f) < 0.1f && std::abs (m.getShortTerm() + 20.0f) < 0.1f
               && std::abs (m.getIntegrated() + 20.0f) < 0.1f, "calibrazione LUFS errata a %.0f Hz", sr);
    }
    {
        const double sr = 48000.0;
        auto a = sine (sr, 10.0, 0.1f, 1000.0), b = sine (sr, 10.0, 0.01f, 1000.0);
        std::vector<float> l (a); l.insert (l.end(), b.begin(), b.end());
        l.resize (l.size() + (size_t) (sr * 10.0), 0.0f); // 10 s di silenzio
        auto r = l;
        LoudnessMeter m; runLufs (sr, l, r, m);
        std::printf ("   gating: 10 s a -20 + 10 s a -40 + 10 s di silenzio -> Integrated %.2f LUFS\n", m.getIntegrated());
        CHECK (std::abs (m.getIntegrated() + 20.0f) < 0.2f, "gating relativo/assoluto errato");
    }

    std::printf ("4) Limiter True Peak (ceiling -1 dBTP), misurato con riferimento 32x indipendente\n");
    for (double sr : kRates)
    {
        // a) picco inter-sample classico: seno a fs/4 sfasato di 45 gradi
        auto l = sine (sr, 1.0, dbToLin (1.5f), sr / 4.0, 3.14159265358979 / 4.0), r = l;
        const float inTp = referenceTruePeak (std::vector<float> (l.begin(), l.begin() + 4000));
        float inSp = 0; for (float v : l) inSp = std::max (inSp, std::abs (v));
        int lat; runLimiter (sr, l, r, dbToLin (-1.0f), lat);
        std::vector<float> tail (l.begin() + (long) (sr * 0.5), l.end());
        const float outA = linToDb (referenceTruePeak (tail));

        // b) rumore molto forte limitato a 0.45 fs (come il segnale che esce
        //    dall'oversampling del plugin: a 44.1 kHz circa 20 kHz)
        std::vector<float> nl = bandNoise (sr, 0.3, 1234), nr = bandNoise (sr, 0.3, 99);
        runLimiter (sr, nl, nr, dbToLin (-1.0f), lat);
        const float outB = linToDb (std::max (referenceTruePeak (nl), referenceTruePeak (nr)));

        std::printf ("   %6.0f Hz: seno fs/4 (picco campioni %.2f dBFS, reale %.2f dBTP) -> %.2f dBTP | rumore forte -> %.2f dBTP | latenza %d campioni (%.2f ms)\n",
                     sr, linToDb (inSp), linToDb (inTp), outA, outB, lat, 1000.0 * lat / sr);
        CHECK (outA <= -0.98f && outB <= -0.98f, "picco reale oltre il ceiling a %.0f Hz (%.2f / %.2f)", sr, outA, outB);
    }

    std::printf ("5) Limiter trasparente sotto il ceiling (solo ritardo) e latenza esatta\n");
    for (double sr : { 48000.0, 96000.0 })
    {
        auto l = sine (sr, 0.5, 0.3f, 997.0), r = l;
        const auto orig = l;
        int lat; runLimiter (sr, l, r, dbToLin (-1.0f), lat);
        float err = 0.0f;
        for (size_t i = (size_t) lat; i < l.size(); ++i) err = std::max (err, std::abs (l[i] - orig[i - (size_t) lat]));
        std::printf ("   %6.0f Hz: errore massimo rispetto all'originale ritardato: %.2e\n", sr, err);
        CHECK (err < 1.0e-6f, "il limiter altera un segnale sotto il ceiling o la latenza dichiarata e' errata");
    }

    std::printf ("6) Filtro anti-Nyquist: piatto nella banda udibile, chiuso vicino a Nyquist\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        auto gainAt = [sr] (double f)
        {
            BandLimiter b; auto l = sine (sr, 0.2, 0.5f, f), r = l;
            float* d[2] = { l.data(), r.data() }; b.process (d, 2, (int) l.size());
            float pk = 0; for (size_t i = l.size() / 2; i < l.size(); ++i) pk = std::max (pk, std::abs (l[i]));
            return linToDb (pk / 0.5f);
        };
        const float g1 = gainAt (1000.0), g19 = gainAt (19000.0), gN = gainAt (sr * 0.499);
        std::printf ("   %6.0f Hz: 1 kHz %+.3f dB | 19 kHz %+.3f dB | 0.499 fs %.1f dB\n", sr, g1, g19, gN);
        CHECK (std::abs (g1) < 0.01f && std::abs (g19) < 0.05f && gN < -40.0f, "risposta del filtro errata a %.0f Hz", sr);
    }

    std::printf ("7) Robustezza: NaN, Inf, valori enormi\n");
    {
        std::vector<float> l (48000, 0.0f); l[10] = NAN; l[20] = INFINITY; l[30] = 1.0e9f; auto r = l;
        int lat; runLimiter (48000.0, l, r, dbToLin (-1.0f), lat);
        bool ok = true; for (float v : l) ok = ok && std::isfinite (v) && std::abs (v) <= dbToLin (-1.0f) + 1e-6f;
        LoudnessMeter m; std::vector<float> bad (48000, NAN); runLufs (48000.0, bad, bad, m);
        CHECK (ok, "il limiter produce valori non finiti o oltre il ceiling");
        CHECK (std::isfinite (m.getMomentary()), "il meter LUFS produce valori non finiti");
        std::printf ("   uscita sempre finita ed entro il ceiling: %s\n", ok ? "si" : "NO");
    }

    std::printf (failures == 0 ? "\nTUTTI I TEST SUPERATI\n" : "\n%d TEST FALLITI\n", failures);
    return failures == 0 ? 0 : 1;
}
