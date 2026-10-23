# Cakewalk Sonar — 3.1.0 kabul testi

**Bu test henüz gerçek Sonar'da yapılmadı.** Windows DLL'nin Wine altında yüklenmesi ve ses üretmesi Sonar doğrulaması değildir. Buradaki liste prosedürdür; gerçekleşmiş sonuç değildir.

Test tarihindeki en güncel kararlı Sonar'ın tam sürüm/build numarasını, tarihi, Windows build'ini, HP EliteBook / 11. nesil i5 / 16 GB bilgisini, dahili ses sürücüsü modunu ve buffer boyutunu kaydedin. Harici ses kartı veya MIDI gerekmez. Her adımı hem 44,1 hem 48 kHz projede sınayın.

1. ZIP'teki VST3 klasörünün tamamını standart VST3 yoluna kopyalayın; taramayı yenileyin. Mono/stereo kanala ekleyin. Tarama, açılış ve editörü kapatıp açma sonucunu ayrı kaydedin.
2. Arayüz ölçeğini %100/125/150/175/200 deneyin; görünmeyen alt alanlara kaydırarak ulaşılmalı. Pencereyi ve projeyi yeniden açınca ölçek korunmalı. Zaman/perde zoom ile iki yönde gezinmeyi sınayın.
3. Kanalı Sonar'dan çalın. WAV/Capture/Apply gerekmeden üst kaynak değerleri ve zaman çizgisi oluşmalı. Aynı aralığı yeniden çalınca toplam gözlem sayısı şişmemeli. Seyrek nota yok sayılmamalı; okunmamış kaynak işaretli ama elle yazılabilir olmalı.
4. ESITLE ile merkezleri hedeflere alın. Kontrollü A +10 c sinyalinde alt A hedefini +5 c girin; %100 miktarda çıkış +5 c'ye yaklaşmalı. +3/+5/+10 ve negatif hedefleri farklı oktavlarda ölçün. Büyük nota eşlemesini ayrıca C→D olarak sınayın. Gerçek kayıt toleransını kontrollü sinüs toleransından ayrı raporlayın.
5. Üst Dinle ölçülen/elle tanımlanan kaynak merkezine akortlu tonu, alt Dinle toplam hedefe akortlu tonu çalmalı. Kısa tıklama yaklaşık 0,5 s, basılı tutma daha uzun olmalı. Okunmamış üst tuş önceki tonu sürdürmemeli ve yeni sahte kaynak tonu çalmamalı.
6. Genel +20 c'nin hedef kutularına eklenmesini ve ikinci kez uygulanmamasını ölçün. %50 düzeltme miktarını Dry/Wet'ten ayrı sınayın. Bypass ve %0 Wet, raporlanan gecikmeden sonra orijinal sesi vermeli. Normal FX gecikmesi 150 ms; host telafisini de kontrol edin.
7. Tüm aralığı dinletip TUM PARCA BURADA deyin. Hazır mesajından sonra transport başlangıcı/seek ile hazırlanmış ses devreye girmeli. Eksik duyulmuş boşluk tamamlanmış gibi kabul edilmemeli. Sonar hızlı/offline bounce ile gerçek çıkışı alın; yalnız grafiğin değişmesi yeterli değildir.
8. Doğal düzeltme, geçiş ayarları, vibrato, ayrı Humanize, rezonans smoother ve yerel filtreyi tek tek aç/kapatıp karşılaştırın. İşlemci komutu ile bağımsız çıkış ölçümünü ayırın. Humanize/merkez daraltma/canlı referans için canlı yolun bounce edildiği açık olmalı. Kontrollerin doğallığını gerçek keman, vokal ve farklı ataklı kayıtlarda dinleyerek değerlendirin.
9. İki kanala eklenti koyun. Birinin öğrendiği haritayı diğerinde referans seçin; önce toplu merkez sonra nota farklarını sınayın. Kaynak tekrarında/analizinde referans haritası silinmemeli. Ayrı süreç sandbox'ı varsa host sidechain yönlendirmesini ayrıca sınayın.
10. Projeyi kaydedip Sonar'ı tamamen kapatın; yeniden açın. Parametreler, haritalar, manuel bölgeler, öğrenilmiş referanslar ve tam-parça hazır ses geri gelmeli. Aynı durumun var olan örneğe tekrar yüklenmesini ve örnekleme hızı değişimini sınayın. API anahtarı projede bulunmamalı.
11. Yeni klip/ses değiştirme sonrası BASTAN OKU ile eski çizgilerin ve hazır sesin karışmadığını kontrol edin. Bounce sonucunu giriş/çıkış dosyaları ve tam ayarlarla saklayın. AI sohbetini yalnız ayrı API anahtarıyla ve kullanıcı tarafından başlatılan veri gönderimiyle sınayın; servis hatalarını başarı diye saymayın.

Her adım için geçti/kaldı/ölçülemedi, hata metni ve dosya adı yazın. Kullanıcı dinleme değerlendirmesi yapılmadan 'gıcırtısız, vızırtısız, yeniden çalınmış gibi' sonucu doğrulanmış sayılmaz. Bağımsız polifonik düzeltme bu sürümün kapsamı değildir.
