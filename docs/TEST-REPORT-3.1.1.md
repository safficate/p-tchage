# Pitchmap Pro 3.1.1 — dil güncellemesi doğrulaması

2026-10-23 UTC. Bu sürüm 3.1.0 üzerine arayüz dili ekler; perde işleme algoritması değiştirilmedi. Windows x64 PE VST3, clang 19 / Windows SDK 10.0.26100 / statik MSVC çalışma zamanı ile üretildi. Gerçek Windows, Sonar ve hedef HP laptop test edilmedi. Windows çalıştırmaları Linux üzerinde Wine 10 altındadır; yayıncı dijital imzası yoktur.

## Tamamlanan kontroller

- Native Linux CTest: 9/9 grup geçti. Önceki sekiz motor/iş akışı/arayüz grubuna interface_localization eklendi. Seçili menü metninin anında yenilenmesi düzeltildikten sonra bütün gruplar tekrar geçti; ekran yakalamada doğru ayrıntı sekmesinin seçildiği son test de ayrıca geçti.
- Dil kataloğu: 181 kayıt × 9 dil; boş/bozuk UTF-8 metin ve sayısal şablonlar kontrol edildi. Standart nota/simge/birimler ve bilinmeyen kullanıcı/kanal metni korunur.
- Gerçek editör: dokuz dilin menüden seçilmesi, düğme ve seçili açılır menü başlıklarının hemen değişmesi, örneklerin birbirinden bağımsızlığı, ilk açılışta İngilizce, proje kaydından dil dönüşü ve dil parametresi olmayan eski durumun İngilizceye dönmesi sınandı.
- Dil değiştirme sırasında 44,1 ve 48 kHz'de iki işlemcinin çıkışı örnek örnek karşılaştırıldı: azami fark 0. Dil değişimi ses parametrelerini değiştirmedi.
- Fontlar: gömülü Latin alt kümede 533 gerekli arayüz/Latin karakteri; Çince alt kümede katalogdaki 360, Arapçada 44 farklı karakter eksiksiz. Wine ekranında sistem Latin fontunun boş çizilmesi görülünce Latin font da gömüldü; ana başlığın gerçekten görünür piksel ürettiği testi eklendi. Arapça bağlamsal birleşme gerçek JUCE TextLayout ile sınandı. Dört Arapça cümle için alt kümenin şekillendirme/konum sonucu orijinal fontla ayrıca karşılaştırıldı.
- Native editörden İngilizce/Çince/Arapça için ana ekran, perde, tını, onarım, sohbet ve geçmiş olmak üzere 18 PNG üretildi. Bunlar tasarım çizimi değil, gerçek bileşen boyamasıdır. Pencerelerin Sonar içinde nasıl göründüğünü kanıtlamaz.
- Son Windows DLL: GetPluginFactory ve 47 gerçek DLL ses vakası geçti; 44,1/48 kHz, canlı/offline, hedef cent, nota taşıma ve referans/sidechain yolları.
- Son Windows durum/canlı testleri geçti; canlı grupta 248 kontrol. Smoother/workspace grubunda 135 kontrol geçti; test yolundaki C++ callback new/new[] tahsis sayısı 0.

Windows dil/arayüz testi de 4.583 kontrolle geçti. İngilizce/Çince/Arapça için aynı 18 ekran Wine altında Windows çalıştırmasından ayrıca üretildi; gerçek Sonar penceresi testi değildir. Native dil testinde 4.583 kontrol vardır; sayı çoğunlukla katalog/glyph doğrulamalarıdır, 4.583 ayrı müzik kaydı değildir. Komutların gerçek errorlevel değerleri .code dosyalarına hemen yazılır; Wine sarmalayıcısının dış dönüşü başarı kanıtı olarak kullanılmaz.

## Kapsam ve sınırlar

Dil seçimi yeni canlı çalışma arayüzüne aittir; v1–v5 eski proje editörünün eski iş akışı korunur. Kullanıcı/kanal/model isimleri ve geçmiş sohbet içeriği otomatik çevrilmez. Yeni AI isteği seçilen dilde yanıt ister; API anahtarı olmadığı için gerçek servis görüşmesi doğrulanmadı.

Çince gömülü font arayüz kataloğunun karakterlerini kapsar. Katalog dışı kullanıcı metni için işletim sistemi font desteği gerekebilir. Arapça yazının yönü grafiğin zaman eksenini ters çevirmez. Font kaynakları, hash değerleri, OFL lisansları ve alt küme üretim betikleri kaynak paketinde bulunur.

Bu güncelleme bağımsız polifonik nota ayırma veya sinirsel yeniden icra eklemez. Doğal, gıcırtısız ve insan yeniden çalmış gibi sonuç garantisi verilmez. Önceki gerçek keman/flüt kayıt ölçümlerinin sapmaları ve vokal testinin yetersiz kalması TEST-REPORT-3.1.md içinde aynen korunur; bu sürüm için yeni insan dinleme veya repertuvar testi yapıldığı iddia edilmez.

Son DLL hash'i ve gerçek test kayıtları validation/3.1.1/ ile BUILD-MANIFEST.json içindedir. Gerçek Sonar kabul testi SONAR-TEST.md protokolüyle kullanıcı bilgisayarında yapılmalıdır.
