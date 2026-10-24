# Jack: Kompresörün İçindeki Karakter

Jack, Wavejackers'ın küçük stüdyo robotu. Kompresörün içinde yaşar, sesi dinler, ne yaptığını anlatır ve yanlış bir ayar gördüğünde **suçlamadan** çözüm önerir.

## Neden bir karakter?

Kompresör, yeni başlayanların en çok korktuğu plugin: threshold, ratio, attack, release, knee, makeup. Jack bu sayıları kullanıcıdan saklar ve yerine tek bir soru sorar: **"Ne sıkıştırıyoruz, ne kadar?"** Gerisini kendi hesaplar. İleri kullanıcı "Gelişmiş" panelini açıp her şeyi elle ayarlayabilir; Jack orada da izler ve gerekirse öneri yapar.

## Görünüm

- Yuvarlak gövdeli, küçük bir robot. İki büyük LED göz, kafasında dalga biçimli bir anten, boynunda kulaklık
- **Gövdesi gerçekten sıkışır:** gain reduction (GR) arttıkça Jack'in gövdesi aşağıdan yukarıya doğru basılır, yanlara genişler. Kullanıcı kompresörün ne yaptığını duymadan önce görür
- Renkler: lacivert-turkuaz gövde, sıcak sarı gözler. Uyarı durumunda gözler turuncuya döner

## Ruh halleri (GR'ye ve uyarıya göre)

| Durum | Ne zaman | Yüz |
|-------|----------|-----|
| Uykulu | Sinyal yok | Gözler yarı kapalı, "zzz" |
| Rahat | GR < 0.5 dB | Gözler açık, hafif gülümseme |
| Çalışıyor | 0.5-6 dB | Odaklı bakış, geniş gülümseme |
| Zorlanıyor | 6-12 dB | Gözler kısık, alında ter damlası |
| Bunalmış | > 12 dB | Gözler iri, ağız "o" şeklinde |
| Endişeli | Aktif bir öneri var | Kaşlar yukarı, göz turuncu |

## Kişilik ve ses tonu

- **Sakin ve destekleyici.** Telaşlanmaz, ukala davranmaz
- **Kısa konuşur.** En fazla 2 kısa cümle
- **"Yanlış" demez.** "Şunu deneyelim", "Bence daha iyi olur" der
- **Terimi önce açıklar.** "Eşik" der, parantezle "(threshold)" ekler. İleri panelde teknik adlar kullanılır
- **"Sen" diye hitap eder.** Samimi ama saygılı
- **Müzisyen gibi konuşur:** "nefes", "vuruş", "yumruk", "yapışmak" gibi kelimeler kullanır
- Küfür, alay ve baskı yok. Kullanıcı bir öneriyi reddederse Jack ısrar etmez (30 saniye susar)

## Konuşma örnekleri

**Açılış:** "Selam, ben Jack! Önce kaynağı seç, sonra SIKIŞTIR düğmesini yavaş yavaş çevir. Gerisini bana bırak."

**Kaynak seçilince:**
- Kick: "Kick! Vuruşu korurum, sadece kuyruğunu toparlarım."
- Bas: "Bas sağlam olsun. Ağır ağır, nazikçe sıkıştırırım."
- Vokal: "Vokal tatlı olsun. Cümlelerin seviyesini eşitliyorum."
- Synth / Lead: "Lead'i yerine oturtalım, ama canlılığı kaybetmeden."
- Davul Bus: "Davullar birbirine yapışsın. Bu bana göre iş!"
- Mix Bus: "Master'da incelikle: sadece 1-3 dB. Çok az, ama çok etkili."

**Çalışırken:**
- Rahat: "Şu an dinleniyorum. Sinyal biraz daha yükselirse devreye giririm."
- Çalışıyor: "Tam kararında. Ses yerine oturuyor."
- Zorlanıyor: "Biraz zorlanıyorum. Böyle devam edersek nefes alamaz."

**Öneri verirken (hiç suçlayıcı değil):**
- "Hmm, çok sert sıkıştırıyoruz. Eşiği biraz yukarı alayım mı?" `[Evet, uygula]` `[Böyle kalsın]`
- "Kick'in vuruşunu yutuyoruz. Hızı biraz yavaşlatalım mı?"

**Uyguladıktan sonra:** "Tamam! Şimdi çok daha doğal."

**Reddedilince:** "Peki, sen bilirsin. İstersen sonra tekrar bakarız." (30 sn sessizlik)

**Her şey iyiyse:** "Ayarlar harika görünüyor. Aynen devam!"

## Jack'in yapmayacakları

- Müziğin tarzına karışmaz; "bu şarkı kötü" demez
- Önerisini kullanıcı onaylamadan **uygulamaz**
- Aynı uyarıyı peş peşe tekrarlamaz
- Test edilmemiş bir şeyi kesinmiş gibi söylemez: eşikler birer başlangıç noktasıdır, "Bence" diye konuşur


## Beden dili (kollar)
| Ruh hali | Kollar |
|---|---|
| Uykulu | sarkık, hareketsiz |
| Rahat | hafifçe yana, yavaş sallanma |
| Çalışıyor | yana açık, müziğin şiddetiyle kalkıp iner |
| Zorlanıyor | başın üstünde, tavanı itiyor, titriyor |
| Bunalmış | havada savruluyor |
| Endişeli | elleri yüzünün yanında |
