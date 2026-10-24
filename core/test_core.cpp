// test_core.cpp
// JackCore.h için ölçüm tabanlı testler.
// Derle:  g++ -std=c++17 -O2 -Wall -Wextra -o test_core test_core.cpp && ./test_core
//
// Not: Bu testler DSP çekirdeğinin matematiksel davranışını ölçer. Bir DAW içinde
// çalışma testi DEĞİLDİR (bkz. README "Test durumu").

#include "JackCore.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace jack;

static int g_pass = 0, g_fail = 0;
static const double FS = 48000.0;
static const double kPi = 3.14159265358979323846;

#define CHECK(cond, ...)                         \
    do {                                         \
        if (cond) { ++g_pass; std::printf("  [OK]    "); } \
        else      { ++g_fail; std::printf("  [HATA]  "); } \
        std::printf(__VA_ARGS__);                \
        std::printf("\n");                       \
    } while (0)

// -------------------------------------------------------------- sinyal üreteçleri
static uint32_t g_rng = 12345u;
static float frand() {  // [-1, 1]
    g_rng = g_rng * 1664525u + 1013904223u;
    return static_cast<float>((g_rng >> 8) & 0xFFFF) / 32768.0f - 1.0f;
}

struct Sine {
    double f, amp;
    float operator()(double t) const { return static_cast<float>(amp * std::sin(2.0 * kPi * f * t)); }
};

static double dbAmp(double db) { return std::pow(10.0, db / 20.0); }

// kick + hat: 120 BPM davul benzeri
struct Drums {
    double kickPhase = 0.0;
    float operator()(double t) {
        const double bt = std::fmod(t, 0.5);
        const double kf = 45.0 + 90.0 * std::exp(-bt / 0.025);
        kickPhase += 2.0 * kPi * kf / FS;
        const double kick = 0.9 * std::exp(-bt / 0.12) * std::sin(kickPhase);
        const double ht = std::fmod(t + 0.125, 0.25);
        const double hat = 0.25 * std::exp(-ht / 0.02) * frand();
        return static_cast<float>(kick + hat);
    }
};

template <class Gen>
static void run(Compressor& c, double& t, double sec, Gen& gen, int block = 256) {
    int n = static_cast<int>(sec * FS);
    std::vector<float> L(block), R(block);
    while (n > 0) {
        const int m = std::min(n, block);
        for (int i = 0; i < m; ++i) {
            const float v = gen(t);
            L[i] = R[i] = v;
            t += 1.0 / FS;
        }
        float* ch[2] = {L.data(), R.data()};
        c.process(ch, 2, m);
        n -= m;
    }
}

// settle saniye ısındır, sonra measure saniye ölç, istatistiği döndür
template <class Gen>
static Stats measure(Compressor& c, Gen& gen, double settle, double measureSec) {
    double t = 0.0;
    run(c, t, settle, gen);
    c.flushStats();
    run(c, t, measureSec, gen);
    c.flushStats();
    return c.getStats();
}

static Params advBase() {
    Params p;
    p.advanced = true;
    p.thresholdDb = -20.f;
    p.ratio = 4.f;
    p.attackMs = 1.f;
    p.releaseMs = 50.f;
    p.kneeDb = 0.f;
    p.scHpfHz = 10.f;
    p.autoMakeup = false;
    p.makeupDb = 0.f;
    p.programRelease = false;
    p.mixPercent = 100.f;
    p.outTrimDb = 0.f;
    return p;
}

static Compressor make(const Params& p) {
    Compressor c;
    c.prepare(FS);
    c.setParams(p);
    c.reset();
    return c;
}

// ======================================================================= TESTLER

static void testStaticCurve() {
    std::printf("\n[1] Statik kazanç eğrisi (1 kHz sinüs, T=-20 dB, oran 4:1, sert diz)\n");
    const double peaks[] = {-20.0, -14.0, -8.0, -2.0};
    for (double pk : peaks) {
        Params p = advBase();
        Compressor c = make(p);
        Sine s{1000.0, dbAmp(pk)};
        const Stats st = measure(c, s, 1.5, 1.0);
        const double measured = -st.loudDiffDb;
        const double expected = std::max(0.0, (pk - (-20.0)) * (1.0 - 1.0 / 4.0));
        CHECK(std::fabs(measured - expected) < 0.5,
              "tepe %.0f dB: ölçülen GR %.2f dB, beklenen %.2f dB", pk, measured, expected);
    }
    Params p = advBase();
    p.kneeDb = 6.f;
    Compressor c = make(p);
    Sine s{1000.0, dbAmp(-20.0)};
    const Stats st = measure(c, s, 1.5, 1.0);
    CHECK(std::fabs(-st.loudDiffDb - 0.5625) < 0.3,
          "yumuşak diz (6 dB), seviye=eşik: GR %.2f dB (teori ~0.56)", -st.loudDiffDb);
}

static void testTiming() {
    std::printf("\n[2] Attack / release süresi (63%% noktası)\n");
    Params p = advBase();
    p.thresholdDb = -30.f;
    p.ratio = 100.f;
    p.attackMs = 20.f;
    p.releaseMs = 200.f;
    Compressor c = make(p);
    double t = 0.0;
    Sine quiet{1000.0, dbAmp(-40.0)}, loud{1000.0, dbAmp(-10.0)};
    run(c, t, 0.3, quiet);

    // attack: tek tek örnek işle
    std::vector<float> gr;
    {
        float l = 0.f, r = 0.f;
        float* ch[2] = {&l, &r};
        for (int i = 0; i < static_cast<int>(1.0 * FS); ++i) {
            l = r = loud(t);
            t += 1.0 / FS;
            c.process(ch, 2, 1);
            gr.push_back(c.currentGrDb());
        }
    }
    const float finalGr = gr.back();
    int idx = 0;
    while (idx < static_cast<int>(gr.size()) && gr[idx] < 0.632f * finalGr) ++idx;
    const double attMs = 1000.0 * idx / FS;
    CHECK(attMs > 15.0 && attMs < 25.0, "attack: 63%% noktası %.1f ms (ayar 20 ms)", attMs);

    // release
    gr.clear();
    {
        float l = 0.f, r = 0.f;
        float* ch[2] = {&l, &r};
        for (int i = 0; i < static_cast<int>(1.5 * FS); ++i) {
            l = r = quiet(t);
            t += 1.0 / FS;
            c.process(ch, 2, 1);
            gr.push_back(c.currentGrDb());
        }
    }
    idx = 0;
    while (idx < static_cast<int>(gr.size()) && gr[idx] > 0.368f * finalGr) ++idx;
    const double relMs = 1000.0 * idx / FS;
    CHECK(relMs > 150.0 && relMs < 250.0, "release: %%37 noktası %.1f ms (ayar 200 ms)", relMs);
}

static void testAutoThreshold() {
    std::printf("\n[3] Otomatik eşik: seviyeden bağımsız aynı sıkıştırma (Genel, SIKIŞTIR %%60)\n");
    Params p;
    p.source = Source::General;
    p.amount = 60.f;
    const double target = 10.0 * std::pow(0.6, 1.2);

    Compressor c = make(p);
    Sine hi{1000.0, dbAmp(-12.0)};
    Stats st = measure(c, hi, 10.0, 2.0);
    CHECK(std::fabs(st.avgGrDb - target) < 1.0,
          "-12 dB giriş: ortalama GR %.2f dB (hedef %.2f dB)", st.avgGrDb, target);

    Sine lo{1000.0, dbAmp(-30.0)};
    double t = 0.0;
    run(c, t, 25.0, lo);
    c.flushStats();
    run(c, t, 2.0, lo);
    c.flushStats();
    st = c.getStats();
    CHECK(std::fabs(st.avgGrDb - target) < 1.2,
          "giriş -12 dB'den -30 dB'ye düşünce: GR %.2f dB (hedef %.2f dB)", st.avgGrDb, target);

    const double amounts[] = {20.0, 100.0};
    for (double a : amounts) {
        Params q = p;
        q.amount = static_cast<float>(a);
        Compressor d = make(q);
        Sine s{1000.0, dbAmp(-9.0)};
        const Stats ss = measure(d, s, 10.0, 2.0);
        const double tg = 10.0 * std::pow(a / 100.0, 1.2);
        CHECK(std::fabs(ss.avgGrDb - tg) < 1.2, "SIKIŞTIR %%%.0f: GR %.2f dB (hedef %.2f dB)", a, ss.avgGrDb, tg);
    }
}

static void testAutoMakeup() {
    std::printf("\n[4] Otomatik makeup: giriş ve çıkış gürlüğü eşit kalmalı\n");
    Params p;
    p.source = Source::General;
    p.amount = 60.f;
    Compressor c = make(p);
    Sine s{1000.0, dbAmp(-12.0)};
    const Stats st = measure(c, s, 15.0, 2.0);
    CHECK(std::fabs(st.loudDiffDb) < 0.7, "ölçülen gürlük farkı %.2f dB (hedef 0)", st.loudDiffDb);
}

static void testMixAndBypass() {
    std::printf("\n[5] Karışım (parallel) ve SIKIŞTIR=0\n");
    {
        Params p = advBase();
        p.mixPercent = 0.f;
        p.thresholdDb = -40.f;
        Compressor c = make(p);
        std::vector<float> in(4800), L, R;
        for (auto& v : in) v = 0.5f * frand();
        L = in; R = in;
        float* ch[2] = {L.data(), R.data()};
        c.process(ch, 2, static_cast<int>(in.size()));
        float maxd = 0.f;
        for (size_t i = 0; i < in.size(); ++i) maxd = std::max(maxd, std::fabs(L[i] - in[i]));
        CHECK(maxd < 1.0e-5f, "mix=0: çıkış girişle aynı (en büyük fark %.2e)", maxd);
    }
    {
        Params p = advBase();
        p.mixPercent = 50.f;
        Compressor c = make(p);
        Sine s{1000.0, dbAmp(-8.0)};  // GR = 9 dB
        const Stats st = measure(c, s, 1.5, 1.0);
        const double expected = 20.0 * std::log10(0.5 + 0.5 * std::pow(10.0, -9.0 / 20.0));
        CHECK(std::fabs(st.loudDiffDb - expected) < 0.5, "mix=%%50: kazanç %.2f dB (beklenen %.2f dB)", st.loudDiffDb, expected);
    }
    {
        Params p;
        p.amount = 0.f;
        Compressor c = make(p);
        Drums d;
        const Stats st = measure(c, d, 2.0, 2.0);
        CHECK(std::fabs(st.loudDiffDb) < 0.05 && st.maxGrDb < 0.01,
              "SIKIŞTIR=0: şeffaf (gürlük farkı %.3f dB, maks GR %.3f dB)", st.loudDiffDb, st.maxGrDb);
    }
}

static void testRobustness() {
    std::printf("\n[6] Dayanıklılık: aşırı ayarlar, sessizlik, çok küçük/büyük sinyal\n");
    bool allFinite = true;
    float worst = 0.f;
    auto check = [&](Compressor& c, const std::function<float(double)>& gen, double sec) {
        double t = 0.0;
        std::vector<float> L(256), R(256);
        int n = static_cast<int>(sec * FS);
        while (n > 0) {
            const int m = std::min(n, 256);
            for (int i = 0; i < m; ++i) { L[i] = R[i] = gen(t); t += 1.0 / FS; }
            float* ch[2] = {L.data(), R.data()};
            c.process(ch, 2, m);
            for (int i = 0; i < m; ++i) {
                if (!std::isfinite(L[i]) || !std::isfinite(R[i])) allFinite = false;
                worst = std::max(worst, std::fabs(L[i]));
            }
            n -= m;
        }
    };
    Params ex = advBase();
    ex.ratio = 100.f; ex.attackMs = 0.05f; ex.releaseMs = 5.f; ex.kneeDb = 24.f;
    ex.thresholdDb = -70.f; ex.scHpfHz = 400.f; ex.makeupDb = 24.f; ex.autoMakeup = false;
    Compressor c1 = make(ex);
    check(c1, [](double) { return 0.9f * frand(); }, 2.0);
    Compressor c2 = make(ex);
    check(c2, [](double) { return 0.f; }, 1.0);
    Compressor c3 = make(ex);
    check(c3, [](double) { return 1.0e-30f * frand(); }, 1.0);
    Compressor c4 = make(ex);
    check(c4, [](double t) { return 50.f * static_cast<float>(std::sin(2 * kPi * 100 * t)); }, 1.0);
    for (int s = 0; s < static_cast<int>(Source::Count); ++s) {
        Params q;
        q.source = static_cast<Source>(s);
        q.amount = 100.f;
        q.feel = Feel::Punchy;
        Compressor c = make(q);
        Drums d;
        check(c, [&](double t) { return d(t); }, 2.0);
    }
    CHECK(allFinite, "tüm çıkışlar sonlu (NaN/Inf yok)");
    CHECK(worst < 1.0e5f, "çıkış patlamıyor (en büyük genlik %.1f)", worst);
}

static void testHpf() {
    std::printf("\n[7] Yan zincir filtresi: 40 Hz sinüs\n");
    Params p = advBase();
    p.thresholdDb = -30.f;
    p.scHpfHz = 10.f;
    Compressor c1 = make(p);
    Sine s{40.0, dbAmp(-10.0)};
    const Stats a = measure(c1, s, 1.5, 1.5);
    p.scHpfHz = 200.f;
    Compressor c2 = make(p);
    const Stats b = measure(c2, s, 1.5, 1.5);
    CHECK(a.avgGrDb > 8.0, "filtre 10 Hz: ortalama GR %.2f dB (sıkıştırıyor)", a.avgGrDb);
    CHECK(b.avgGrDb < 0.5, "filtre 200 Hz: ortalama GR %.2f dB (tetiklenmiyor)", b.avgGrDb);
}

static void testRipple() {
    std::printf("\n[8] Düşük frekansta kazanç dalgalanması (80 Hz sinüs)\n");
    Params p = advBase();
    p.thresholdDb = -30.f;
    p.attackMs = 15.f;
    p.releaseMs = 100.f;
    p.scHpfHz = 20.f;
    Compressor c = make(p);
    Sine s{80.0, dbAmp(-10.0)};
    double t = 0.0;
    run(c, t, 2.0, s);
    float lo = 1e9f, hi = -1e9f;
    float l = 0.f, r = 0.f;
    float* ch[2] = {&l, &r};
    for (int i = 0; i < static_cast<int>(0.5 * FS); ++i) {
        l = r = s(t); t += 1.0 / FS;
        c.process(ch, 2, 1);
        lo = std::min(lo, c.currentGrDb());
        hi = std::max(hi, c.currentGrDb());
    }
    CHECK(hi - lo < 1.5f, "GR dalgalanması %.2f dB tepeden tepeye (hedef < 1.5 dB)", hi - lo);
}

static void testProfiles() {
    std::printf("\n[9] Kaynak profilleri ve SIKIŞTIR eşlemesi\n");
    bool ok = true, mono = true, ranges = true;
    for (int s = 0; s < static_cast<int>(Source::Count); ++s) {
        float prevGr = -1.f, prevRatio = 0.f;
        for (int a = 0; a <= 100; a += 10) {
            Params p;
            p.source = static_cast<Source>(s);
            p.amount = static_cast<float>(a);
            const Effective e = resolve(p);
            const SourceProfile& sp = profile(p.source);
            if (e.targetGrDb < prevGr - 1e-4f || e.ratio < prevRatio - 1e-4f) mono = false;
            if (e.ratio < 1.49f || e.ratio > sp.ratioMax + 1e-3f) ranges = false;
            if (e.attackMs < 1.f || e.attackMs > 100.f || e.releaseMs < 20.f || e.releaseMs > 600.f) ranges = false;
            prevGr = e.targetGrDb;
            prevRatio = e.ratio;
        }
        Params z; z.source = static_cast<Source>(s); z.amount = 0.f;
        if (!resolve(z).bypassed) ok = false;
    }
    CHECK(ok, "SIKIŞTIR=0 her kaynakta bypass");
    CHECK(mono, "SIKIŞTIR arttıkça hedef GR ve oran azalmıyor (tüm kaynaklar)");
    CHECK(ranges, "oran, attack ve release makul aralıkta (tüm kaynaklar)");
}

// ------------------------------------------------------------------ öneri motoru
static bool hasRule(const std::vector<Advice>& v, int id) {
    for (const auto& a : v) if (a.ruleId == id) return true;
    return false;
}
static const Advice* findRule(const std::vector<Advice>& v, int id) {
    for (const auto& a : v) if (a.ruleId == id) return &a;
    return nullptr;
}

static Stats goodStats() {
    Stats s;
    s.valid = true; s.inPeakDb = -6.f; s.outPeakDb = -4.f;
    s.avgGrDb = 3.f; s.maxGrDb = 6.f; s.activeRatio = 0.6f; s.stuckRatio = 0.4f; s.loudDiffDb = 0.f;
    return s;
}

static void testAdvisorRules() {
    std::printf("\n[10] Öneri motoru: tek tek kurallar\n");
    {
        Params p; Stats s = goodStats();
        auto v = advise(p, s);
        CHECK(v.size() == 1 && v[0].ruleId == kRuleAllGood, "her şey iyiyken yalnızca 'harika' mesajı");
        CHECK(advise(p, Stats{}).empty(), "sinyal yokken (geçersiz istatistik) sessiz");
    }
    {
        Params p; p.amount = 80.f; Stats s = goodStats(); s.avgGrDb = 14.f; s.maxGrDb = 22.f;
        auto v = advise(p, s);
        const Advice* a = findRule(v, kRuleTooMuch);
        CHECK(a && a->severity == Severity::Warning && a->patch.values.amount < 80.f,
              "basit mod, GR 14 dB: SIKIŞTIR %.0f -> %.0f", p.amount, a ? a->patch.values.amount : -1.f);
    }
    {
        Params p = advBase(); p.thresholdDb = -18.f; p.ratio = 4.f; Stats s = goodStats(); s.avgGrDb = 14.f;
        auto v = advise(p, s);
        const Advice* a = findRule(v, kRuleTooMuch);
        CHECK(a && a->patch.values.thresholdDb > -18.f, "gelişmiş mod: eşik -18 -> %.1f dB (yukarı)",
              a ? a->patch.values.thresholdDb : -99.f);
    }
    {
        Params p; Stats s = goodStats(); s.outPeakDb = 1.2f;
        auto v = advise(p, s);
        const Advice* a = findRule(v, kRuleClip);
        CHECK(a && a->patch.values.outTrimDb < 0.f, "çıkış 0 dB üstü: çıkış %.1f dB'ye kısılıyor",
              a ? a->patch.values.outTrimDb : 0.f);
    }
    {
        Params p = advBase(); p.source = Source::Kick; p.attackMs = 1.f;
        auto v = advise(p, goodStats());
        const Advice* a = findRule(v, kRuleFastAttack);
        CHECK(a && std::fabs(a->patch.values.attackMs - 25.f) < 0.01f, "kick + 1 ms attack: öneri 25 ms");
    }
    {
        Params p = advBase(); p.source = Source::Bass; p.releaseMs = 10.f;
        CHECK(hasRule(advise(p, goodStats()), kRuleFastRelease), "bas + 10 ms release: uyarı");
    }
    {
        Params p = advBase(); p.source = Source::Vocal; p.ratio = 20.f;
        CHECK(hasRule(advise(p, goodStats()), kRuleLimiterRatio), "vokal + 20:1 oran: limiter uyarısı");
        p.ratio = 4.f; p.kneeDb = 0.f;
        CHECK(hasRule(advise(p, goodStats()), kRuleHardKnee), "vokal + sert diz: yumuşatma önerisi");
    }
    {
        Params p = advBase(); p.source = Source::MixBus; p.ratio = 6.f; p.scHpfHz = 20.f; p.releaseMs = 200.f; p.attackMs = 30.f;
        auto v = advise(p, goodStats());
        CHECK(hasRule(v, kRuleLimiterRatio) && hasRule(v, kRuleLowFreq), "mix bus: yüksek oran + düşük filtre");
    }
    {
        Params p = advBase(); p.autoMakeup = false; p.makeupDb = 9.f;
        Stats s = goodStats(); s.loudDiffDb = 5.f;
        CHECK(hasRule(advise(p, s), kRuleLoudnessIllusion), "manuel makeup ile +5 dB: adil karşılaştırma önerisi");
    }
    {
        Params p; p.source = Source::DrumBus; p.mixPercent = 100.f;
        Stats s = goodStats(); s.avgGrDb = 7.6f;
        CHECK(hasRule(advise(p, s), kRuleParallel), "davul bus, GR 7.6 dB, karışım %%100: paralel önerisi");
    }
    {
        Params p; p.amount = 50.f; Stats s = goodStats(); s.avgGrDb = 0.1f; s.maxGrDb = 0.4f;
        CHECK(hasRule(advise(p, s), kRuleIdle), "devreye girmiyorsa: güç artırma önerisi");
        p.amount = 10.f;
        CHECK(!hasRule(advise(p, s), kRuleIdle), "SIKIŞTIR çok düşükse (kullanıcı hafif istiyor): ısrar etmez");
    }
    {
        Params p = advBase(); p.source = Source::Kick; p.attackMs = 1.f;
        Stats s = goodStats(); s.outPeakDb = 2.f; s.avgGrDb = 14.f;
        auto v = advise(p, s);
        bool sorted = true;
        for (size_t i = 1; i < v.size(); ++i)
            if (static_cast<int>(v[i].severity) > static_cast<int>(v[i - 1].severity)) sorted = false;
        CHECK(v.size() >= 3 && sorted, "birden çok sorun: uyarılar ipuçlarından önce sıralanır (%zu öneri)", v.size());
    }
}

static void testClosedLoop() {
    std::printf("\n[11] Kapalı döngü: kötü ayar -> Jack öneriyor -> uygulanınca düzeliyor\n");
    // 1) çok düşük eşik
    {
        Params p = advBase();
        p.source = Source::General;
        p.thresholdDb = -45.f; p.ratio = 8.f; p.attackMs = 10.f; p.releaseMs = 100.f; p.scHpfHz = 60.f;
        p.autoMakeup = true;
        const float limit = profile(p.source).goodMaxGr + 1.0f;
        float avg = 0.f;
        int it = 0;
        std::printf("      başlangıç eşik %.0f dB\n", p.thresholdDb);
        for (; it < 5; ++it) {
            Compressor c = make(p);
            Drums d;
            const Stats st = measure(c, d, 4.0, 4.0);
            avg = st.avgGrDb;
            std::printf("      tur %d: eşik %.1f dB -> ortalama GR %.1f dB\n", it, p.thresholdDb, avg);
            auto v = advise(p, st);
            const Advice* a = findRule(v, kRuleTooMuch);
            if (!a) break;
            applyPatch(p, a->patch);
        }
        CHECK(avg <= limit, "çok düşük eşik: %d öneri sonra ortalama GR %.1f dB (sınır %.1f dB)", it, avg, limit);
    }
    // 2) eşik çok yüksek (hiçbir şey olmuyor)
    {
        Params p = advBase();
        p.source = Source::General; p.thresholdDb = -1.f; p.ratio = 4.f; p.attackMs = 10.f; p.releaseMs = 100.f;
        p.scHpfHz = 60.f; p.autoMakeup = true;
        Compressor c = make(p);
        Drums d;
        Stats st = measure(c, d, 2.0, 4.0);
        auto v = advise(p, st);
        const Advice* a = findRule(v, kRuleIdle);
        const float before = st.avgGrDb;
        bool fixed = false;
        if (a) {
            applyPatch(p, a->patch);
            Compressor c2 = make(p);
            Drums d2;
            st = measure(c2, d2, 4.0, 4.0);
            // Kural, yüksek tepelerde ~4 dB GR hedefler. 10 ms attack'lı kısa vuruşlarda yumuşatılmış
            // GR bunun altında kalır; burada ölçülen: kompresör artık duyulur biçimde devreye giriyor mu?
            fixed = (st.maxGrDb >= 1.5f && st.maxGrDb < 8.0f);
        }
        CHECK(a != nullptr && fixed, "eşik çok yüksek: tepe GR %.2f -> %.2f dB (öneri sonrası, kısa vuruşlarda ~4 dB hedefin altında kalır)", before, st.maxGrDb);
    }
    // 3) clip
    {
        Params p = advBase();
        p.thresholdDb = -10.f; p.ratio = 3.f; p.attackMs = 10.f; p.releaseMs = 100.f; p.scHpfHz = 60.f;
        p.autoMakeup = false; p.makeupDb = 12.f;
        Compressor c = make(p);
        Drums d;
        Stats st = measure(c, d, 2.0, 4.0);
        auto v = advise(p, st);
        const Advice* a = findRule(v, kRuleClip);
        const float before = st.outPeakDb;
        bool fixed = false;
        if (a) {
            applyPatch(p, a->patch);
            Compressor c2 = make(p);
            Drums d2;
            st = measure(c2, d2, 2.0, 4.0);
            fixed = st.outPeakDb <= -0.1f;
        }
        CHECK(a != nullptr && fixed, "makeup 12 dB clip: çıkış tepe %.1f -> %.1f dB (öneri sonrası)", before, st.outPeakDb);
    }
    // 4) basit mod her kaynakta "güvenli"
    {
        int problems = 0;
        for (int s = 0; s < static_cast<int>(Source::Count); ++s) {
            Params p;
            p.source = static_cast<Source>(s);
            p.amount = 100.f;
            Compressor c = make(p);
            Drums d;
            const Stats st = measure(c, d, 6.0, 4.0);
            auto v = advise(p, st);
            const bool bad = hasRule(v, kRuleTooMuch) || hasRule(v, kRuleClip);
            std::printf("      %-13s SIKIŞTIR %%100: ortalama GR %.1f dB, çıkış tepe %.1f dB%s\n",
                        profile(p.source).name, st.avgGrDb, st.outPeakDb, bad ? "  <-- uyarı" : "");
            if (bad) ++problems;
        }
        CHECK(problems == 0, "basit modda SIKIŞTIR %%100 bile hiçbir kaynakta TooMuch/Clip uyarısı vermiyor (%d sorun)", problems);
    }
}

int main() {
    std::printf("JackCore testleri (örnekleme hızı %.0f Hz)\n", FS);
    testStaticCurve();
    testTiming();
    testAutoThreshold();
    testAutoMakeup();
    testMixAndBypass();
    testRobustness();
    testHpf();
    testRipple();
    testProfiles();
    testAdvisorRules();
    testClosedLoop();
    std::printf("\nSonuç: %d başarılı, %d başarısız\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
