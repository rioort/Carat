# Carat – Mastering Converter Clipper

Clipper da mastering ispirato al comportamento dei convertitori A/D di fascia alta quando vengono spinti: clip pulito a 0 dBFS e *soft saturation* istantanea (senza attacco né rilascio) seguita da guadagno digitale. VST3 e Audio Unit per macOS (Apple Silicon e Intel), VST3 per Windows (64 bit).

## Controlli

| Controllo | Range | Note |
|---|---|---|
| Input | -12 … +24 dB | quanto spingi il "convertitore" |
| Soft Saturation | Off / +3 dB / +6 dB / Var | +3 dB: soglia -6 dBFS, per materiale già compresso. +6 dB: soglia -12 dBFS, per materiale dinamico. Var: da 1 a 9 dB con *Amount* |
| Ceiling | -6 … 0 dB | livello massimo di uscita |
| True Peak Limit | on/off | limiter con lookahead: anche i picchi tra un campione e l'altro restano sotto il Ceiling |
| Listen | Stereo / Mid / Side | solo ascolto, non cambia l'elaborazione |
| Delta | on/off | ascolti solo ciò che il clipper toglie |
| Bypass + Level Match | on/off | confronto con l'originale **a pari loudness**, per giudicare il suono e non il volume |

**Meter:** Integrated / Short-term / Momentary LUFS (ITU-R BS.1770, EBU R128), True Peak massimo, "Louder by" (quanto è più forte l'uscita dell'ingresso), riduzione di clip/saturazione e del limiter, curva di trasferimento con il livello in tempo reale.

## Sotto il cofano

- Clip e saturazione in **oversampling 8x** con filtri FIR a fase lineare, poi un filtro anti-Nyquist a fase lineare piatto fino a 0.45 fs (19.8 kHz a 44.1 kHz).
- Limiter True Peak con stima inter-sample 8x: verificato con un misuratore di riferimento indipendente, anche con il clipper spinto di +12 dB.
- Stesso comportamento a 44.1, 48, 88.2, 96 e 192 kHz (verificato dai test). Latenza dichiarata alla DAW e compensata.
- Parametri smussati, protezione da NaN e denormali, nessuna allocazione di memoria durante l'elaborazione.

## Compilazione

Ogni modifica caricata su GitHub avvia la compilazione automatica (tab **Actions**) su Mac e Windows: test del DSP, validazione `auval` e `pluginval`, poi la pagina di download pubblica **Releases**.

Basato su [JUCE](https://juce.com) (licenza gratuita per uso personale).
