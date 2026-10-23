# Pitchmap Pro — kullanım

## Kurulum

Sonar'ı kapatın. ZIP'i açın; içindeki **Microtonal Pitch Map Pro.vst3 klasörünün tamamını** `C:\Program Files\Common Files\VST3\` konumuna kopyalayın. Yalnız içteki aynı adlı dosyayı taşımayın. Sonar'ı açıp VST3 taramasını yenileyin. Aynı eklentinin eski kopyalarıyla çakışmaması için tek sürüm kullanın. Paket bir kurulum EXE'si gerektirmez; dijital imzalı yayıncı sertifikası yoktur.

Sonar'ın güncel sürümü ve gerçek Windows masaüstü bu ortamda test edilmedi. Pakette Windows x64 PE VST3 bulunur; Wine testleri Sonar testi değildir.

## İlk kullanım

3.1.1 ilk açılışta İngilizcedir. Sağ üst dil menüsünden Türkçe seçebilirsiniz. İngilizce MATCH, Türkçe EŞİTLE düğmesiyle aynı işlemdir. Dil seçimi örnek bazında projeye kaydedilir; ayrıntılar LANGUAGES-3.1.1-TR.md içindedir.

1. Eklentiyi düzeltilecek ses kanalına ekleyin. Sonar'dan çalın; ayrı Play, WAV açma veya Capture gerekmez. Normal kanal efekti henüz üzerinden geçmemiş klipleri kendiliğinden alamaz.
2. Üstte KAYNAKTA OLCULEN satırına okunan notaların cent merkezleri ve gözlem oranları gelir. Okunmayan nota işaretlidir ama elle düzenlenebilir. Çok seyrek güvenilir nota da sayılır. Bu oranlar karar notasını kesin olarak belirlemez; merkez seçimi kullanıcıdadır.
3. **ESITLE** ölçülen/elle tanımlanan merkezleri alt hedeflere kopyalar. Bu aşamada kaynağı olduğu gibi hedeflemiş olursunuz. Ardından alt düğmelerden birini çevirin veya sayısına tıklayıp cent girin. İşlem anlık duyulur; Uygula/onay adımı yoktur.
4. Örnek: kaynak Do −15 c, hedef Do −10 c ise istenen taşıma +5 c'dir. Kaynak salınımları korunarak merkezi taşınır; her anı düz bir çizgiye sabitlemek farklı bir işlemdir. **Düzeltme miktarı %100** tam farkı ister; %50 yarısını ister. Dry/Wet bu hesap değil, kuru/işlenmiş ses karışımıdır.
5. **TUMUNU KAYDIR** genel cent ve nota kaydırmasını ekler. Genel +20 c, Do hedefi −15 c ise hedef kutusu +5 c gösterir; DSP bunu iki kez eklemez. Do→Re için Do sütununun nota menüsünden Re seçin. İnce cent kutusu seçilen notanın centidir.
6. Üstteki küçük **Dinle** kaynak merkezine, alttaki **Dinle** hesaplanan hedefe akortlu sinüs tonu çalar. Kısa tıklama yaklaşık 0,5 saniye; basılı tutma daha uzundur. Bunlar kayıttan kesit çalmaz. Okunmamış kaynağa sahte ölçüm tonu verilmez; isterseniz önce elle tanımlayın. Host ses motorunun çalışması gerekir.
7. Aynı yere dönmek gözlemleri çoğaltmaz. Kanalın içeriğini değiştirdiyseniz **BASTAN OKU** kullanın. Öğrenilmiş zaman çizgisi ve ayarlar projeyle saklanır.
8. Bütün aralık gerçekten çalındıysa **TUM PARCA BURADA** deyin. Ağır bölgesel analiz ve ses hazırlığı arka planda yapılır. Hazır bildirimi sonrası baştan oynatın veya konum değiştirin. Kaynağın duyulmamış boşluğu varsa hazırlık reddedilir. Normal hedef değişimi hemen canlı yoldan duyulur; güncellenmiş hazır ses sonraki başlangıçta alınır.
9. **Bypass** ve **Dry/Wet** ile karşılaştırın. Sonar üzerinden Bounce/Render alın. İfade sapması, merkez daraltma veya canlı host referansı açıksa hazırlanmış önbellek yerine canlı DSP yolu bounce edilir; durum yazısı bunu bildirir.

## Referans ve gezinme

İkinci kanala aynı eklentiyi ekleyin, kanalını çaldırıp ölçmesini sağlayın. İlk eklentinin referans listesinden ikinci örneği seçin. Bu paylaşım aynı host süreci içindedir. DAW eklentileri ayrı süreçlerde çalıştırıyorsa host sidechain yönlendirmesi gerekir. Kaynak sesi ikinci kanala aktarılmaz; öğrenilmiş merkezler paylaşılır. Merkez hizalaması için hedef notaların üstündeki merkez seçimini ve referans hizalama düğmesini kullanın. Karar otomatik ve kesin bulunmuş sayılmaz.

Grafikte sarı kaynak, mavi planlanan hedef, varsa yeşil host referansıdır. Mavi çizgi bağımsız ölçülmüş çıkış değildir. Teker zaman yakınlaştırır; Ctrl+teker perde yakınlaştırır, Shift+teker sağ/sol, Alt+teker yukarı/aşağı gezdirir. İki kaydırma çubuğu da vardır. Bölgesel sürükleme yerel cent düzeltmesi ekler. Üstte %100–200 arayüz ölçeği tüm ekranı büyütür; grafiğin yakınlaştırması ayrıdır.

## Ek ayarlar ve AI

**Detaylar** içinde Perde/Humanize, Tını/Rezonans ve Yerel Onarım ayrı bölümlerdir. Her işlem kendi aç/kapa ve varsa otomatik anahtarına sahiptir. Tını ve son filtre başlangıçta kapalıdır; çekirdek perde hatasını gizlemek için zorunlu uygulanmaz. Humanize isteğe bağlı sınırlı sapma ekler; açıkken hedefe birebir sabit cent beklenmez. Otomatik davranış müzikal kusursuzluk garantisi değildir.

AI sohbet için ayrı OpenAI API anahtarı gerekir; ChatGPT aboneliği bu anahtar yerine geçmez. **SOR** analiz/ayar metnini gönderir. **SESLE SOR** başlatırsanız çalan işlenmiş çıkıştan 12 saniye kaydedilip API'ye gönderilir. Otomatik ses yükleme yoktur. Anahtar projeye yazılmaz. Parametre komutları sayısal izin listesiyle sınırlıdır. API anahtarı olmadığı için gerçek servis görüşmesi doğrulanmadı.

**RAPORU KOPYALA** ile teknik metni bu sohbete yapıştırabilirsiniz. Eklenti bu sohbetin ajanına doğrudan bağlanmaz. AI yorumu gerçek Sonar testi veya insan dinleme değerlendirmesi yerine geçmez.

Bağımsız akor notalarını/polifonik çalgıları ayırma, genel çalgı tanıma ve yapay zekayla yeniden icra bu sürümde yoktur. Gerçek kayıt doğallığını kendi malzemenizde dinleyerek değerlendirin; sayısal cent testi bunu tek başına göstermez.

Canlı perde davranışının gözlem/geçiş/eğim değerleri varsayılandan elle değiştirilirse veya perde smoother/otomatiği kapatılırsa hazır bölgesel önbellek devreden çıkar. Bu kontroller canlı DSP üzerinde hemen duyulur ve Sonar bounce bu yolu alır. Tam-parça motoru varsayılan otomatik bölge planını kullanır; elle değiştirilen canlı kontrolü uygulamadığı halde uygulanmış gibi göstermez.
