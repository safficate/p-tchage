# “Yeniden çalınmış gibi” kalite hedefi: ilk araştırma kararı

Tarih: 2026-10-21. Bu incelemede aşağıdaki projelerin birincil README ve LICENSE belgeleri indirildi ve okundu. Makalelerin tam metinleri, model ağırlıkları veya ses demoları bu incelemede değerlendirilmedi. Hiçbir aday model çalıştırılmadı; burada performans, dinleme veya Sonar testi sonucu yoktur.

## Başarı ölçütü

Kullanıcı düzeltilmiş bir kemanı dinlediğinde yalnız cent hatasının azalmasını değil, aynı müzisyenin o cümleyi doğru akortla yeniden çalmış olabileceği kadar doğal bir icra duymayı istiyor. Bu, bütün sesin yeni bir enstrüman örneğiyle değiştirilmesi talebi değildir. İcracı/enstrüman kimliği, tını, yay/nefes/mızrap davranışı, süslemeler ve müzikal geçişler korunacak; yalnız istenen hatalar değişecek.

Bu bir kalite hedefidir. Mevcut motorun veya herhangi bir araştırma modelinin bunu bütün çalgılarda sağladığı sonucu çıkarılamaz.

## Okunan kaynaklar ve taşıdıkları kanıt

| Kaynak | Belgede görülen yetenek | Bizim için anlamı ve sınırı |
| --- | --- | --- |
| [DDSP](https://github.com/magenta/ddsp) | Öğrenilen kontrolleri harmonik sentez ve efekt işlemcilerine bağlama; tını aktarımı ve kullanıcı verisiyle autoencoder eğitimi örnekleri | Perde ile harmonik/gürültü bileşenlerini ayrı kontrol etme adayı. Tını aktarımı demosu, kaynak kemanın kimliğini koruyan onarım veya bağımsız polifonik düzenleme kanıtı değildir. |
| [MIDI-DDSP](https://github.com/magenta/midi-ddsp) | Nota bazında volume, vol_fluc, vibrato, brightness, attack, vol_peak_pos kontrolleri; f0_hz, amplitudes, noise_magnitudes, harmonic_distribution üzerinden yeniden sentez | Kullanıcının “ajan nota üzerinde çalışsın” isteğine somut bir model örneği. Hazır model URMP'deki 13 çalgıyı kapsıyor; keman var, kanun/bağlama/piyano yok. MIDI'den örnek üretmek, herhangi bir kaydı aynı icracı kimliğiyle onarmak değildir. |
| [Basic Pitch](https://github.com/spotify/basic-pitch) | Çok sesli nota transkripsiyonu, pitch bend içeren MIDI; belgeye göre tek enstrümanla daha iyi sonuç | Nota/bölge adayları için karşılaştırılabilecek bir analiz yolu. Ses onarımı yapmaz; pitch bend çıktısı hassas cent doğruluğunu kanıtlamaz. Analiz için 22050 Hz'e örnekleme dönüştürür; özgün 44.1/48 kHz sesin bu yoldan yeniden üretilmesi gerekmiyor. |
| [MT3](https://github.com/magenta/mt3) | T5X tabanlı çok enstrümanlı müzik transkripsiyonu ve hazır checkpoint ile Colab örneği | Karşılaştırılacak olay/enstrüman analizi yaklaşımı. Kaynak ayırma, sürekli mikrotonal perde eğrisi ve sesin yeniden sentezi ayrıca çözülmelidir. |

MIDI-DDSP belgesi yaklaşık 18 saatlik tek RTX 8000 eğitiminden ve önerilen yüksek GPU belleğinden söz ediyor. Bunlar yazarların belirtilen eğitim koşullarıdır; bizim i5 laptopumuzda çıkarım maliyeti ölçümü değildir. Aynı belge keyfî veri kümesiyle eğitimin mevcut kodda desteklenmediğini belirtiyor. Bu nedenle “hazır modeli alıp bütün çalgıları hemen destekleriz” kararı verilmiyor.

Dört depodaki LICENSE metinleri Apache-2.0. Bu, ayrıca indirilecek ağırlıkların, veri kümelerinin ve diğer bağımlılıkların tüm dağıtım haklarını kendiliğinden doğrulamaz. Şimdilik yeni çalışma zamanı bağımlılığı veya ağırlık eklenmedi.

## Şimdiki mühendislik kararı

1. **Algılama, müzikal karar ve ses üretimini ayır.** Bir modelin nota çıkarması, o notayı doğal biçimde değiştirebildiği anlamına gelmesin. Ajan, doğrulanan nota/bölge işlemleri hazırlasın; ses üretimini uygun motor yapsın.
2. **Özgün kayıt üzerinde en az müdahaleyi başlangıç adayı tut.** Küçük akort düzeltmesi için bütün icrayı yeniden üretme. Kaynak eğrinin sabit taşınması, yerel biçimlendirme ve yeniden sentezi aynı kayıt üzerinde ayrı adaylar olarak karşılaştır.
3. **Yeniden sentezi yerel bir araştırma kolu olarak sınırla.** Mevcut sesin yeterince iyi düzeltilemediği bölgelere odaklan. Tını ve ifade kaybını azaltması deneyle gösterilmeden otomatik varsayılan motor yapma. Sınır birleştirmelerini, fazı, stereo alanı ve kuyrukları ayrıca kontrol et.
4. **Canlı yol ile ağır hazırlığı ayır.** Ağır polifonik analiz/yeniden sentez arka planda veya offline yapılsın. Hazırlanmış sonuçlar DAW zamanına hizalı önbellekten çalsın. Gerçek zamanlı preview yetenekleri ile offline kalite seçenekleri açıkça belirtilsin; hazırlık bittiği varsayılmasın.
5. **Her çalgıyı ayrı doğrula.** Solo keman ilk vaka olabilir; çift ses, unison topluluk ve sustainli piyano aynı başarı sayılmaz. Bir hazır modelin çalgı etiketi, kaynak icranın kimliğini koruduğunu kanıtlamaz.

Bu bir nihai motor seçimi değildir. Signalsmith mevcut karşılaştırma tabanıdır; DDSP/MIDI-DDSP araştırma adaylarıdır. Model karşılaştırması ve gerçek kayıtla dinleme olmadan mevcut motorun yerine seçilmezler.

## Kararı belirleyecek deney

- İzinli gerçek kayıtlardan tek keman için düz nota, vibrato, yay değişimi, süsleme ve portamento örnekleri oluştur. Sonra çift ses, topluluk ve diğer çalgıları ekle. Örnek kimliği, lisans, sürüm, hedef ve kullanılan işlem kaydedilsin.
- Doğru icraya bilinen perde hataları ekleyip geri düzeltme testi yap; özgün iyi kayıt karşılaştırma referansı olsun. Hata ekleme işleminin kendi artefaktları da kontrol koşuluyla ölçülsün. Buna ek olarak gerçek hatalı icra ve ayrı referans icra kullan; bu iki kayıt örnek örnek aynı dalga biçimi gibi karşılaştırılmasın.
- Aynı hedef üzerinde kaynak üzerinde kaydırma, eğri düzenleme ve yerel yeniden sentez adaylarını karşılaştır. Cent ölçümünü tek motorun kendi dedektörüne dayandırma; güvenilir bölgelerde bağımsız analizle doğrula.
- Yanlış/eksik/fazladan nota, oktav takibi, hedef merkez hatası, vibrato genişliği/hızı, glide sürekliliği, atak/ku­yruk ve stereo değişimleri kaydedilsin. Spektral fark tek başına iyi/kötü tını kararı vermesin.
- Rastgele sıralanmış, yöntem adı gizlenmiş dinleme örneklerinde doğallık, çalgı/icracı kimliği, süslemelerin korunması ve işlem duyulurluğu ayrı değerlendirilsin. Tercih testi ile duyulabilir fark testi birbirine karıştırılmasın. Genel bir ses kalitesi puanı tek başına yeterli değildir.
- Laptop üzerinde hazırlık süresi, peak RAM, playback CPU ve dropout ölçülsün. Ses kalitesindeki kazanç performans maliyetiyle birlikte raporlansın. Güncel Sonar'da yerel test ayrıca gereklidir.

Sonraki karar gerçek ses çıktıları ve bu deney sonuçlarıyla verilecek. Bugünkü inceleme, teknik adayları ve sınırlarını belirledi; “yeniden çalınmış gibi” kaliteyi henüz göstermedi.

## Kanıt dosyaları

İndirilen belgeler ve ilgili lisanslar [research/expressive-repair](../research/expressive-repair/) altında. [retrieval.json](../research/expressive-repair/retrieval.json) erişilen URL'leri, tarih, boyut ve SHA-256 değerlerini kaydeder. Dosyalar hareketli `main` dalından indirilen belge anlık görüntüleridir; sabit bir kod commit'i veya tekrar üretilebilir model çalışma ortamı değildir.
