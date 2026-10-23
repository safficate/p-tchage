# Pitchmap Pro 3.1.0 — doğrulama ve sınırlar

2026-10-23 UTC. Windows x64 PE VST3, clang 19 / Windows SDK 10.0.26100 / statik MSVC çalışma zamanı ile üretildi. **Güncel Sonar, gerçek Windows masaüstü ve hedef HP laptop test edilmedi.** Windows çalıştırmaları Linux üzerindeki Wine 10 altındadır. Bu rapor Sonar sertifikası değildir; yayıncı dijital imzası yoktur.

## Çalışan kapsam

Yeni örneklerde canlı kaynak/hedef arayüzü, 12 nota sınıfına bütün oktavlarda ortak hedef, ESITLE, genel cent/nota taşıma, zaman kayıtlı öğrenme, tekrar sayacı şişirmeme, seçilebilir merkez ve örnekler arası referans haritası vardır. Parametre değişimi canlı DSP'yi hemen etkiler. Tam parça işaretleme ağır adaptif analiz ve gerçek PCM hazırlığı yapar; hazır ses sonraki transport başlangıcı/seek sırasında alınır. Manuel canlı geçiş/gözlem/eğim ayarları, Humanize, merkez daraltma ve eşzamanlı host referansı canlı yol ve DAW bounce kullanır; bu kontrolleri uygulamayan bir önbellek onların önüne geçmez.

Kaynak ve hedef satırlarında Dinle düğmeleri gerçek sinüs tonu üretir. Kısa tıklama ~0,5 s, basılı tutma daha uzun, bırakma yumuşak sonlanır. Okunmamış kaynak önceki tonu durdurur ve ölçülmüş gibi yeni ses çalmaz. Ton testleri kaynak +10 c ve hedef +5 c + genel 20 c için çıkış frekansını bağımsız sıfır geçişleriyle 0,1 c toleransında ölçer. Bu sinüs dinleme tonunun ölçümüdür; müzikal pitch-shift kalitesi değildir.

## Testler

| Kontrol | Sonuç / kapsam |
|---|---|
| Native Linux CTest | 8/8: motor, bölgesel ses, durum/playback, gerçek editör boyama, eski klip iş akışı, ARA mock transferi, yeni smoother/workspace, canlı işlem |
| Son Windows DLL | GetPluginFactory yükleme ve 47 gerçek DLL ses vakası; 44,1/48 kHz, gerçek zamanlı/offline, hedef, genel nota/cent, referans/sidechain |
| Yeni workspace/smoother | 135 kontrol; kısa/uzun ton, gerçek frekans, bilinmeyen kaynak, tekrar histogramı, seyrek nota, referans kalıcılığı, manuel kaydırma, dry/bypass, gerçek filtre enerjisi, tam-parça callback çıkışı, aynı durumu mevcut örneğe geri yükleme ve host reprepare |
| Callback tahsisleri | Yeni test yolunda ölçülen C++ new/new[] tahsisi: 0; işletim sistemi zamanlayıcısı veya tüm olası host davranışları için sertifika değildir |
| Windows durum/canlı testleri | Proje kaynak/harita/parametre dönüşü ve gerçek playback; 248 canlı ses kontrolü |
| Korunan eski işleme testleri | İlk 3.1 Windows geçişinde motor 82, bölgesel 13.080, iş akışı 41, ARA 44 kontrol geçti. Son durum/canlı/DLL/workspace kontrolleri son binary ile yeniden çalıştırıldı. Native CTest bütün grupları son kaynakla sınar. |
| Arayüz | Native Linux yazılımsal boyama ve %100–200 ölçek kontrolü; Windows GUI ve Sonar penceresi doğrulanmadı |

Kontrol sayıları bağımsız kayıt sayısı değildir. Özellikle bölgesel testte tekrarlı örnek eşitliği kontrolleri vardır. Gerçek Windows komutlarının `%errorlevel%` değeri hemen .code dosyasına yazıldı; Wine sarmalayıcısının dış dönüşü başarı kanıtı olarak kullanılmadı. Son DLL hash'i BUILD-MANIFEST.json içinde bulunur.

Kontrollü sabit tonlarda D2/D3/D4/D5/D6, kaynak +12/−7 c → hedef +5 c, iki örnekleme hızında sınandı. En büyük ölçülen hedef farkı yaklaşık **0,887 c**. Ölçüm, pitch dedektöründen ayrı dar bant spektral uyumlamadır. Bu kapı başlangıç ve geçişler bittikten sonraki kararlı örneği değerlendirir; her müzikal an için aynı tolerans iddiası değildir.

## Gerçek kayıtlar

MTG/UPF sms-tools-materials kaynaklı, CC BY 4.0 lisanslı iki kısa tek-nota kaydı işlendi. Kaynak, lisans bağlantısı, hash ve değişiklik atfı ses karşılaştırma ZIP'inde tutulur. Keman ve flüt örneklerinin her birinde 142 güvenilir eşleşen perde karesi ölçüldü. İşlemde rezonans smoother ve son filtre kapalıdır.

| Kayıt / yol | İstenen taşıma | Ölçülen medyan taşıma | Perde konturu korelasyonu |
|---|---:|---:|---:|
| Keman / canlı | −20 c | −19,738 c | 0,9949 |
| Keman / canlı | +5 c | +5,273 c | 0,9939 |
| Keman / canlı | +20 c | +22,060 c | 0,9940 |
| Keman / hazır bölgesel | −20 c | −22,997 c | 0,9976 |
| Keman / hazır bölgesel | +5 c | +5,271 c | 0,9938 |
| Keman / hazır bölgesel | +20 c | +20,121 c | 0,9901 |
| Flüt / canlı | −20 c | −18,924 c | 0,9965 |
| Flüt / canlı | +5 c | +5,569 c | 0,9969 |
| Flüt / canlı | +20 c | +21,303 c | 0,9940 |

Bu kayıtlarda hata kontrollü bir-cent kapısından daha büyüktür: özellikle canlı +20 c keman ve hazır −20 c keman hedefe bir cent içinde değildir. Gerçek kayıt sonuçları bir-cent doğrulaması diye sunulmaz. Kayıt testi ±3 c medyan ve >0,8 kontur korelasyonu araştırma kapısını geçmiştir; bu daha geniş kapı kusursuz ses anlamına gelmez. Tam cümle, hızlı süsleme, çoklu enstrüman ve repertuvar değerlendirmesinin yerine geçmez.

Soprano-E4.wav kısa kaydında yalnız 10 yüksek güvenli ortak kare bulundu; 30'dan fazla kare gerektiren ölçüm kapısı **geçmedi**. Bu test başarısı olarak sayılmadı ve vokal doğrulandı denmedi. Gitar/piyano için denenmiş dosya URL'leri 404 verdi; test edilmiş gibi listelenmedi.

Perde konturunun benzerliği işitsel doğallık puanı değildir. Metalik kalıntı, yay sürtünmesi, atak bulaşması ve 'yeniden çalınmış gibi' sonuç için insan dinleme değerlendirmesi yapılmadı. Artefaktsızlık garantisi yoktur. Ayrı dinleme ZIP'i kullanıcının doğrudan A/B incelemesi içindir.

## İncelemede düzeltilen sorunlar

- Büyük öğrenme kuyruğu stack'ten heap'e alındı; Windows'ta çoklu işlemci testindeki stack overflow sonrası durum ve canlı testleri yeniden geçti.
- Kaynak sesin gerçekten belleğe yazılması ve hazırlanmış PCM'nin callback'ten duyulması ayrı sınandı; yalnız 'hazır' bayrağı yeterli sayılmadı.
- Modern kararlı-perde ince ayarı daha uzun/sıkı kararlılık kanıtı ister; vibratolu kemanda geri beslemenin ifadeyi izlemesi azaltıldı. Kararlı ton hassasiyeti yeniden sınandı.
- Aynı parametreli proje yüklemede kaybolan hazırlık işi/dirty durumu, host yeniden hazırlaması ve iptal edilen bellek işinin takılı bayrağı düzeltildi.
- Kaynak tekrarının ve offline kaynak analizinin öğrenilmiş host referansını silmesi düzeltildi; ayrı zaman indeksli referans geçmişi kaydedilir.

## Açık kalanlar

Bağımsız polifonik ayırma, öğrenilmiş genel çalgı tanıma, garanti edilmiş tını koruma ve sinirsel yeniden icra yoktur. Çalgı türüne özel kusursuzluk doğrulanmadı. Canlı doğal davranış sinyal sezgiselleridir; hazır bölgesel yol farklı bir ağır analiz/plan kullanır. Bunların aynı sonucu verdiği iddia edilmez.

OpenAI API bağlantısı, sınırlı parametre komutları ve kullanıcı tarafından başlatılan 12 saniyelik işlenmiş çıkış gönderimi uygulanmıştır. API anahtarı bulunmadığından gerçek servis/model görüşmesi **test edilmedi**. Bu sohbet oturumuna doğrudan bağlantı yoktur; teknik rapor kopyalanabilir. AI yorumları test veya dinleme kanıtı değildir.

Gerçek Sonar kabul testi için SONAR-TEST.md kullanılmalı: tarama, açılış, playback, hızlı/offline render, yeniden yükleme, dahili sürücü ve iki örnekleme hızı. Sonar'da sonuca ulaşılmadan uyumluluk doğrulanmış sayılmaz.
