# Jack Compressor

Müzik prodüksiyonu için akıllı kompresör. İçinde **Jack** adında bir karakter yaşar. Kompresör bilmeyen biri de kullanabilir; yanlış ayar yapıldığında Jack durumu söyler ve düzeltmeyi önerir.

## Nasıl çalışır (kullanıcı gözüyle)
1. **Bu ne?** Kaynağı seç: Genel, Kick, Bas, Vokal, Synth/Lead, Davul Bus, Mix Bus.
2. **Sıkıştır** düğmesini yavaş yavaş çevir. Eşik, oran, attack, release ve makeup Jack'te otomatiktir.
3. **His:** Yumuşak, Dengeli, Vurucu.
4. Jack ölçer (2 saniyelik pencere). Çok sert sıkıştırma, hiç dokunmama, çıkışta clip, vuruşu yutan attack, titreyen bas release'i, limiter gibi oran vb. durumlarda konuşur ve **"Evet, gevşet"** gibi tek tıkla düzeltir. **"Böyle kalsın"** der ve o uyarıyı 90 sn susturur.
5. **Gelişmiş** panelde bilenler tüm parametreleri elle ayarlar. Jack yine izler.

**Jack sese tepki verir:** gövdesi sıkıştırdıkça ezilir, ruh haline göre yüzü değişir, kolları iner ya da kalkar (uyurken sarkık, çalışırken müziğe göre sallanır, zorlanırken başının üstünde tavanı iter, bunalınca kollarını savurur, endişelenince elleri yüzüne gider). Ağzı sesle birlikte açılıp kapanır, vuruşla zıplar. Demoda fare/parmak yönüne bakar ve gövdesi ışıklandırmalı 3B görünümlüdür.

Karakter detayları: `docs/KARAKTER.md`

## Klasörler
| Yol | İçerik |
|---|---|
| `core/JackCore.h` | DSP + öneri motoru + Jack'in sözleri (tek başlık dosyası, C++17) |
| `core/test_core.cpp` | 11 test grubu, 42 kontrol |
| `plugin/` | JUCE 8.0.4 VST3 kaynak kodu (CMake) |
| `demo/jack-demo.html` | Tarayıcıda oynanabilir demo (çekirdeğin JS karşılığı) |
| `demo/parity.cpp`, `parity.js` | C++ ile JS aynı sinyalde karşılaştırma |
| `.github/workflows/build.yml` | GitHub'da Windows + macOS VST3 derlemesi |
| `docs/TEST-RAPORU.txt` | Test çıktısı |

## Derleme (geliştirici bilmeyenler için)
1. GitHub'da boş bir depo aç, bu klasörün içeriğini yükle.
2. **Actions** sekmesinde "Jack Compressor derleme" iş akışını çalıştır.
3. Bitince **Artifacts** altından Windows ve macOS `.vst3` dosyalarını indir.
4. Windows: `C:\Program Files\Common Files\VST3\` içine kopyala. FL Studio'da eklenti taramasını çalıştır.
   macOS: `/Library/Audio/Plug-Ins/VST3/`.

Yerelde: `cmake -S plugin -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`

## Dürüst test durumu
| Konu | Durum |
|---|---|
| DSP çekirdeği (C++) | **Test edildi:** 42/42 geçti, ASan/UBSan temiz |
| Öneri motoru kuralları | **Test edildi:** her kural için tetikleme ve çözüm ölçüldü |
| JS demo ↔ C++ çekirdek | **Ölçüldü:** aynı sinyalde GR farkı ≤ 0,01 dB |
| JUCE eklenti kaynağı (kollu Jack dahil) | Yalnızca sözdizimi denetimi (`g++ -fsyntax-only`). **Bağlanmadı, derlenip çalıştırılmadı.** |
| VST3 ikilisi | **Henüz üretilmedi.** GitHub Actions iş akışı yazıldı ama hiç çalıştırılmadı |
| FL Studio ve diğer DAW'lar | **Test edilmedi** |
| AU / AAX / CLAP | Yok (şimdilik yalnızca VST3) |

İlk gerçek derlemede küçük hatalar çıkabilir; hata metnini paylaşırsan düzeltilir.

## Satmadan önce kontrol et
- **JUCE lisansı:** JUCE 8 AGPLv3 veya ücretli ticari lisansla gelir. Kapalı kaynak satacaksan ticari lisans gerekir; açık kaynak yayınlarsan AGPL şartlarına uyman gerekir. Satıştan önce juce.com lisans sayfasını oku.
- **VST3:** Steinberg'in VST3 lisans koşulları geçerli (JUCE içinde gelir).
- **Marka:** "Jack" adı ve karakter için kullanılabilirlik/marka araması yap. Bilinen markalarla çakışmasın.
- Karakter ve ses tamamen özgündür; başka eklentinin görselini ya da kodunu kullanmaz.
