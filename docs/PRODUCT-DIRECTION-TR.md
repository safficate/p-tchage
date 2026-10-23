# Ürün yönü: polifonik akort ve makam düzenleme

Kayıt tarihi: 2026-10-21. Kullanıcının son tarifini geliştirme boyunca koruyan gereksinim belgesidir. Aşağıdakiler hedef özelliklerdir; teslim edilmiş veya testten geçmiş özellik listesi değildir. Önceki ilk teslim yol haritasındaki öncelikler, bu uzun vadeli hedefin tamamlandığı anlamına gelmez.

Temel kalite hedefi, düzeltilmiş sesin aynı icracı tarafından doğru biçimde yeniden çalınmış gibi doğal duyulmasıdır. İlk kaynak incelemesi, aday yöntemler ve deney planı [EXPRESSIVE-REPAIR-RESEARCH-TR.md](EXPRESSIVE-REPAIR-RESEARCH-TR.md) belgesindedir; doğallık henüz doğrulanmış değildir.

## Kullanımın temeli

- Windows x64 VST3 ve WAV açıp çalabilen, durdurabilen ve dışa aktarabilen masaüstü uygulaması birlikte geliştirilecek.
- Eklenti, yerleştirildiği kanalın sesini tuner gibi otomatik okuyacak. DAW zamanıyla eşleşen nota, oktav, cent merkezi ve ölçüm güveni gösterilecek. Normal kullanım Capture/Analiz/Uygula zincirine bağlı olmayacak.
- Bir nota hedefi bütün oktavlardaki karşılıklarına uygulanacak. Kullanıcı oktavları veya her nota olayını tek tek ayarlamak zorunda kalmayacak. Grafik yakınlaştırması ve dinleme tonunun oktavı, işlem kapsamını sınırlamayacak.
- Toplu hedef, her olayın kendi ölçülen merkezine göre hesaplanacak: örneğin Re2 +12 c ve Re5 −7 c için hedef +5 c ise düzeltmeler sırasıyla −7 c ve +12 c olacak. İki olaya da aynı miktarı eklemek farklı bir işlemdir.
- Aynı anda çalan farklı notaları ayrı hedeflere taşıma, gerçek polifonik işlem gerektirir. Tek temel frekans bulup kanalın tamamını kaydırmak bu gereksinimi karşılamaz.
- Kullanıcının hedef değişikliği doğrudan duyulacak; normal düzeltme için ayrıca öneri onayı gerekmeyecek. Dry/Wet, bypass ve geri alma karşılaştırmayı sağlayacak. Kilitlenen hedefler otomatik öğrenmeyle değişmeyecek.
- Hedef satırının en sağındaki Doğal düzeltme düğmesi, ifadeye yönelik ek otomatik davranışları açıp kapatacak; temel cent hedeflerini veya bypass'ı temsil etmeyecek. Ayrıntı panelinde bu davranışlar görülebilir ve değiştirilebilir olacak. Araştırma ve kontrol taslağı: [NATURAL-CORRECTION-PLAN-TR.md](NATURAL-CORRECTION-PLAN-TR.md).
- İşlem bilgisi sağdan gelip sola ilerleyen bir tabela halinde, durdurulabilir ve geçmişi okunabilir olarak gösterilecek. Açılan koruma seçeneği ile gerçekten ölçülmüş başarılı koruma sonucu ayrı ifade edilecek.
- Otomatik doğal düzeltme, her nota ve notanın içindeki atak/sustain/süsleme/glide/kuyruk boyunca değişen etkin ayarlarla çalışacak. Çalgı/teknik aynı kayıtta değişebilir; tek sabit profil yeterli sayılmayacak. Ek bir ultra-dinamik düğmesi eklenmeyecek. Ayrıntı panelinde zamanla değişen etkin değerler ve eğriler görülebilecek, kullanıcı ilgili kapsamı değiştirebilecek.
- Son öncelik açıklaması: ses kalitesi şu aşamada CPU tasarrufundan önde. Ağır analiz/yeniden sentez için arka plan/offline hazırlığı ve sonradan DAW bounce kabul ediliyor. Hazırlık ses callback'ini bloke etmeyecek; ilk dinleme sırasında normal VST3'e ulaşmayan klibin tamamı okunmuş varsayılmayacak.

## Nota ve makam görünümü

- Her ses/nota olayı ayrı iz veya bölge olarak gösterilecek. Üzerinde nota adı, oktav ve cent merkezi okunacak; kaynak, hedef ve gerçekten ölçülebilen çıkış birbirinden ayrılacak.
- Okunmayan notalar işaretlenecek fakat kontrolleri kapatılmayacak. Kullanıcı elle tanımlayabilecek; bu değer kayıtta ölçülmüş gibi gösterilmeyecek. Nadir görülen güvenilir nota da yüzdesiyle bulunacak.
- Eşitle düğmesi üst ölçümleri alt hedeflere kopyalayacak. Genel kaydırma, tanımlı alt hedeflerin sayılarında da görünecek ve DSP'de ikinci kez eklenmeyecek. Referansa toplu hizalama merkezi, hedef nota satırındaki tek seçimle belirlenecek.
- VST3 ana ekranında WAV açma, DAW transportunu taklit eden oynat/durdur, kanal seçimi ve zorunlu otomatik düzelt/uygula düğmeleri olmayacak. WAV transportu bağımsız masaüstü sürümünün konusudur.
- Kullanıcı nota üzerindeki veya altındaki değer alanını değiştirerek ya da çizgiyi sürükleyerek müdahale edebilecek. Toplu dizi görünümü ile ayrıntılı olay görünümü aynı hedef modelini kullanacak.
- Zaman ve perde eksenlerinde hem yakınlaştırma hem gezinme bulunacak. Çalınan bölgeyi takip, görünüme sığdırma ve okunabilir cent ayrıntısı sağlanacak.
- İlk duyulan, en sık geçen veya en pes nota otomatik olarak karar sesi kabul edilmeyecek. Karar sesi ve makamın derece eşlemesi ayrı, görünür bilgiler olacak; kullanıcı bunları düzeltebilecek.
- Kullanıcı kararı kilitlediğinde, makam değişimi aynı karar frekansını koruyacak. Kaynak ve hedef makamda kararın farklı derece numarasıyla gösterilmesi, karar frekansını değiştirmeyecek. Eşleme, derece aralıklarını karar sesine göre kuracak ve oktav geçişlerini koruyacak.
- Makam modeli yalnız 12 eşit aralıklı nota kartından ibaret olmayacak. Dereceler, cent aralıkları, karar, seyir ve bağlama göre değişebilen perde varyantlarını taşıyabilmeli. Nota yazımını değiştirmek sesin değiştiği anlamına gelmez.
- Kaynaktan öğrenme önce gözlenen perde merkezlerini, dağılımları ve belirsizlikleri çıkaracak. Bir makam adını kesin saptadığı iddiası ayrı bir doğrulama gerektirir. Kullanıcı kendi dizisini ve varyantlarını kaydedebilecek.
- Yeni makamı incelemek için serbest deneme notları ve açıklamalar saklanabilecek. Bunlar doğrulanmış müzik teorisi veya otomatik tanı sonucu olarak sunulmayacak.

## Enstrüman ve ifade davranışı

Tek keman, keman grubu, tek/çok telli gitar, bağlama, kanun, piyano ve vokal için aynı işlem varsayılmayacak. Kanun ve benzeri çalgılar tek sesli ve çok sesli pasajlar arasında geçebilir; piyano pedalıyla üst üste binen kuyruklar da eşzamanlı ses sayılır.

Korunacak unsurlar: ataklar, vibrato, süslemeler, glide/portamento, ses kuyrukları, formant/tını ve stereo ilişki. Merkez taşıma, ifadenin genişliğini değiştirme ve geçişi yeniden şekillendirme ayrı işlemler olacak. Varsayılan merkez düzeltmesi bütün çizgileri düzleştirmeyecek; ifadeye müdahale gerektiğinde bunun kapsamı görülebilecek ve geri alınabilecek.

Enstrüman seçimi bir işlem profili belirleyebilir; yalnız bir profil adı göstermek enstrümanın otomatik tanındığını veya kalitenin sağlandığını kanıtlamaz. Belirsiz çok sesli bölgeler görünür olacak; sistem güvenilmez ayırmayı başarılı düzeltme gibi göstermeyecek.

## İki icrayı karşılaştıran nota düzenleme ve onarım ajanı

Kullanıcının örneği: aynı parçanın iki keman kaydı verilir. Biri referans olarak seçilir; diğeri bu icraya uygun akortlanırken kendi hatalı nota bölgeleri de düzeltilir. Ajan yalnız genel parametreleri değiştirmez; bir editör gibi tek tek nota olaylarını inceleyip toplu görevi bu olaylar üzerinde yürütür.

- Önce iki icranın müzikal olarak karşılık gelen cümleleri ve nota olayları eşlenir. Kayıt başlangıcı veya icra temposu farklı olduğunda aynı saniyedeki seslerin aynı nota olduğu varsayılmaz. Perde düzeltme isteği zamanlamayı kendiliğinden değiştirmez.
- Referans mutlak doğru kabul edilmez. Ölçülen fark, olası hata ve bilinçli icra/süsleme ayrılır; karar, makam, transpozisyon ve kullanıcının kilitlediği bölgeler eşlemeye katılır. Belirsiz eşleme görünür kalır.
- Ajan olay merkezini taşıma, yerel perde eğrisini yeniden çizme, glide bağlantısını şekillendirme ve süslemeyi koruma gibi işlemleri ayrı uygulayabilir. Referansın bütün vibratosunu ve cümlelemesini hedef icraya körlemesine kopyalamaz.
- Her düzenlemede özgün kayıt saklanır. Değişen bölgeler, kaynak ve hedef eğrileri, yapılan işlem ve gerçek çıkış ölçümleri görülebilir; düzenlemeler tek tek veya topluca geri alınabilir.
- Öncelik mevcut ses üzerinden şeffaf düzeltmedir. Bunun yetmediği bölgelerde enstrüman tınısını koruyan yerel yeniden sentez araştırılacaktır. Yalnız bir formant ayarı, keman kimliğini veya yay davranışını koruma garantisi değildir; harmonikler, spektral zarf, atak, gürültü bileşenleri, geçişler, faz ve kuyruk da değerlendirilmelidir.
- Yeniden sentezlenen bölgeler açıkça işaretlenir. Yeni ses üretimini, kaynak icradan yalnız perde kaydırılmış gibi göstermemek gerekir. Yeniden sentez, sınırlı düzeltmeden ayrı bir işlem türü ve kalite kapısıdır.
- Bu yaklaşımın hedef kapsamı bütün belirtilen çalgılardır; ancak tek kemanda elde edilen sonuç keman grubu, kanun, bağlama veya piyano için doğrulama sayılmaz. Her çalgı ve tek/çok sesli davranış ayrı gerçek kayıtlarla değerlendirilir.
- Otomatik ajan, ileri kontrolleri amaca göre kullanır: merkez, geçiş, süsleme, atak ve yeniden sentez için farklı müdahaleler seçebilir. Kullanıcı her nota için onlarca ayar girmek zorunda kalmaz; gerektiğinde ayrıntı görünümünden yapılan işe müdahale edebilir.
- Testler, referans eşleme doğruluğunu ve cent hedefini olduğu kadar eklenen/kaybolan sesleri, sınırdaki klikleri, tını değişimini ve çalımın korunmasını da kapsar. Tını ölçümleri dinleme değerlendirmesinin yerine geçmez. Güvenilir onarım sağlanamayan bölge, başarılı sonuç gibi sunulmaz.

Bu, mevcut önizlemenin yaptığı bir işlem değildir. Olay eşleme, polifonik ayırma ve tınıyı koruyan yeniden sentez ayrı araştırma ve uygulama başlıklarıdır.

## Yan panelde ajan

- Hedef, kullanıcıyla konuşan, izin verilen ayarları değiştiren, önce/sonra ölçümlerini çalıştıran ve geliştirmeye geri bildirim hazırlayan gerçek bir model bağlantısıdır. Sohbet maketi, çalışan ajan sayılmaz.
- İlk adım tekrarlanabilir geri bildirim paketi: sürüm, ilgili zaman/bölge, kaynak ve hedef değerleri, uygulanan değişiklikler, gerçek test çıktıları ve kullanıcının açıklaması. Ses dosyası varsayılan olarak pakete eklenmeyecek.
- Sonraki adım, doğal dil isteğini sınırları belirli parametre ve bölge işlemlerine dönüştürmek. Örneğin “kararı koru, ikinci dereceyi 5 cent indir” yalnız belirtilen derecenin hedefini değiştirecek; yapılan değişiklik gösterilecek ve geri alınabilecek.
- Ajanın kontrolü, ses motorunun doğrulanan komut arayüzünden geçecek. Gerçek zamanlı ses iş parçacığında model çağrısı, ağ erişimi veya ağır analiz olmayacak.
- Kodex ile devamlılık için proje/sürüm bilgisi taşıyan geri bildirim aktarımı tasarlanacak. Uygulamanın bu sohbet oturumuna kendiliğinden eriştiği veya model aboneliğinin uygulamaya otomatik API erişimi sağladığı varsayılmayacak.
- Ölçüm testleri cent hatası, yanlış nota/oktav takibi, eksik veya eklenen sesler, atak/kuyruk bozulması, CPU/RAM ve durum geri yüklemeyi kapsayacak. Ölçümlerin geçmesi tek başına müzikal doğallık kanıtı değildir; gerçek kayıtlarla dinleme değerlendirmesi de gerekecek.
- Güncelleme hazırlama ile çalışan uygulamaya güncelleme kurma ayrı işlemler olacak. Değişiklik, test sonucu, sürüm ve geri dönüş yolu görülebilecek; sohbet yanıtı kendi başına yazılım güncellemesi sayılmayacak.

## Aşamalı geliştirme

Kullanıcının 4–5 aylık çalışma niyeti bir geliştirme ufkudur; her kayıtta kusursuz polifoni için süre veya kalite garantisi değildir. Aşamalar ölçülen sonuca göre ilerleyecek:

1. **Kullanılabilir temel:** sade ekran, bağımsız WAV transportu, canlı öğrenme, tüm oktavlara ortak hedefler, anlık değişiklik, proje kaydı. Tek sesli gerçek kayıtlarla ve 44.1/48 kHz ile doğrulama.
2. **Makam ve karar:** değişebilir derece modeli, karar kilidi, toplu dönüşüm, kullanıcı dizileri ve açıklanabilir eşleme. Karar frekansının dönüşüm öncesi/sonrası değişmediğini ölçme.
3. **İfade:** glide, vibrato, atak ve kuyruklar için ayrı davranışlar; enstrüman profilleri ve gerçek kayıtlarla karşılaştırmalar.
4. **Polifoni araştırması:** önce kontrollü çift sesler, sonra gitar/kanun/piyano ve topluluklar. Eksik temel, birleşen harmonikler, unison ve sustain testleri. Ayrıştırma ile yeniden birleştirme kalitesi geçmeden tam polifonik düzeltme ilan edilmeyecek.
5. **Ajan ve bütünleştirme:** gerçek model sağlayıcısı, komut uygulama, test raporları ve geliştirme geri bildirimi. Güncel Cakewalk Sonar ve hedef HP EliteBook üzerinde yerel tarama/açılış/playback/render/proje yükleme testleri ayrıca yürütülecek.

## Bugünkü sınır

Mevcut yayımlanmış önizleme bağımsız polifonik nota düzeltmesi, makam öğrenme, enstrüman tanıma veya çalışan sohbet ajanı sağlamıyor. Yeni canlı çalışma alanı kodu henüz tamamlanmış ve test edilmiş bir teslim değildir. Paylaşılan yeni ekran görseli taslaktır. Bu belge yeni çalıştırılabilir dosya üretmez ve Sonar uyumluluğuna yeni bir doğrulama eklemez.
