// JackCore.h
// Jack: akıllı kompresör çekirdeği (Wavejackers)
//
// Başlık dosyası (header-only), C++17, harici bağımlılık yok.
// İçerik:
//   1) Params / Effective   : kullanıcı ayarları ve çözülmüş (gerçek) değerler
//   2) Compressor           : sıfır gecikmeli, otomatik eşikli, otomatik makeup'lı kompresör
//   3) Advisor              : yanlış ayarı fark edip öneri (ve uygulanabilir yama) üreten motor
//   4) Mood / Voice         : Jack'in ruh hali ve konuşma satırları
//
// Gerçek zamanlı güvenlik: Compressor::process() bellek ayırmaz, kilit kullanmaz.
// Advisor ve Voice (std::string kullanır) ses iş parçacığında ÇALIŞTIRILMAMALIDIR.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace jack {

// ---------------------------------------------------------------- yardımcılar
inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }
inline float linToDb(float lin) { return 20.0f * std::log10(std::max(lin, 1.0e-9f)); }
inline float clampf(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ---------------------------------------------------------------- kaynaklar
enum class Source : int { General = 0, Kick, Bass, Vocal, Synth, DrumBus, MixBus, Count };
enum class Feel : int { Soft = 0, Balanced, Punchy };

struct SourceProfile {
    const char* name;
    float ratioMax;   // Amount %100 iken oran
    float grMax;      // Amount %100 iken hedef gain reduction (dB)
    float attackMs;
    float releaseMs;
    float kneeDb;
    float hpfHz;      // yan zincir filtresi
    float goodMaxGr;  // öneri motoru için "sağlıklı" ortalama GR üst sınırı (dB)
};

inline const SourceProfile& profile(Source s) {
    static const SourceProfile kProfiles[] = {
        //  ad            ratioMax grMax att   rel   knee  hpf   goodMaxGr
        {"Genel",         4.0f,   10.0f, 15.f, 150.f, 6.f,  60.f, 8.0f},
        {"Kick",          4.0f,   10.0f, 25.f,  80.f, 3.f,  60.f, 8.0f},
        {"Bas",           4.0f,    9.0f, 20.f, 120.f, 6.f,  40.f, 7.0f},
        {"Vokal",         4.0f,   10.0f,  8.f, 120.f, 8.f, 120.f, 8.0f},
        {"Synth / Lead",  3.5f,    8.0f, 12.f, 130.f, 6.f,  80.f, 7.0f},
        {"Davul Bus",     4.0f,    8.0f, 30.f, 100.f, 4.f,  80.f, 7.0f},
        {"Mix Bus",       2.0f,    4.0f, 30.f, 200.f, 10.f, 90.f, 3.5f},
    };
    int i = std::min(std::max(static_cast<int>(s), 0), static_cast<int>(Source::Count) - 1);
    return kProfiles[i];
}

// ---------------------------------------------------------------- ayarlar
struct Params {
    // Basit katman (herkes için)
    Source source = Source::General;
    float amount = 40.0f;          // 0..100 "SIKIŞTIR"
    Feel feel = Feel::Balanced;
    float mixPercent = 100.0f;     // 0..100 (paralel kompresyon)
    float outTrimDb = 0.0f;        // -24..+12

    // Gelişmiş katman (advanced=true iken kullanılır)
    bool advanced = false;
    float thresholdDb = -18.0f;
    float ratio = 3.0f;
    float attackMs = 10.0f;        // 63% noktası
    float releaseMs = 120.0f;
    float kneeDb = 6.0f;
    float scHpfHz = 80.0f;
    float makeupDb = 0.0f;
    bool autoMakeup = true;
    bool programRelease = true;    // sıkıştırma arttıkça bırakma yavaşlar
};

// Çözülmüş, DSP'nin gerçekten kullandığı değerler
struct Effective {
    bool bypassed = false;
    bool autoThreshold = true;
    float manualThresholdDb = -18.f;
    float targetGrDb = 3.f;
    float ratio = 3.f;
    float attackMs = 15.f;
    float releaseMs = 150.f;
    float kneeDb = 6.f;
    float hpfHz = 60.f;
    bool autoMakeup = true;
    float manualMakeupDb = 0.f;
    bool programRelease = true;
    float mix = 1.f;
    float trimDb = 0.f;
};

inline Effective resolve(const Params& p) {
    Effective e;
    e.mix = clampf(p.mixPercent, 0.f, 100.f) * 0.01f;
    e.trimDb = clampf(p.outTrimDb, -24.f, 12.f);

    if (p.advanced) {
        e.autoThreshold = false;
        e.manualThresholdDb = clampf(p.thresholdDb, -70.f, 0.f);
        e.ratio = clampf(p.ratio, 1.0f, 100.f);
        e.attackMs = clampf(p.attackMs, 0.05f, 300.f);
        e.releaseMs = clampf(p.releaseMs, 5.f, 3000.f);
        e.kneeDb = clampf(p.kneeDb, 0.f, 24.f);
        e.hpfHz = clampf(p.scHpfHz, 10.f, 400.f);
        e.autoMakeup = p.autoMakeup;
        e.manualMakeupDb = clampf(p.makeupDb, 0.f, 24.f);
        e.programRelease = p.programRelease;
        e.bypassed = false;
        return e;
    }

    const SourceProfile& sp = profile(p.source);
    const float t = clampf(p.amount, 0.f, 100.f) * 0.01f;
    e.bypassed = (p.amount < 0.5f);
    e.autoThreshold = true;
    e.ratio = lerpf(1.5f, sp.ratioMax, std::pow(t, 0.8f));
    e.targetGrDb = sp.grMax * std::pow(t, 1.2f);

    float attMul = 1.0f, relMul = 1.0f;
    switch (p.feel) {
        case Feel::Soft:     attMul = 0.6f; relMul = 1.4f; break;  // vuruşları yumuşatır
        case Feel::Balanced: break;
        case Feel::Punchy:   attMul = 1.8f; relMul = 0.7f; break;  // vuruşu geçirir, yumruk bırakır
    }
    e.attackMs = sp.attackMs * attMul;
    e.releaseMs = sp.releaseMs * relMul;
    e.kneeDb = sp.kneeDb;
    e.hpfHz = sp.hpfHz;
    e.autoMakeup = true;
    e.programRelease = true;
    return e;
}

// ---------------------------------------------------------------- istatistik
struct Stats {
    bool valid = false;
    float inPeakDb = -120.f;
    float outPeakDb = -120.f;
    float avgGrDb = 0.f;       // aktif (sessiz olmayan) örneklerde ortalama GR
    float maxGrDb = 0.f;
    float activeRatio = 0.f;   // aktif örneklerde GR > 0.5 dB oranı
    float stuckRatio = 0.f;    // aktif örneklerde GR > 1.5 dB oranı
    float loudDiffDb = 0.f;    // çıkış RMS - giriş RMS (dB)
};

// ---------------------------------------------------------------- kompresör
class Compressor {
public:
    static constexpr int kMaxCh = 8;

    void prepare(double sampleRate) {
        fs = static_cast<float>(sampleRate);
        aProgAtt = std::exp(-1.0f / (0.020f * fs));
        aProgRel = std::exp(-1.0f / (3.000f * fs));
        aDetRel = std::exp(-1.0f / (0.010f * fs));
        aLoud = std::exp(-1.0f / (0.400f * fs));
        aMakeup = std::exp(-1.0f / (1.000f * fs));
        aSmooth = std::exp(-1.0f / (0.020f * fs));
        aPeakRel = std::exp(-1.0f / (6.000f * fs));
        windowLen = static_cast<int>(2.0f * fs);
        reset();
        setParams(params);
    }

    void reset() {
        for (auto& b : hpf) b.clear();  // yalnızca durum; katsayılar korunur
        detEnv = 0.f;
        grState = 0.f;
        progLevelDb = -40.f;
        msIn = msOutPre = 0.f;
        makeupAutoDb = 0.f;
        pkInEnv = pkPreEnv = 0.f;
        mixS = eff.mix;
        trimS = dbToLin(eff.trimDb);
        manualMkS = dbToLin(eff.manualMakeupDb);
        curThresholdDb = -20.f;
        resetWindow();
        stats = Stats{};
    }

    void setParams(const Params& p) {
        params = p;
        eff = resolve(p);
        attCoef = std::exp(-1.0f / (eff.attackMs * 0.001f * fs));
        if (std::fabs(eff.hpfHz - hpfDesigned) > 0.01f) {
            hpfDesigned = eff.hpfHz;
            for (auto& b : hpf) b.designHighpass(hpfDesigned, fs);
        }
    }

    // Yerinde (in-place) işler. ch[c][i]
    void process(float* const* ch, int numCh, int n) {
        numCh = std::min(numCh, kMaxCh);
        const float invCh = 1.0f / static_cast<float>(std::max(numCh, 1));
        const float slopeDefault = 1.0f - 1.0f / eff.ratio;
        const float mixTarget = eff.mix;
        const float trimTarget = dbToLin(eff.trimDb);
        const float mkTarget = dbToLin(eff.manualMakeupDb);

        for (int i = 0; i < n; ++i) {
            // yumuşatılmış kullanıcı kontrolleri (zipper önleme)
            mixS = mixTarget + aSmooth * (mixS - mixTarget);
            trimS = trimTarget + aSmooth * (trimS - trimTarget);
            manualMkS = mkTarget + aSmooth * (manualMkS - mkTarget);

            // 1) yan zincir: HPF + bağlı (linked) tepe algılama
            float det = 0.f, inPk = 0.f, sqIn = 0.f;
            for (int c = 0; c < numCh; ++c) {
                const float x = ch[c][i];
                inPk = std::max(inPk, std::fabs(x));
                sqIn += x * x;
                det = std::max(det, std::fabs(hpf[c].process(x)));
            }
            sqIn *= invCh;
            detEnv = std::max(det, detEnv * aDetRel);
            const float levelDb = linToDb(detEnv);

            // 2) program seviyesi (otomatik eşik için "yüksek kısımların" seviyesi)
            if (levelDb > -70.f) {
                const float a = (levelDb > progLevelDb) ? aProgAtt : aProgRel;
                progLevelDb = levelDb + a * (progLevelDb - levelDb);
            }

            // 3) eşik
            float T;
            if (eff.autoThreshold) {
                if (eff.targetGrDb < 0.01f) {
                    T = 0.f;
                } else {
                    const float over = eff.targetGrDb * eff.ratio / (eff.ratio - 1.0f);
                    T = clampf(progLevelDb - over, -70.f, -1.f);
                }
            } else {
                T = eff.manualThresholdDb;
            }
            curThresholdDb = T;

            // 4) kazanç hesaplayıcı (yumuşak diz)
            float grTarget = eff.bypassed ? 0.f : computeGr(levelDb, T, slopeDefault, eff.kneeDb);

            // 5) balistik (dB alanında, yumuşatma gain computer'dan sonra)
            float coef;
            if (grTarget > grState) {
                coef = attCoef;
            } else {
                float relMs = eff.releaseMs;
                if (eff.programRelease)
                    relMs *= 1.0f + 1.5f * clampf(grState * 0.1f, 0.f, 1.f);
                coef = std::exp(-1.0f / (relMs * 0.001f * fs));
            }
            grState = grTarget + coef * (grState - grTarget);
            if (grState < 1.0e-4f && grTarget == 0.f) grState = 0.f;  // denormal/ölü bölge

            // 6) kazancı uygula
            const float g = dbToLin(-grState);
            float sqPre = 0.f;
            for (int c = 0; c < numCh; ++c) sqPre += (ch[c][i] * g) * (ch[c][i] * g);
            sqPre *= invCh;

            // 7) otomatik makeup: giriş ile (makeup öncesi) çıkış gürlüğünü eşitle
            msIn = sqIn + aLoud * (msIn - sqIn);
            msOutPre = sqPre + aLoud * (msOutPre - sqPre);
            float pkPre = 0.f;
            for (int c = 0; c < numCh; ++c) pkPre = std::max(pkPre, std::fabs(ch[c][i] * g));
            pkInEnv = std::max(inPk, pkInEnv * aPeakRel);
            pkPreEnv = std::max(pkPre, pkPreEnv * aPeakRel);
            float mkLin;
            if (eff.autoMakeup) {
                if (msIn > 1.0e-9f && msOutPre > 1.0e-12f) {
                    const float want = clampf(10.0f * std::log10(msIn / msOutPre), 0.f, 12.f);
                    makeupAutoDb = want + aMakeup * (makeupAutoDb - want);
                }
                // güvenlik tavanı: otomatik makeup, çıkış tepesini -1 dBFS'in (ya da girişin
                // kendi tepesinin) üstüne taşıyamaz. Yavaş attack'ta geçen vuruşlar clip yapmasın.
                // Tepe zarfı çok yavaş bırakır (6 sn), böylece tavan vuruşla birlikte "pompalamaz".
                const float ceilDb = std::max(-1.0f, linToDb(pkInEnv));
                const float headroom = std::max(0.0f, ceilDb - linToDb(pkPreEnv));
                mkLin = dbToLin(std::min(makeupAutoDb, headroom));
            } else {
                mkLin = manualMkS;
            }

            // 8) çıkış: kuru/ıslak karışım + trim
            float outPk = 0.f, sqOut = 0.f;
            for (int c = 0; c < numCh; ++c) {
                const float dry = ch[c][i];
                const float wet = dry * g * mkLin;
                const float y = (dry * (1.0f - mixS) + wet * mixS) * trimS;
                ch[c][i] = y;
                outPk = std::max(outPk, std::fabs(y));
                sqOut += y * y;
            }
            sqOut *= invCh;

            // 9) istatistik
            accumulate(inPk, outPk, sqIn, sqOut, grState, levelDb);
        }
    }

    // --- okuma
    float currentGrDb() const { return grState; }
    float currentThresholdDb() const { return curThresholdDb; }
    float currentMakeupDb() const {
        if (!eff.autoMakeup) return linToDb(manualMkS);
        const float ceilDb = std::max(-1.0f, linToDb(pkInEnv));
        return std::min(makeupAutoDb, std::max(0.0f, ceilDb - linToDb(pkPreEnv)));
    }
    const Stats& getStats() const { return stats; }
    const Effective& effective() const { return eff; }

    // Testler ve kapanış için: açık pencereyi hemen yayınla
    void flushStats() { finalizeWindow(); }

private:
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void clear() { z1 = z2 = 0.f; }
        void designHighpass(float f, float sr) {
            const float w0 = 6.28318530718f * clampf(f, 5.f, sr * 0.45f) / sr;
            const float cs = std::cos(w0), al = std::sin(w0) / (2.0f * 0.70710678f);
            const float a0 = 1.0f + al;
            b0 = (1.0f + cs) * 0.5f / a0;
            b1 = -(1.0f + cs) / a0;
            b2 = b0;
            a1 = -2.0f * cs / a0;
            a2 = (1.0f - al) / a0;
        }
        float process(float x) {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    static float computeGr(float xDb, float T, float slope, float W) {
        const float over = xDb - T;
        if (W <= 0.01f) return over > 0.f ? over * slope : 0.f;
        if (2.0f * over < -W) return 0.f;
        if (2.0f * std::fabs(over) <= W) {
            const float t = over + W * 0.5f;
            return slope * t * t / (2.0f * W);
        }
        return over * slope;
    }

    void resetWindow() {
        winN = activeN = over05 = over15 = 0;
        sumGrActive = 0.0;
        maxGr = 0.f;
        winInPk = winOutPk = 0.f;
        sumSqIn = sumSqOut = 0.0;
    }

    void accumulate(float inPk, float outPk, float sqIn, float sqOut, float gr, float levelDb) {
        ++winN;
        winInPk = std::max(winInPk, inPk);
        winOutPk = std::max(winOutPk, outPk);
        sumSqIn += sqIn;
        sumSqOut += sqOut;
        if (levelDb > -50.f) {
            ++activeN;
            sumGrActive += gr;
            if (gr > 0.5f) ++over05;
            if (gr > 1.5f) ++over15;
            maxGr = std::max(maxGr, gr);
        }
        if (winN >= windowLen) finalizeWindow();
    }

    void finalizeWindow() {
        if (winN == 0) return;  // boş pencere, önceki ölçümün üstüne yazma
        Stats s;
        s.valid = activeN >= static_cast<int>(0.25f * fs);
        s.inPeakDb = linToDb(winInPk);
        s.outPeakDb = linToDb(winOutPk);
        if (activeN > 0) {
            s.avgGrDb = static_cast<float>(sumGrActive / activeN);
            s.activeRatio = static_cast<float>(over05) / static_cast<float>(activeN);
            s.stuckRatio = static_cast<float>(over15) / static_cast<float>(activeN);
        }
        s.maxGrDb = maxGr;
        if (sumSqIn > 1.0e-12 && sumSqOut > 1.0e-12)
            s.loudDiffDb = static_cast<float>(10.0 * std::log10(sumSqOut / sumSqIn));
        stats = s;
        resetWindow();
    }

    // durum
    float fs = 44100.f;
    Params params;
    Effective eff = resolve(Params{});
    Biquad hpf[kMaxCh];
    float hpfDesigned = -1.f;

    float aProgAtt = 0, aProgRel = 0, aDetRel = 0, aLoud = 0, aMakeup = 0, aSmooth = 0, aPeakRel = 0;
    float attCoef = 0;
    float detEnv = 0, grState = 0, progLevelDb = -40.f;
    float msIn = 0, msOutPre = 0, makeupAutoDb = 0;
    float pkInEnv = 0, pkPreEnv = 0;
    float mixS = 1.f, trimS = 1.f, manualMkS = 1.f;
    float curThresholdDb = -20.f;

    // istatistik penceresi
    int windowLen = 88200;
    int winN = 0, activeN = 0, over05 = 0, over15 = 0;
    double sumGrActive = 0.0, sumSqIn = 0.0, sumSqOut = 0.0;
    float maxGr = 0.f, winInPk = 0.f, winOutPk = 0.f;
    Stats stats;
};

// ---------------------------------------------------------------- öneri motoru
enum class Severity : int { Info = 0, Tip = 1, Warning = 2 };

enum RuleId : int {
    kRuleClip = 1,
    kRuleTooMuch,
    kRuleIdle,
    kRuleFastAttack,
    kRuleFastRelease,
    kRuleLimiterRatio,
    kRuleHardKnee,
    kRuleLoudnessIllusion,
    kRuleParallel,
    kRuleLowFreq,
    kRuleAllGood
};

enum PatchField : uint32_t {
    kPAmount = 1u << 0,
    kPMix = 1u << 1,
    kPOutTrim = 1u << 2,
    kPThreshold = 1u << 3,
    kPRatio = 1u << 4,
    kPAttack = 1u << 5,
    kPRelease = 1u << 6,
    kPKnee = 1u << 7,
    kPHpf = 1u << 8,
    kPAutoMakeup = 1u << 9
};

struct Patch {
    uint32_t mask = 0;
    Params values;  // yalnızca mask'te işaretli alanlar geçerlidir
};

inline void applyPatch(Params& p, const Patch& patch) {
    const Params& v = patch.values;
    if (patch.mask & kPAmount) p.amount = v.amount;
    if (patch.mask & kPMix) p.mixPercent = v.mixPercent;
    if (patch.mask & kPOutTrim) p.outTrimDb = v.outTrimDb;
    if (patch.mask & kPThreshold) p.thresholdDb = v.thresholdDb;
    if (patch.mask & kPRatio) p.ratio = v.ratio;
    if (patch.mask & kPAttack) p.attackMs = v.attackMs;
    if (patch.mask & kPRelease) p.releaseMs = v.releaseMs;
    if (patch.mask & kPKnee) p.kneeDb = v.kneeDb;
    if (patch.mask & kPHpf) p.scHpfHz = v.scHpfHz;
    if (patch.mask & kPAutoMakeup) p.autoMakeup = v.autoMakeup;
}

struct Advice {
    int ruleId = 0;
    Severity severity = Severity::Info;
    std::string message;     // Jack'in sözü (Türkçe)
    std::string applyLabel;  // düğme yazısı (boşsa uygulanacak yama yok)
    Patch patch;
};

// Önerileri öncelik sırasıyla döndürür (en önemlisi başta). Ses iş parçacığında ÇAĞIRMAYIN.
inline std::vector<Advice> advise(const Params& p, const Stats& s) {
    std::vector<Advice> out;
    const SourceProfile& sp = profile(p.source);
    const Effective e = resolve(p);
    const float slope = 1.0f - 1.0f / e.ratio;
    const float rr = e.ratio / std::max(e.ratio - 1.0f, 0.05f);  // R/(R-1)

    auto add = [&](int id, Severity sev, const char* msg, const char* label, const Patch& patch) {
        Advice a;
        a.ruleId = id;
        a.severity = sev;
        a.message = msg;
        a.applyLabel = label;
        a.patch = patch;
        out.push_back(a);
    };

    // ---- parametreye dayalı kurallar (yalnızca gelişmiş modda anlamlı)
    if (p.advanced) {
        const bool transientSrc = (p.source == Source::Kick || p.source == Source::DrumBus);
        if (transientSrc && p.attackMs < 3.0f) {
            Patch pt; pt.mask = kPAttack;
            pt.values.attackMs = (p.source == Source::Kick) ? 25.f : 30.f;
            add(kRuleFastAttack, Severity::Warning,
                "Çok hızlı yakalıyorum, vuruşun yumruğunu yutuyorum. Biraz yavaşlatalım mı?",
                "Evet, yavaşlat", pt);
        }
        const bool lowSrc = (p.source == Source::Bass || p.source == Source::MixBus ||
                             p.source == Source::DrumBus);
        if (lowSrc && p.releaseMs < 30.0f) {
            Patch pt; pt.mask = kPRelease;
            pt.values.releaseMs = (p.source == Source::MixBus) ? 200.f : 100.f;
            add(kRuleFastRelease, Severity::Warning,
                "Çok hızlı bırakıyorum, bas titreyip bozulabilir. Biraz daha yavaş bırakayım mı?",
                "Evet, yavaşlat", pt);
        }
        const bool limitish = (p.ratio >= 10.0f) &&
            (p.source == Source::Vocal || p.source == Source::Synth || p.source == Source::MixBus);
        if (limitish) {
            Patch pt; pt.mask = kPRatio;
            pt.values.ratio = (p.source == Source::MixBus) ? 2.0f : 4.0f;
            add(kRuleLimiterRatio, Severity::Tip,
                "Bu oran sıkıştırıcıdan çok limiter gibi çalışıyor. Daha doğal bir oran deneyelim mi?",
                "Evet, deneyelim", pt);
        } else if (p.source == Source::MixBus && p.ratio > 3.0f) {
            Patch pt; pt.mask = kPRatio; pt.values.ratio = 2.0f;
            add(kRuleLimiterRatio, Severity::Tip,
                "Master'da oran yüksek. 2'ye 1 çok daha doğal yapışır, deneyelim mi?",
                "Evet, deneyelim", pt);
        }
        if (p.source == Source::Vocal && p.kneeDb < 1.0f && p.ratio >= 4.0f) {
            Patch pt; pt.mask = kPKnee; pt.values.kneeDb = 8.0f;
            add(kRuleHardKnee, Severity::Tip,
                "Bu keskin geçiş vokalde fark edilir. Daha yumuşak bir geçiş deneyelim mi?",
                "Evet, yumuşat", pt);
        }
        if ((p.source == Source::MixBus || p.source == Source::DrumBus) && p.scHpfHz < 40.0f) {
            Patch pt; pt.mask = kPHpf;
            pt.values.scHpfHz = (p.source == Source::MixBus) ? 90.f : 80.f;
            add(kRuleLowFreq, Severity::Tip,
                "Düşük frekanslar beni gereksiz tetikliyor. Yan zincir filtresini yükseltelim mi?",
                "Evet, yükselt", pt);
        }
    }

    // ---- ölçüme dayalı kurallar
    if (s.valid) {
        if (s.outPeakDb > -0.1f) {
            Patch pt; pt.mask = kPOutTrim;
            pt.values.outTrimDb = clampf(p.outTrimDb - clampf(s.outPeakDb + 1.0f, 1.0f, 12.0f), -24.f, 12.f);
            add(kRuleClip, Severity::Warning,
                "Dikkat, çıkış 0 dB'ye çarpıyor, ses bozulabilir. Çıkışı biraz kısayım mı?",
                "Evet, kıs", pt);
        }

        const bool tooMuch = (s.avgGrDb > sp.goodMaxGr + 1.0f) || (s.maxGrDb > 20.0f && s.avgGrDb > sp.goodMaxGr * 0.7f);
        if (tooMuch) {
            Patch pt;
            if (p.advanced) {
                pt.mask = kPThreshold;
                const float excess = std::max(s.avgGrDb - sp.goodMaxGr * 0.8f, 1.0f);
                pt.values.thresholdDb = clampf(p.thresholdDb + excess * rr, -70.f, 0.f);
            } else {
                pt.mask = kPAmount;
                const float ratioRed = sp.goodMaxGr * 0.8f / std::max(s.avgGrDb, 0.1f);
                pt.values.amount = clampf(p.amount * std::pow(clampf(ratioRed, 0.2f, 0.95f), 1.0f / 1.2f), 5.f, 100.f);
            }
            add(kRuleTooMuch, Severity::Warning,
                "Hmm, çok sert sıkıştırıyoruz, müzik nefes alamıyor. Biraz gevşeteyim mi?",
                "Evet, gevşet", pt);
        }

        const bool idle = (s.avgGrDb < 0.3f && s.maxGrDb < 1.0f);
        if (idle && (p.advanced || p.amount >= 20.0f)) {
            Patch pt;
            if (p.advanced) {
                pt.mask = kPThreshold;
                pt.values.thresholdDb = clampf(s.inPeakDb - 4.0f * rr, -70.f, 0.f);
            } else {
                pt.mask = kPAmount;
                pt.values.amount = clampf(p.amount + 25.f, 0.f, 100.f);
            }
            add(kRuleIdle, Severity::Tip,
                "Neredeyse hiç dokunamıyorum. Biraz daha güç vereyim mi?",
                "Evet, artır", pt);
        }

        if (p.advanced && !p.autoMakeup && s.loudDiffDb > 2.5f) {
            Patch pt; pt.mask = kPAutoMakeup; pt.values.autoMakeup = true;
            add(kRuleLoudnessIllusion, Severity::Tip,
                "Çıkış girişten daha yüksek, bu yüzden daha iyi duyuluyor olabilir. Adil karşılaştırma için seviyeyi eşitleyeyim mi?",
                "Evet, eşitle", pt);
        }

        const bool parallelSrc = (p.source == Source::Kick || p.source == Source::DrumBus ||
                                  p.source == Source::General);
        if (parallelSrc && s.avgGrDb > 7.0f && p.mixPercent >= 99.0f) {
            Patch pt; pt.mask = kPMix; pt.values.mixPercent = 50.0f;
            add(kRuleParallel, Severity::Tip,
                "Epey sıkıştırdık ama karışım %100. Orijinalin yarısını karıştırırsak hem güç hem canlılık kalır. Deneyelim mi?",
                "Evet, deneyelim", pt);
        }
    }

    // ---- öncelik: Uyarı > İpucu > Bilgi
    std::stable_sort(out.begin(), out.end(), [](const Advice& a, const Advice& b) {
        return static_cast<int>(a.severity) > static_cast<int>(b.severity);
    });

    if (out.empty() && s.valid) {
        Advice a;
        a.ruleId = kRuleAllGood;
        a.severity = Severity::Info;
        a.message = "Ayarlar harika görünüyor. Aynen devam!";
        out.push_back(a);
    }
    (void)slope;
    return out;
}

// ---------------------------------------------------------------- Jack: ruh hali ve ses
enum class Mood : int { Sleepy = 0, Relaxed, Working, Strained, Overwhelmed, Worried };

inline Mood moodFor(float grDb, bool signalPresent, bool hasActiveAdvice) {
    if (!signalPresent) return Mood::Sleepy;
    if (hasActiveAdvice) return Mood::Worried;
    if (grDb < 0.5f) return Mood::Relaxed;
    if (grDb < 6.0f) return Mood::Working;
    if (grDb < 12.0f) return Mood::Strained;
    return Mood::Overwhelmed;
}

inline const char* moodLine(Mood m) {
    switch (m) {
        case Mood::Sleepy:       return "Zzz... Ses gelince uyanırım.";
        case Mood::Relaxed:      return "Şu an dinleniyorum. Ses yükselirse devreye girerim.";
        case Mood::Working:      return "Tam kararında. Ses yerine oturuyor.";
        case Mood::Strained:     return "Biraz zorlanıyorum. Böyle devam edersek nefes alamaz.";
        case Mood::Overwhelmed:  return "Çok bastırıyorum! Bir bakalım, ayarı gevşetebilir miyiz?";
        case Mood::Worried:      return "";
    }
    return "";
}

inline const char* sourceLine(Source s) {
    switch (s) {
        case Source::General: return "Genel mod. Ne olduğunu bilmiyorum ama dengeli giderim.";
        case Source::Kick:    return "Kick! Vuruşu korurum, sadece kuyruğunu toparlarım.";
        case Source::Bass:    return "Bas sağlam olsun. Ağır ağır, nazikçe sıkıştırırım.";
        case Source::Vocal:   return "Vokal tatlı olsun. Cümlelerin seviyesini eşitliyorum.";
        case Source::Synth:   return "Lead'i yerine oturtalım, ama canlılığı kaybetmeden.";
        case Source::DrumBus: return "Davullar birbirine yapışsın. Bu bana göre iş!";
        case Source::MixBus:  return "Master'da incelikle: sadece 1-3 dB. Çok az, ama çok etkili.";
        default: return "";
    }
}

inline const char* greetingLine() {
    return "Selam, ben Jack! Önce kaynağı seç, sonra SIKIŞTIR düğmesini yavaş yavaş çevir. Gerisini bana bırak.";
}
inline const char* appliedLine() { return "Tamam! Şimdi çok daha doğal."; }
inline const char* declinedLine() { return "Peki, sen bilirsin. İstersen sonra tekrar bakarız."; }

}  // namespace jack
