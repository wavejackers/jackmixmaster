// Jack çekirdeği (JackCore.h'nin JavaScript karşılığı). Aynı matematik, aynı sayılar.
const Jack = (() => {
  const T = (en, tr) => ({ en, tr });
  const pk = (v, lang) => (v && typeof v === 'object') ? (lang === 'tr' ? v.tr : v.en) : v;
  const dbToLin = db => Math.pow(10, db * 0.05);
  const linToDb = l => 20 * Math.log10(Math.max(l, 1e-9));
  const clampf = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const lerpf = (a, b, t) => a + (b - a) * t;

  const Source = { General: 0, Kick: 1, Bass: 2, Vocal: 3, Synth: 4, DrumBus: 5, MixBus: 6, Count: 7 };
  const Feel = { Soft: 0, Balanced: 1, Punchy: 2 };
  const PROFILES = [
    { name: 'General', nameTr: 'Genel',        ratioMax: 4.0, grMax: 10, attackMs: 15, releaseMs: 150, kneeDb: 6,  hpfHz: 60,  goodMaxGr: 8.0 },
    { name: 'Kick', nameTr: 'Kick',         ratioMax: 4.0, grMax: 10, attackMs: 25, releaseMs: 80,  kneeDb: 3,  hpfHz: 60,  goodMaxGr: 8.0 },
    { name: 'Bass', nameTr: 'Bas',          ratioMax: 4.0, grMax: 9,  attackMs: 20, releaseMs: 120, kneeDb: 6,  hpfHz: 40,  goodMaxGr: 7.0 },
    { name: 'Vocal', nameTr: 'Vokal',        ratioMax: 4.0, grMax: 10, attackMs: 8,  releaseMs: 120, kneeDb: 8,  hpfHz: 120, goodMaxGr: 8.0 },
    { name: 'Synth / Lead', nameTr: 'Synth / Lead', ratioMax: 3.5, grMax: 8,  attackMs: 12, releaseMs: 130, kneeDb: 6,  hpfHz: 80,  goodMaxGr: 7.0 },
    { name: 'Drum Bus', nameTr: 'Davul Bus',    ratioMax: 4.0, grMax: 8,  attackMs: 30, releaseMs: 100, kneeDb: 4,  hpfHz: 80,  goodMaxGr: 7.0 },
    { name: 'Mix Bus', nameTr: 'Mix Bus',      ratioMax: 2.0, grMax: 4,  attackMs: 30, releaseMs: 200, kneeDb: 10, hpfHz: 90,  goodMaxGr: 3.5 },
  ];
  const profile = s => PROFILES[Math.min(Math.max(s | 0, 0), Source.Count - 1)];

  const defaultParams = () => ({
    source: Source.General, amount: 40, feel: Feel.Balanced, mixPercent: 100, outTrimDb: 0,
    advanced: false, thresholdDb: -18, ratio: 3, attackMs: 10, releaseMs: 120, kneeDb: 6,
    scHpfHz: 80, makeupDb: 0, autoMakeup: true, programRelease: true,
  });

  function resolve(p) {
    const e = { bypassed: false, autoThreshold: true, manualThresholdDb: -18, targetGrDb: 3, ratio: 3,
      attackMs: 15, releaseMs: 150, kneeDb: 6, hpfHz: 60, autoMakeup: true, manualMakeupDb: 0,
      programRelease: true, mix: 1, trimDb: 0 };
    e.mix = clampf(p.mixPercent, 0, 100) * 0.01;
    e.trimDb = clampf(p.outTrimDb, -24, 12);
    if (p.advanced) {
      e.autoThreshold = false;
      e.manualThresholdDb = clampf(p.thresholdDb, -70, 0);
      e.ratio = clampf(p.ratio, 1, 100);
      e.attackMs = clampf(p.attackMs, 0.05, 300);
      e.releaseMs = clampf(p.releaseMs, 5, 3000);
      e.kneeDb = clampf(p.kneeDb, 0, 24);
      e.hpfHz = clampf(p.scHpfHz, 10, 400);
      e.autoMakeup = p.autoMakeup;
      e.manualMakeupDb = clampf(p.makeupDb, 0, 24);
      e.programRelease = p.programRelease;
      return e;
    }
    const sp = profile(p.source);
    const t = clampf(p.amount, 0, 100) * 0.01;
    e.bypassed = p.amount < 0.5;
    e.ratio = lerpf(1.5, sp.ratioMax, Math.pow(t, 0.8));
    e.targetGrDb = sp.grMax * Math.pow(t, 1.2);
    let am = 1, rm = 1;
    if (p.feel === Feel.Soft) { am = 0.6; rm = 1.4; }
    else if (p.feel === Feel.Punchy) { am = 1.8; rm = 0.7; }
    e.attackMs = sp.attackMs * am; e.releaseMs = sp.releaseMs * rm;
    e.kneeDb = sp.kneeDb; e.hpfHz = sp.hpfHz; e.autoMakeup = true; e.programRelease = true;
    return e;
  }

  class Biquad {
    constructor() { this.b0 = 1; this.b1 = 0; this.b2 = 0; this.a1 = 0; this.a2 = 0; this.z1 = 0; this.z2 = 0; }
    clear() { this.z1 = this.z2 = 0; }
    designHighpass(f, sr) {
      const w0 = 6.28318530718 * clampf(f, 5, sr * 0.45) / sr;
      const cs = Math.cos(w0), al = Math.sin(w0) / (2 * 0.70710678);
      const a0 = 1 + al;
      this.b0 = (1 + cs) * 0.5 / a0; this.b1 = -(1 + cs) / a0; this.b2 = this.b0;
      this.a1 = -2 * cs / a0; this.a2 = (1 - al) / a0;
    }
    process(x) {
      const y = this.b0 * x + this.z1;
      this.z1 = this.b1 * x - this.a1 * y + this.z2;
      this.z2 = this.b2 * x - this.a2 * y;
      return y;
    }
  }

  function computeGr(xDb, T, slope, W) {
    const over = xDb - T;
    if (W <= 0.01) return over > 0 ? over * slope : 0;
    if (2 * over < -W) return 0;
    if (2 * Math.abs(over) <= W) { const t = over + W * 0.5; return slope * t * t / (2 * W); }
    return over * slope;
  }

  class Compressor {
    constructor() {
      this.fs = 44100; this.params = defaultParams(); this.eff = resolve(this.params);
      this.hpf = []; for (let i = 0; i < 8; i++) this.hpf.push(new Biquad());
      this.hpfDesigned = -1; this.stats = { valid: false, inPeakDb: -120, outPeakDb: -120, avgGrDb: 0, maxGrDb: 0, activeRatio: 0, stuckRatio: 0, loudDiffDb: 0 };
    }
    prepare(sr) {
      const fs = this.fs = sr;
      this.aProgAtt = Math.exp(-1 / (0.020 * fs)); this.aProgRel = Math.exp(-1 / (3.0 * fs));
      this.aDetRel = Math.exp(-1 / (0.010 * fs)); this.aLoud = Math.exp(-1 / (0.4 * fs));
      this.aMakeup = Math.exp(-1 / (1.0 * fs)); this.aSmooth = Math.exp(-1 / (0.020 * fs));
      this.aPeakRel = Math.exp(-1 / (6.0 * fs)); this.windowLen = Math.floor(2 * fs);
      this.reset(); this.setParams(this.params);
    }
    reset() {
      this.hpf.forEach(b => b.clear());
      this.detEnv = 0; this.grState = 0; this.progLevelDb = -40; this.msIn = 0; this.msOutPre = 0;
      this.makeupAutoDb = 0; this.pkInEnv = 0; this.pkPreEnv = 0;
      this.mixS = this.eff.mix; this.trimS = dbToLin(this.eff.trimDb); this.manualMkS = dbToLin(this.eff.manualMakeupDb);
      this.curThresholdDb = -20; this.resetWindow();
      this.stats = { valid: false, inPeakDb: -120, outPeakDb: -120, avgGrDb: 0, maxGrDb: 0, activeRatio: 0, stuckRatio: 0, loudDiffDb: 0 };
    }
    setParams(p) {
      this.params = p; this.eff = resolve(p);
      this.attCoef = Math.exp(-1 / (this.eff.attackMs * 0.001 * this.fs));
      if (Math.abs(this.eff.hpfHz - this.hpfDesigned) > 0.01) {
        this.hpfDesigned = this.eff.hpfHz;
        this.hpf.forEach(b => b.designHighpass(this.hpfDesigned, this.fs));
      }
    }
    resetWindow() { this.winN = 0; this.activeN = 0; this.over05 = 0; this.over15 = 0; this.sumGrActive = 0; this.maxGr = 0; this.winInPk = 0; this.winOutPk = 0; this.sumSqIn = 0; this.sumSqOut = 0; }
    accumulate(inPk, outPk, sqIn, sqOut, gr, levelDb) {
      this.winN++; this.winInPk = Math.max(this.winInPk, inPk); this.winOutPk = Math.max(this.winOutPk, outPk);
      this.sumSqIn += sqIn; this.sumSqOut += sqOut;
      if (levelDb > -50) { this.activeN++; this.sumGrActive += gr; if (gr > 0.5) this.over05++; if (gr > 1.5) this.over15++; this.maxGr = Math.max(this.maxGr, gr); }
      if (this.winN >= this.windowLen) this.finalizeWindow();
    }
    finalizeWindow() {
      if (this.winN === 0) return;
      const s = { valid: this.activeN >= Math.floor(0.25 * this.fs), inPeakDb: linToDb(this.winInPk), outPeakDb: linToDb(this.winOutPk),
        avgGrDb: 0, maxGrDb: this.maxGr, activeRatio: 0, stuckRatio: 0, loudDiffDb: 0 };
      if (this.activeN > 0) { s.avgGrDb = this.sumGrActive / this.activeN; s.activeRatio = this.over05 / this.activeN; s.stuckRatio = this.over15 / this.activeN; }
      if (this.sumSqIn > 1e-12 && this.sumSqOut > 1e-12) s.loudDiffDb = 10 * Math.log10(this.sumSqOut / this.sumSqIn);
      this.stats = s; this.resetWindow();
    }
    flushStats() { this.finalizeWindow(); }
    currentGrDb() { return this.grState; }
    currentThresholdDb() { return this.curThresholdDb; }

    // ch: Float32Array dizisi, yerinde işler
    process(ch, n) {
      const numCh = Math.min(ch.length, 8), invCh = 1 / Math.max(numCh, 1), e = this.eff;
      const slopeDefault = 1 - 1 / e.ratio, mixTarget = e.mix, trimTarget = dbToLin(e.trimDb), mkTarget = dbToLin(e.manualMakeupDb);
      for (let i = 0; i < n; i++) {
        this.mixS = mixTarget + this.aSmooth * (this.mixS - mixTarget);
        this.trimS = trimTarget + this.aSmooth * (this.trimS - trimTarget);
        this.manualMkS = mkTarget + this.aSmooth * (this.manualMkS - mkTarget);
        let det = 0, inPk = 0, sqIn = 0;
        for (let c = 0; c < numCh; c++) {
          const x = ch[c][i]; inPk = Math.max(inPk, Math.abs(x)); sqIn += x * x;
          det = Math.max(det, Math.abs(this.hpf[c].process(x)));
        }
        sqIn *= invCh;
        this.detEnv = Math.max(det, this.detEnv * this.aDetRel);
        const levelDb = linToDb(this.detEnv);
        if (levelDb > -70) { const a = levelDb > this.progLevelDb ? this.aProgAtt : this.aProgRel; this.progLevelDb = levelDb + a * (this.progLevelDb - levelDb); }
        let T;
        if (e.autoThreshold) {
          if (e.targetGrDb < 0.01) T = 0;
          else { const over = e.targetGrDb * e.ratio / (e.ratio - 1); T = clampf(this.progLevelDb - over, -70, -1); }
        } else T = e.manualThresholdDb;
        this.curThresholdDb = T;
        const grTarget = e.bypassed ? 0 : computeGr(levelDb, T, slopeDefault, e.kneeDb);
        let coef;
        if (grTarget > this.grState) coef = this.attCoef;
        else {
          let relMs = e.releaseMs;
          if (e.programRelease) relMs *= 1 + 1.5 * clampf(this.grState * 0.1, 0, 1);
          coef = Math.exp(-1 / (relMs * 0.001 * this.fs));
        }
        this.grState = grTarget + coef * (this.grState - grTarget);
        if (this.grState < 1e-4 && grTarget === 0) this.grState = 0;
        const g = dbToLin(-this.grState);
        let sqPre = 0, pkPre = 0;
        for (let c = 0; c < numCh; c++) { const v = ch[c][i] * g; sqPre += v * v; pkPre = Math.max(pkPre, Math.abs(v)); }
        sqPre *= invCh;
        this.msIn = sqIn + this.aLoud * (this.msIn - sqIn);
        this.msOutPre = sqPre + this.aLoud * (this.msOutPre - sqPre);
        this.pkInEnv = Math.max(inPk, this.pkInEnv * this.aPeakRel);
        this.pkPreEnv = Math.max(pkPre, this.pkPreEnv * this.aPeakRel);
        let mkLin;
        if (e.autoMakeup) {
          if (this.msIn > 1e-9 && this.msOutPre > 1e-12) {
            const want = clampf(10 * Math.log10(this.msIn / this.msOutPre), 0, 12);
            this.makeupAutoDb = want + this.aMakeup * (this.makeupAutoDb - want);
          }
          const ceilDb = Math.max(-1, linToDb(this.pkInEnv));
          const headroom = Math.max(0, ceilDb - linToDb(this.pkPreEnv));
          mkLin = dbToLin(Math.min(this.makeupAutoDb, headroom));
        } else mkLin = this.manualMkS;
        let outPk = 0, sqOut = 0;
        for (let c = 0; c < numCh; c++) {
          const dry = ch[c][i], wet = dry * g * mkLin;
          const y = (dry * (1 - this.mixS) + wet * this.mixS) * this.trimS;
          ch[c][i] = y; outPk = Math.max(outPk, Math.abs(y)); sqOut += y * y;
        }
        sqOut *= invCh;
        this.accumulate(inPk, outPk, sqIn, sqOut, this.grState, levelDb);
      }
    }
  }

  // ---- öneri motoru
  const PF = { Amount: 1, Mix: 2, OutTrim: 4, Threshold: 8, Ratio: 16, Attack: 32, Release: 64, Knee: 128, Hpf: 256, AutoMakeup: 512 };
  function applyPatch(p, patch) {
    const v = patch.values, m = patch.mask;
    if (m & PF.Amount) p.amount = v.amount; if (m & PF.Mix) p.mixPercent = v.mixPercent;
    if (m & PF.OutTrim) p.outTrimDb = v.outTrimDb; if (m & PF.Threshold) p.thresholdDb = v.thresholdDb;
    if (m & PF.Ratio) p.ratio = v.ratio; if (m & PF.Attack) p.attackMs = v.attackMs;
    if (m & PF.Release) p.releaseMs = v.releaseMs; if (m & PF.Knee) p.kneeDb = v.kneeDb;
    if (m & PF.Hpf) p.scHpfHz = v.scHpfHz; if (m & PF.AutoMakeup) p.autoMakeup = v.autoMakeup;
  }
  const Sev = { Info: 0, Tip: 1, Warning: 2 };
  const Rule = { Clip: 1, TooMuch: 2, Idle: 3, FastAttack: 4, FastRelease: 5, LimiterRatio: 6, HardKnee: 7, LoudnessIllusion: 8, Parallel: 9, LowFreq: 10, AllGood: 11 };

  function advise(p, s, lang = 'en') {
    const out = [], sp = profile(p.source), e = resolve(p);
    const rr = e.ratio / Math.max(e.ratio - 1, 0.05);
    const add = (id, sev, msg, label, mask, vals) => out.push({ ruleId: id, severity: sev, message: pk(msg, lang), applyLabel: pk(label, lang), patch: { mask, values: vals } });
    if (p.advanced) {
      const S = Source;
      if ((p.source === S.Kick || p.source === S.DrumBus) && p.attackMs < 3)
        add(Rule.FastAttack, Sev.Warning, T("I'm grabbing too fast and swallowing the punch of the hit. Shall we slow the attack down a little?","Çok hızlı yakalıyorum, vuruşun yumruğunu yutuyorum. Biraz yavaşlatalım mı?"), T("Yes, slow it down","Evet, yavaşlat"), PF.Attack, { attackMs: p.source === S.Kick ? 25 : 30 });
      if ((p.source === S.Bass || p.source === S.MixBus || p.source === S.DrumBus) && p.releaseMs < 30)
        add(Rule.FastRelease, Sev.Warning, T("I'm letting go too fast, the bass can flutter and distort. Shall I release a bit slower?","Çok hızlı bırakıyorum, bas titreyip bozulabilir. Biraz daha yavaş bırakayım mı?"), T("Yes, slow it down","Evet, yavaşlat"), PF.Release, { releaseMs: p.source === S.MixBus ? 200 : 100 });
      const limitish = p.ratio >= 10 && (p.source === S.Vocal || p.source === S.Synth || p.source === S.MixBus);
      if (limitish) add(Rule.LimiterRatio, Sev.Tip, T("This ratio works more like a limiter than a compressor. Shall we try a more natural one?","Bu oran sıkıştırıcıdan çok limiter gibi çalışıyor. Daha doğal bir oran deneyelim mi?"), T("Yes, let's try","Evet, deneyelim"), PF.Ratio, { ratio: p.source === S.MixBus ? 2 : 4 });
      else if (p.source === S.MixBus && p.ratio > 3) add(Rule.LimiterRatio, Sev.Tip, T("The ratio is high for a master bus. 2:1 glues much more naturally. Shall we try it?","Master'da oran yüksek. 2'ye 1 çok daha doğal yapışır, deneyelim mi?"), T("Yes, let's try","Evet, deneyelim"), PF.Ratio, { ratio: 2 });
      if (p.source === S.Vocal && p.kneeDb < 1 && p.ratio >= 4)
        add(Rule.HardKnee, Sev.Tip, T("This sharp transition is audible on vocals. Shall we try a softer one?","Bu keskin geçiş vokalde fark edilir. Daha yumuşak bir geçiş deneyelim mi?"), T("Yes, soften it","Evet, yumuşat"), PF.Knee, { kneeDb: 8 });
      if ((p.source === S.MixBus || p.source === S.DrumBus) && p.scHpfHz < 40)
        add(Rule.LowFreq, Sev.Tip, T("Low frequencies are triggering me for no reason. Shall we raise the sidechain filter?","Düşük frekanslar beni gereksiz tetikliyor. Yan zincir filtresini yükseltelim mi?"), T("Yes, raise it","Evet, yükselt"), PF.Hpf, { scHpfHz: p.source === S.MixBus ? 90 : 80 });
    }
    if (s.valid) {
      if (s.outPeakDb > -0.1)
        add(Rule.Clip, Sev.Warning, T("Careful, the output is hitting 0 dB and may distort. Shall I turn the output down a bit?","Dikkat, çıkış 0 dB'ye çarpıyor, ses bozulabilir. Çıkışı biraz kısayım mı?"), T("Yes, turn it down","Evet, kıs"), PF.OutTrim, { outTrimDb: clampf(p.outTrimDb - clampf(s.outPeakDb + 1, 1, 12), -24, 12) });
      const tooMuch = s.avgGrDb > sp.goodMaxGr + 1 || (s.maxGrDb > 20 && s.avgGrDb > sp.goodMaxGr * 0.7);
      if (tooMuch) {
        let mask, vals;
        if (p.advanced) { mask = PF.Threshold; const ex = Math.max(s.avgGrDb - sp.goodMaxGr * 0.8, 1); vals = { thresholdDb: clampf(p.thresholdDb + ex * rr, -70, 0) }; }
        else { mask = PF.Amount; const rd = sp.goodMaxGr * 0.8 / Math.max(s.avgGrDb, 0.1); vals = { amount: clampf(p.amount * Math.pow(clampf(rd, 0.2, 0.95), 1 / 1.2), 5, 100) }; }
        add(Rule.TooMuch, Sev.Warning, T("Hmm, we're squeezing too hard and the music can't breathe. Shall I ease off a bit?","Hmm, çok sert sıkıştırıyoruz, müzik nefes alamıyor. Biraz gevşeteyim mi?"), T("Yes, ease off","Evet, gevşet"), mask, vals);
      }
      if (s.avgGrDb < 0.3 && s.maxGrDb < 1 && (p.advanced || p.amount >= 20)) {
        let mask, vals;
        if (p.advanced) { mask = PF.Threshold; vals = { thresholdDb: clampf(s.inPeakDb - 4 * rr, -70, 0) }; }
        else { mask = PF.Amount; vals = { amount: clampf(p.amount + 25, 0, 100) }; }
        add(Rule.Idle, Sev.Tip, T("I'm barely touching it. Shall I give it a bit more power?","Neredeyse hiç dokunamıyorum. Biraz daha güç vereyim mi?"), T("Yes, more","Evet, artır"), mask, vals);
      }
      if (p.advanced && !p.autoMakeup && s.loudDiffDb > 2.5)
        add(Rule.LoudnessIllusion, Sev.Tip, T("The output is louder than the input, so it may just sound better. Shall I match the levels for a fair comparison?","Çıkış girişten daha yüksek, bu yüzden daha iyi duyuluyor olabilir. Adil karşılaştırma için seviyeyi eşitleyeyim mi?"), T("Yes, match levels","Evet, eşitle"), PF.AutoMakeup, { autoMakeup: true });
      const parallelSrc = p.source === Source.Kick || p.source === Source.DrumBus || p.source === Source.General;
      if (parallelSrc && s.avgGrDb > 7 && p.mixPercent >= 99)
        add(Rule.Parallel, Sev.Tip, T("We compressed quite a bit but the mix is 100%. Blending in half of the original keeps both power and life. Try it?","Epey sıkıştırdık ama karışım %100. Orijinalin yarısını karıştırırsak hem güç hem canlılık kalır. Deneyelim mi?"), T("Yes, let's try","Evet, deneyelim"), PF.Mix, { mixPercent: 50 });
    }
    out.sort((a, b) => b.severity - a.severity); // Array.sort kararlı
    if (!out.length && s.valid) out.push({ ruleId: Rule.AllGood, severity: Sev.Info, message: T("Settings look great. Keep going!","Ayarlar harika görünüyor. Aynen devam!"), applyLabel: '', patch: { mask: 0, values: {} } });
    return out;
  }

  const Mood = { Sleepy: 0, Relaxed: 1, Working: 2, Strained: 3, Overwhelmed: 4, Worried: 5 };
  function moodFor(gr, sig, adv) {
    if (!sig) return Mood.Sleepy; if (adv) return Mood.Worried;
    if (gr < 0.5) return Mood.Relaxed; if (gr < 6) return Mood.Working; if (gr < 12) return Mood.Strained; return Mood.Overwhelmed;
  }
  const MOOD = [
    T('Zzz... I\'ll wake up when sound arrives.', 'Zzz... Ses gelince uyanırım.'),
    T('Resting for now. I\'ll step in if the sound gets louder.', 'Şu an dinleniyorum. Ses yükselirse devreye girerim.'),
    T('Just right. The sound is sitting in place.', 'Tam kararında. Ses yerine oturuyor.'),
    T('I\'m struggling a bit. If we keep this up, it can\'t breathe.', 'Biraz zorlanıyorum. Böyle devam edersek nefes alamaz.'),
    T('I\'m pushing too hard! Can we ease the setting?', 'Çok bastırıyorum! Bir bakalım, ayarı gevşetebilir miyiz?'),
    T('', '')];
  const moodLine = (m, lang = 'en') => pk(MOOD[m], lang);
  const SRC = [
    T("General mode. I don't know what this is, but I'll keep it balanced.", 'Genel mod. Ne olduğunu bilmiyorum ama dengeli giderim.'),
    T("Kick! I'll protect the punch and just tidy up the tail.", 'Kick! Vuruşu korurum, sadece kuyruğunu toparlarım.'),
    T('Let the bass stay solid. I\'ll compress slowly and gently.', 'Bas sağlam olsun. Ağır ağır, nazikçe sıkıştırırım.'),
    T("Let's keep the vocal sweet. I'm evening out the phrases.", 'Vokal tatlı olsun. Cümlelerin seviyesini eşitliyorum.'),
    T("Let's sit the lead in place without losing its life.", "Lead'i yerine oturtalım, ama canlılığı kaybetmeden."),
    T('Let the drums glue together. This is my kind of job!', 'Davullar birbirine yapışsın. Bu bana göre iş!'),
    T('Gently on the master: just 1-3 dB. Very little, very effective.', "Master'da incelikle: sadece 1-3 dB. Çok az, ama çok etkili.")];
  const sourceLine = (s, lang = 'en') => pk(SRC[s], lang) || '';
  const sourceName = (s, lang = 'en') => lang === 'tr' ? PROFILES[s].nameTr : PROFILES[s].name;
  const greetingLine = (lang = 'en') => pk(T('Hi, I\'m Jack! Pick the source first, then slowly turn the SQUEEZE knob. Leave the rest to me.', 'Selam, ben Jack! Önce kaynağı seç, sonra SIKIŞTIR düğmesini yavaş yavaş çevir. Gerisini bana bırak.'), lang);
  const appliedLine = (lang = 'en') => pk(T('Done! Much more natural now.', 'Tamam! Şimdi çok daha doğal.'), lang);
  const declinedLine = (lang = 'en') => pk(T('Okay, your call. We can look again later if you like.', 'Peki, sen bilirsin. İstersen sonra tekrar bakarız.'), lang);

  return { dbToLin, linToDb, clampf, Source, Feel, PROFILES, profile, defaultParams, resolve, Compressor, PF, applyPatch, Sev, Rule, advise, Mood, moodFor, moodLine, sourceLine, sourceName, greetingLine, appliedLine, declinedLine, T, pk };
})();
if (typeof module !== 'undefined') module.exports = Jack;
