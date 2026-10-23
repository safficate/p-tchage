# 3.1: perde, ifade ve isteğe bağlı cilalama

Bu sürümün mühendislik konusu, bölgesel perde taşımanın ürettiği yapay geçişleri azaltmak ve kullanıcının bütün ek işlemleri ayrı karşılaştırabilmesidir. Kayda önceden girmiş gürültünün restorasyonu farklı bir görevdir. Bu belge uygulanmış işlemleri araştırma adaylarından ayırır; geniş kapsamlı insan doğallığı veya bütün çalgılarda kusursuz sonuç sertifikası değildir.

## Araştırma ve seçim

Kaynak incelemeleri `NATURAL-CORRECTION-PLAN-TR.md`, `EXPRESSIVE-REPAIR-RESEARCH-TR.md` ve `research/smoother/retrieval.json` ile kayıtlıdır. Yeni incelemede:

- Signalsmith'in otomasyon zamanının input/output gecikmesine göre tanımlandığı README ve faz tahminini zaman/frekans/kanal komşularıyla bağlayan gerçek kod okundu. Bölge başında motoru yeniden başlatmamak ve işleme zamanını doğru tutmak esas alındı.
- Rubber Band R3LiveShifter kodunda faz ilerlemesinin gelecek hop yerine önceki input/output ilerlemesine bağlandığı; çok pencereli analiz, sentez pencereleri ve overlap-add birleştirmesi incelendi. Bu kod kopyalanmadı veya çalışma zamanı bağımlılığı yapılmadı. Bu inceleme Rubber Band ses kalitesi karşılaştırması değildir.
- Librosa pYIN, onset/backtrack, DTW ve HPSS kodları okundu. Onset, yeni nota kanıtı olarak tek başına kullanılmıyor; armonik/perküsyonsu ayrım bağımsız polifonik nota ayırma sayılmıyor. Bunlar ek çalışma zamanı bağımlılığı değil.
- JUCE parametre yumuşatma ve ballistics kaynakları incelendi. Parametre adımını yumuşatmak, yanlış algılanmış nota veya yanlış bölgeyi düzeltmez.
- ZL Equalizer README ve gerçek LICENSE.md incelendi. AGPL-3.0 kaynak, araştırma örneğidir; bu sürümün kendi dinamik EQ'sunun yerine kopyalanmadı. Oeksound soothe2 resmi sayfası proxy tarafından engellendi. Ürünün kapalı algoritması incelenmiş veya aynısı geliştirilmiş gibi sunulmuyor.
- MTG sms-tools-materials README'sindeki ses dosyaları lisansı CC BY 4.0 olarak belirtilmiş. `sounds/violin-B3.wav` kısa gerçek keman kaydı incelenecek ses örneği olarak alındı; kaynak URL, hash ve atıf paketle birlikte korunacak. Tek bir kısa nota, gerçek keman repertuvarını veya farklı çalgıları temsil etmez.

İşitsel algı için tek bir 'beyin puanı' seçilmedi. Duyulabilirlik; kayıt, dinleyici, seviye, müzikal bağlam ve çalgının davranışına bağlıdır. Düzgün bir spline, düşük cent hatası veya bir modelin 'doğal' demesi bağımsız doğallık doğrulaması değildir.

## Uygulanan katmanlar

### Temel hedefler

Kaynak/ölçüm ve hedef ayrı. Kaynak notanın sınıfı bütün oktavlarıyla aynı hedefe bağlı; kaynak merkezleri olayın gerçek perdesinden izlenir. Doğrudan eklenen genel cent/nota kaydırması ayrı kalır. Başlangıçta bilinmeyen hedefler otomatik olarak eşit aralığa çekilmez.

ESITLE ölçülmüş veya elle tanımlanmış merkezleri hedeflere alır. Genel cent kaydırması hedef kutularında görünür; DSP'ye ikinci kez eklenmez. Nota eşleme menüsü ve genel nota kaydırması, ince cent değerinden ayrı tanımlanır. Üst satırda hiç okunmayan nota düzenlenebilir; elle girilen değer ölçülmüş kayıt gibi sayılmaz.

### Perde smoother ve düzeltme miktarı

- Perde smoother bağımsız açık/kapalı; otomatik davranış ayrı açık/kapalı.
- Merkez gözlem süresi, geçiş süresi ve azami düzeltme eğimi ayarlanabilir. Otomatik canlı yol, hareket ve atak kanıtına göre geçiş zamanını değiştirir. Bu sinyal tabanlı bir sezgiseldir; eğitilmiş çalgı tanıyıcı değildir.
- Düzeltme miktarı varsayılan %100. Kullanıcı miktarı azaltabilir veya üst sınır olarak otomatiğe bırakabilir. Otomatik miktar, dedektörün periyodiklik güvenine göre sınırlanır; kalibre edilmiş müzikal hata olasılığı değildir.
- %50 yaklaşma, kuru/işlenmiş sesleri karıştırmak değildir. Kaynak merkez ile hedef arasındaki düzeltmenin yarısını uygular. Dry/Wet ise ses karışımıdır ve ayrı kontroldür.
- Merkez daraltma ve vibrato koruma, temel hedef değerinden ayrı kontrollerdir. Bunları artırmanın her çalgıda daha iyi olduğu varsayılmaz.

### İfade sapması / Humanize

Varsayılan kapalıdır. Açılırsa kullanıcının belirlediği cent sınırı içinde, zamanla yumuşak değişen ve DAW zamanına bağlı tekrarlanabilir bir sapma ekler. Seed ve zamanla aynı yerde aynı hareket üretilebilir; her ses bloğunda rastgele akort sıçraması oluşturulmaz. Otomatik seçenek hareketli bölümlerde ek sapmanın miktarını azaltır.

Bu bilinçli ifade değişikliğidir: aktifken nihai perde tam hedef centinde sabit kalmayabilir. Kapatınca bu ek hareket kalkar. İfade sapması ve merkez daraltma canlı DSP yolundan çalışır; bu işlemler açıkken tam parça önbelleği kullanılmaz, Sonar bounce canlı yolun sonucunu alır. Hazırlanmış sesin bu kontrolleri uyguladığı iddia edilmez.

### Bölgesel hazırlık

Normal VST3 yalnız kanaldan geçen sesi alır. Sabit boyutlu ses kuyruğu, duyulan örnekleri DAW'ın mutlak zamanıyla işçiye aktarır; seyrek bellek arka planda kurulur. Aynı yeri yeniden çalmak ikinci bir kayıt eklemez. Örnek değeri değişirse kaynak değişimi dikkate alınır. Kayıtlı aralıkta duyulmamış boşluk varsa hazırlık bunu sessizlikle doldurup tamamlanmış gibi göstermez.

TUM PARCA BURADA, o anda toplanmış aralığı tam performans olarak işaretler ve ağır analiz/render başlatır. Okunan ses dondurulur; yeni kaynak için BASTAN OKU kullanılır. Hazır kaynakla karşılaştırmada ses değişikliği görülürse eski önbellek devreden çıkar. Bu özellik DAW kliplerine otomatik dosya erişimi veya ARA bölge okuyucu yerine geçmez.

Hazırlık, mevcut adaptif bölge analizi ve bağlı geçiş planını kullanır. Tek motor, komşu kararlı bölgeler arasında devam eden düzeltme eğrisini işler; bölge sınırında sıfır düzeltmeye inip tekrar çıkma yaklaşımı kullanılmaz. Örtüşen seslerde tek kaydırıcıyla uyuşmaz hedefler bağımsız düzeltilmiş gibi gösterilmez. Hazırlanmış plan bir sonraki transport başlangıcı/konum değişiminde devreye alınır; geçişte kısa karışım uygulanır. Canlı yol parametre değişikliklerini hemen duyurur.

Hazırlık 600 saniye ve yaklaşık 240 MiB kaynak veri sınırı taşır; çalışma belleği, render kopyaları ve kuyrukla birlikte daha yüksektir. Birden fazla örnekte bellek maliyeti ayrıca ölçülmelidir. Ses callback'inde seyrek harita kurma, plan üretme veya disk/ağ işlemi yapılmaz.

### Tını / rezonans smoother

Varsayılan kapalı, 20 logaritmik merkezli bantta kesme yapan kendi dinamik EQ uygulamasıdır. Yerel bant enerjisini komşularla karşılaştırır, bant başına azami azaltma, atak/bırakma, frekans aralığı ve beş grup anahtarı sağlar. Kanal kazançları bağlıdır; stereo kanallar birbirinden bağımsız rastgele işlenmez. Filtre katsayıları ara örneklerde geçişli güncellenir.

Harmonik ve atak koruma ayrı kapatılabilir. Harmonik koruma gerçek veya tahmin edilmiş F0 gerektirir; tını, kaynak ayırma veya enstrüman kimliği garantisi değildir. Otomatik seçenek bir spektral kuraldır. Bu işlem, çekirdek pitch motorundaki bir hatanın giderildiği kanıtı değildir; yalnız isteğe bağlı tonal cilalamadır.

### Yerel onarım filtresi

Varsayılan kapalı. Kullanıcı seçerse frekans ve karışım ayarlı son filtre çalışır; bütün kayda veya belirtilen zaman aralığına uygulanabilir. Otomatik karışım yüksek frekans enerjisinden önerilir; kullanıcının miktarını aşmaz. Tınıyı koyulaştırabilir. Yanlış perdeyi veya yanlış ayrılmış sesi düzelten bir yeniden sentez motoru olarak sunulmaz.

## İzlenebilirlik ve AI

Ana ekran açık/kapalı modüllerin ayar ormanına dönüşmez. Ek ayarlar Perde/Humanize, Tını/Rezonans ve Yerel Onarım sayfalarında. Akan tabela son işlem kayıtlarını tutar; motor komutu ile bağımsız ölçüm birbirine karıştırılmaz. Hazır render çalarken canlı panel ölçümleri ayrıca belirtilir.

OpenAI Chat Completions için gerçek HTTP bağlantısı, oturumluk API anahtarı, sınırlı parametre komutları ve kopyalanabilir teknik rapor vardır. Metin isteği analiz özetini gönderir. SESLE SOR, kullanıcının başlattığı 12 saniyelik işlenmiş çıkış örneğini de gönderir. Anahtar projeye veya rapora kaydedilmez. Ağ ve model çağrısı ses callback'inde yapılmaz.

Model yanıtı izinli sayısal parametreler ve aralıklar açısından doğrulanır; bilinmeyen/bozuk komutlarda ayar uygulanmaz. İstek sırasında ayarlar değişmişse eski yanıt uygulanmaz. Kod veya shell komutu çalıştırılmaz. Bu sohbet oturumuna otomatik bağlantı yoktur; RAPORU KOPYALA çıktısı buraya getirilebilir.

Gerçek API anahtarı olmadığı sürece servis bağlantısı ve sesli model değerlendirmesi test edilmiş sayılmaz. ChatGPT aboneliği API kullanım hakkı değildir. AI'ın yorumları insan dinleme paneli, Sonar testi veya gerçek ölçüm sonucu yerine geçmez.

## Açık sınırlar

Bağımsız polifonik nota ayırma, genel enstrüman tanıma, her teknik için öğrenilmiş üretim ve kaynak icracının tınısını garantili koruyan sinirsel yeniden sentez bu sürümde yoktur. Devam eden araştırma hedefleri bunlardır. Yeni modüller gerçek DSP'dir; kapsam dışındaki özelliklerin yerine arayüz maketi veya başarı beyanı konulmaz.

Canlı perde davranışının gözlem/geçiş/eğim değerleri varsayılandan elle değiştirilirse veya perde smoother/otomatiği kapatılırsa hazır bölgesel önbellek devreden çıkar. Bu kontroller canlı DSP üzerinde hemen duyulur ve Sonar bounce bu yolu alır. Tam-parça motoru varsayılan otomatik bölge planını kullanır; elle değiştirilen canlı kontrolü uygulamadığı halde uygulanmış gibi göstermez.
