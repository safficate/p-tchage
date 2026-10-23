# Doğal düzeltme: araştırma bulguları ve uygulanacak kontrol taslağı

Tarih: 2026-10-23. Kullanıcının istediği sıra: önce araştırma ve işlemlerin açıklaması, sonra eklenti uygulaması ve gerçek sesle değerlendirme. Bu belge tasarımdır; aşağıdaki sınıflandırıcılar, yeniden sentez ve ajan henüz uygulanmış değildir. Bu belgenin ilk araştırma aşamasında yeni derleme yapılmamıştı. Daha sonra uygulanan 3.1 kapsamı için SMOOTHER-RESEARCH-3.1.md ve TEST-REPORT-3.1.md esas alınmalıdır; burada anlatılan uzun vadeli sınıflandırıcılar ve yeniden sentez teslim edilmiş sayılmaz.

## 1. Düğmenin anlamı

Hedeflerim satırının en sağında **Doğal düzeltme: Açık/Kapalı** bulunacak. Kapalı konum, girilen cent/nota hedeflerini veya genel kaydırmayı kapatmayacak; yalnız ifadeyi yorumlayan ek müdahaleleri devre dışı bırakacak. Tıklama önleyen temel parametre geçişleri ve sayısal güvenlik her iki durumda da kalacak. Gerçek bypass, üst araç çubuğunda ayrı olacak.

Açıkken sistem kaynağı ve komşu olayları analiz edip aşağıdaki davranışları seçebilecek. Düğme bir hazır EQ veya rastgele humanize efekti olmayacak. Hedefleri kullanıcı belirleyecek; motor hedefe ulaşmak için kullanılacak yolu düzenleyecek. Hedefe ulaşılamazsa gerçek kalan fark gösterilecek; hedef değeri sessizce değiştirilip başarı ilan edilmeyecek.

Kullanıcının son açıklaması: açık konumdaki otomatik mod, **her olayın içinde de değişen bir davranış planı** üretecek. Kayıt başında bir kez 'keman' seçip tek ayar setini sonuna kadar kullanmak yeterli değil. Ayrıca 'ultra dinamik' gibi bir kalite düğmesi istenmiyor. Sonuç kalitesi şu an CPU tasarrufundan öncelikli; ağır analiz ve render için arka plan/offline hazırlığı kabul ediliyor. Bu, ses callback'inin bloke edilmesi anlamına gelmiyor.

Doğal düzeltme düğmesinin yanından ayrıntı sayfası açılacak. Otomatik karar, kanıtı, etkin değer, elle verilen değer, kapsam ve çıktı ölçümü ayrı görülecek. Kapsam seçenekleri: tüm kayıt, nota grubu, seçili olay ve bağlı cümle. Elle kilitlenen ayar bir sonraki öğrenmeyle değişmeyecek. Çalgı etiketi ve akort/dizi tercihi ayrı kontroller olacak.

## 2. Araştırmada gerçekten okunan kaynaklar

- **Librosa onset/backtrack uygulaması:** spektral/enerji olayları, başlangıcı önceki enerji minimumuna geri taşıma. Bir onset, otomatik olarak yeni müzikal nota değildir. Yay değişimi aynı notada olabilir; tremolo yeniden ataklar içerir. [Kaynak](https://github.com/librosa/librosa/blob/main/librosa/onset.py).
- **Librosa DTW ve zaman dizisi uygulaması:** iki icranın olaylarını süre farklarıyla eşleme ve sınırlandırılmış yollar. Tek başına notanın müzikal işlevini çözmez, kaydın zamanını kendiliğinden değiştirmeyi gerektirmez. [Kaynak](https://github.com/librosa/librosa/blob/main/librosa/sequence.py).
- **Librosa HPSS:** medyan filtrelerle harmonik/perküsyonsu bileşen maskeleri ve kalıntı. Çalgı veya nota bazında polifonik ayırma değildir. [Kaynak](https://github.com/librosa/librosa/blob/main/librosa/decompose.py).
- **STK Bowed ve StifKarp kodu:** yaylı modelinde uyarım, vibrato ve gövde filtresi; telli modelde tel sertliği, çekme konumu, sönüm ve dağılımın ayrı ele alınması. Bunlar fiziksel model örnekleridir; bağlama, kanun veya gerçek keman kaydında ölçülmüş başarı değildir. [Bowed](https://github.com/thestk/stk/blob/master/src/Bowed.cpp), [StifKarp](https://github.com/thestk/stk/blob/master/src/StifKarp.cpp).
- **SMS Tools README ve harmonicFreqScaling kodu:** harmonik/kalıntı modelleri; frekansı taşırken spektral zarfı örnekleyerek genlikleri yeniden düzenleme. Yeniden sentezde tını kontrolü için somut yöntem örneği; keman kimliğini koruduğu sonucu çıkarılamaz. [Kaynak](https://github.com/MTG/sms-tools/blob/master/smstools/transformations/harmonicTransformations.py).
- **MIDI-DDSP README:** nota bazında vibrato, atak, parlaklık ve harmonik/gürültü kontrolleri. Önceki incelemede okundu; hazır model kanun, bağlama ve piyano kapsamını sağlamıyor. [Kaynak](https://github.com/magenta/midi-ddsp).
- **Tomato README:** makam müziği için perde dağılımı, karar/transpozisyon, seyir ve nota eşleme ayrı görevler. Sıklık histogramından tek başına kesin karar veya makam çıkarımı yapılmayacak. [Kaynak](https://github.com/sertansenturk/tomato).
- **PANNs README:** AudioSet üzerinde ses etiketi ve olay tespiti. İncelenen belgedeki genel etiket başarısı, bizim çalgı/teknik ayrımımız için ölçüm değildir. Bir sınıflandırıcı profil önerebilir; kanunu, bağlamayı veya icra tekniğini kesin tanıdığını göstermiyor. [Kaynak](https://github.com/qiuqiangkong/audioset_tagging_cnn).
- **ViSQOL README:** tam referanslı algısal benzerlik değerlendirmesi; audio modu 48 kHz bekliyor ve çok kanalı monoya indiriyor. Bu nedenle tek başına stereo koruma, makam doğruluğu veya doğal icra ölçeri olamaz. [Kaynak](https://github.com/google/visqol).

Yeni erişim kayıtları, başarısız erişimler ve SHA-256 değerleri `research/natural-correction/retrieval.json` içinde tutuldu. Birincil kod/belge incelemesi yapıldı; ilgili makalelerin tüm metinleri veya model demoları incelenmiş gibi sunulmuyor. ITU-R BS.1534 sayfası proxy nedeniyle açılamadı. PANNs için denenen LICENSE yolları 404 verdi; bu inceleme o projenin lisansını doğrulamıyor.

SMS Tools AGPL-3.0; Tomato kodu AGPL-3.0, belgede belirtilen veri/model/çıktıların lisansı CC BY-NC-SA 4.0. Bunlar şu anda eklenti bağımlılığı yapılmadı. Yöntem incelemek, bu kodu veya ağırlıkları dağıtmak anlamına gelmiyor.

## 3. Nota parçalarının birbirine bağlanması

Bir sesin değişen harmonik genliği, yeni nota diye bölünmemeli. Aynı notada yeniden mızrap/yay atağı olabilir; yeni atak olmadan başka notaya kayış olabilir. Bu nedenle küçük parçaları yalnız enerji eşikleriyle kesmek yetmez.

Planlanan model, her olay için önceki/sonraki olay ilişkisini tutacak:

- **Bağlı geçiş:** sürekli perde hareketi, kararlı uçlar, benzer kaynak izi ve belirgin yeni atak yokluğu birlikte değerlendirilir. Kaynak glide boyunca mevcut şekil korunur; iki uç için gereken düzeltme eğrisi ayrıca eklenir.
- **Yeniden atak:** aynı nota veya komşu nota yeni uyarımla başlar. Bir önceki olayın sönümünü yeni atağa yapay bir glide ile bağlama.
- **Süsleme:** kısa süre ve perde yönü tek başına karar verdirmez; komşu notalar, tekrarlanan cümle, bağlam ve varsa referansla birlikte ele alınır. Kısa notalar otomatik silinmez veya ortalamaya yedirilmez.
- **Üst üste ses:** eski sesin kuyruğu yeni nota altında devam edebilir. İki farklı hedef gerekiyorsa tek kanal kaydırıcısı bunları ayrı düzeltemez. Gerçek ses ayırma gerekir; HPSS bunun yerine geçmez.
- **Belirsiz ilişki:** tek etiketi kesinmiş gibi seçmek yerine adaylar ve belirsizlik ayrıntıda saklanır. Kullanıcı bölme/birleştirme veya bağlantı seçimini düzeltebilir.

Oktav/harmonik grubu, yalnız frekansın tam sayı katı olmasına bakılarak kurulmayacak. Zaman içindeki birlikte hareket, atak, sönüm, faz/stereo ve bağlam kanıtı değerlendirilecek. Aynı nota adındaki farklı icracılar veya teller yanlışlıkla tek ses yapılmayacak. Renk, nota sınıfını; grup çizgisi/deseni ise kaynak/olay ilişkisini gösterebilir.

## 4. Ayrıntı sayfasında önerilen kontrol aileleri

Evrensel bir 'profesyonellik yüzdesi' veya bütün çalgılara sabit gecikme seçilmiyor. Sayısal aralıklar gerçek kayıt deneyleriyle belirlenecek.

| Kontrol | Etkisi | Görülecek kanıt / başlıca risk |
| --- | --- | --- |
| Merkez izleme | Kararlı merkez ile yavaş kaymayı ayırma; izleme süresi ve güven | Yanlış uzun pencere hızlı süslemeyi yutar, kısa pencere vibratoyu merkez sanır. |
| Olay bağlantısı | Önceki/sonraki olayla legato, yeniden atak, süsleme veya kuyruk ilişkisi | Onset ve perde kanıtları beraber; otomatik etiket elle düzeltilebilir. |
| Geçiş şekli | Düzeltmenin başlama/bitiş yeri, süresi, eğimi ve kıvrımı | Sadece 'daha yavaş' yapmak doğallık sağlamaz; fazla eğim veya uzatma duyulur. |
| Vibrato / süsleme koruma | Mevcut merkez çevresindeki hareketi koruma, istenirse ayrıca düzenleme | Koruma açık bilgisi ile gerçekten ölçülen vibrato korunumu ayrı gösterilir. |
| Atak koruma | Gürültülü/impulsif başlangıca müdahalenin kapsamı ve geçişi | Dry/wet karışımı faz ve comb-filter rengi oluşturabilir; her koşulda tam koruma iddiası yok. |
| Kuyruk / sustain koruma | Bir önceki sesin devamını ve yeniden atakları ilişkilendirme | Ayrılmamış farklı sesler için ayrı hedef uygulanamaz. |
| Tını / zarf koruma | Harmonik genlik dağılımı veya vokal spektral zarfına müdahale | Küçük cent düzeltmesinde gereksiz zarf müdahalesi sesi bozabilir. |
| Gürültü bileşeni | Nefes, yay ve mızrap benzeri periyodik olmayan bileşenlere yaklaşım | Perküsyonsu bileşen maskesi, bunların kusursuz ayrıldığı anlamına gelmez. |
| Stereo / faz | Kanal ilişkisinin ve kaynak yerinin korunması | İşlem sonrası genişlik/kanal ilişkisi ölçülür; mono kalite puanı bunu kapsamaz. |
| Belirsizlik davranışı | Düşük güvenli bölgede müdahaleyi sınırlama ve raporlama | İstenen hedef ile gerçekte uygulanan hedef/fark ayrı tutulur. |
| Yerel yeniden sentez | Seçili bölge için ayrı render adayı, sınır birleştirme | Kaynak kimliği değişebilir; deneysel, görünür ve geri alınabilir. |

Olay bazlı etkin değerler, otomatik değiştiklerinde bir geçmiş kaydı üretir. Her ses bloğunda bütün parametreleri gelişi güzel değiştirmek yerine kararlı kararlar ve sürüm bilgisi kullanılacak.

### Zaman boyunca otomatik davranış

Plan sabit profil yerine, olay/bağlı cümle/zaman kimliğiyle her kontrol için etkin bir eğri taşıyacak: merkez takibi, merkez çevresindeki daraltma miktarı, geçiş kapsamı ve şekli, vibrato/süsleme yaklaşımı, atak ve kuyruk koruma, zarf/gürültü davranışı, kaynak gruplaması, kullanılacak üretim motoru ve belirsizlik. Etkin değer bir kontrol için sabit kalabilir; sırf 'dinamik' görünmek için bütün kontroller oynatılmayacak.

Üç zaman ölçeği birlikte kullanılacak:

1. **Çalgı/kaynak ve teknik bağlamı:** bir pasaj içindeki temel kaynak, başka çalgıya veya vokale geçiş ve olası karışım. Sınıflandırma önerileri zaman içinde tutarlı izlenecek; anlık tek kare yanlışlığı bütün motoru değiştirmeyecek. Kanıt destekliyorsa aynı kayıt içinde profil değişebilecek.
2. **Cümle ve nota ilişkisi:** yeni atak, bağlı komşu notalar, süsleme grubu, birlikte sönen veya üst üste kalan sesler. Önceki ve sonraki olay hesaba katılacak; offline hazırlık gelecekteki sesi de değerlendirebilecek.
3. **Olayın içi:** başlangıçta atak yaklaşımı, kararlı bölümde merkez taşıma, vibratolu bölümde hareketin korunması, çıkışta glide veya sönüm yaklaşımı. Aynı notanın başlangıcı, ortası ve sonu farklı davranış alabilecek.

Geçişlerde hedef eğrinin sürekliliği, üretim motorunun durumu ve stereo/faz ilişkisi birlikte kontrol edilecek. Parametre sıçramaları veya kare kare çalgı etiketi değişmesi kalite sayılmayacak. Canlı ve offline inceleme farklı karar yeteneklerine sahip olabilir; henüz dinlenmemiş gelecekteki sesi canlı yol bildiğini iddia etmeyecek.

Ayrıntı panelinde global varsayılan ile o anda etkin olan değer ayrı gösterilecek. Seçili nota/bölge üzerinde kontrol eğrileri, uygulanan zaman aralığı ve gerekçe görülebilecek. Kullanıcı bir değeri veya eğriyi ilgili kapsamda değiştirebilir/kilitleyebilir; diğer zamanların otomatik davranışı devam eder. Aynı ekranda notalar ve işlem şeritleri birlikte izlenebilecek.

İşlem sırası: saklanmış ses/ölçüm → bağlama dayalı dinamik plan → offline/arka plan ses üretimi → gerçek çıkış ölçümü → hazır sürümün zamanına hizalı çalımı → DAW bounce. Salt perde verileri yeniden render için yeterli değildir; ilgili sesin de DAW'dan ulaşmış ve sınırlandırılmış belleğe/diske alınmış olması gerekir. Normal kanal VST3'ü bütün klipleri kendiliğinden çekemez. Hazırlanan/eksik zaman aralıkları ve sürüm açık gösterilecek; yetersiz verili bir bounce tamamlanmış profesyonel sonuç gibi sunulmayacak.

Yerleşik performans hedefi, ağır hazırlık sırasında DAW ses callback'ini serbest tutmak ve hazır sonucu tutarlı çalabilmektir. Bu aşamada sırf düşük CPU için daha kötü kaliteli bir yöntem seçilmez. Kaynak/ayar değişmedikçe yeniden oynatma aynı hazırlanmış sonucu verir; yeni bilgiyle plan değişirse sürümü, kapsamı ve gerekçesi görünür olur.

## 5. Çalgı ve icra profilleri

Buradaki davranışlar test edilecek hipotezlerdir; eğitilmiş evrensel çalgı tanıyıcı veya ayarlanmış hazır değerler değildir.

| Profil | Öncelikli davranış | Kaçınılacak hata |
| --- | --- | --- |
| Solo keman | Kararlı nota merkezini taşı; vibrato ve yay değişimini koru; portamento bağlantısını iki uçla şekillendir | Vibratoyu düzleştirmek, her yay değişimini yeni nota saymak, yanlış F0 ile tını telafisi yapmak. |
| Yaylı grubu | Ortak akort sapması, unison genişliği ve stereo alanı izle; bağımsız ses düzeltmesini yalnız ayırma sağlanınca kullan | Bütün icracıları tek çizgiye sıkıştırıp ensemble etkisini yok etmek. |
| Bağlama | Mızrap, tremolo, bağlı süslemeler, tel grupları ve sönümü ayrı değerlendir; kısa süslemeyi komşu olaylara bağla | Her yeniden atağı başka nota saymak; çift/üçlü tel vurumlarını rastgele merkez hatası sanmak. |
| Kanun | Hızlı ardışık ataklar, tremolo, eşzamanlı sesler ve uzun kuyruklar; gerçekleşen mandal/perde değişimini bağlamla izle | Önceki telin kuyruğunu yeni notayla sürüklemek; üst üste seslere tek eğri uygulamak. |
| Gitar | Tel atağı, bend/release, başlangıçtaki doğal tizleşme ve akor/arpej kuyruklarını ayır | Bend'i yanlış nota diye bölmek, eski tel sönümünü yeni tel hedefiyle taşımak. |
| Piyano | Çekiç atağı, pedal, unison teller, doğal stretch tuning ve harmonik olmayan kısmi sesleri hesaba kat | Vurulmuş iki notayı glide ile birleştirmek, bütün kısmi sesleri ideal tam katlara zorlamak. |
| Ney / nefesli | Nefes bileşeni, perdeye yaklaşma ve ayrılma hareketlerini koru | Nefesi sesli nota sanmak; sürekli kayışın her nota sınırını ayrı olay yapmak. |
| Vokal | Ünlü zarfı ile ünsüz/nefesi ayır; melisma, vibrato ve register geçişlerini koru | Konuşma için eğitilmiş kalite ölçerini şarkı doğallığı hakemi saymak. |

**Mikrotonal ve Batı müziği ayrı çalgı değildir.** Çalgı/teknik profili, perde sistemiyle çarpraz kullanılacak. Mikrotonal seçimde yerel perde varyantları eşit aralıklı en yakın notaya otomatik yapıştırılmayacak. Batı kemanı seçimi de tek başına 440 Hz veya 12-TET zorlamayacak; hedefler kullanıcının haritasından gelir.

Vokalde konuşur gibi, sürekli legato, melismatik/süslemeli, nefesli ve sert/gürültülü üretim gibi davranışlar bölüm bölüm önerilebilir. Bir kayıt içinde değişebilirler. Bunlar kesin eğitimli sınıflar veya cinsiyet etiketleri değildir. Profil önerisinin güveni görünür ve elle seçilebilir olacak.

EQ, eksik nota veya yanlış perdeyi düzeltemez. Tını sorunu ölçülürse zarf telafisi ya da sınırlı spektral müdahale ayrı aday olabilir; sırf 'daha profesyonel' görünsün diye otomatik EQ/reverb/sıkıştırma eklenmeyecek.

## 6. Anlık davranış tabelası

Kullanıcının son düzeltmesi: birkaç saniyede kaybolan tek durum yazısı değil, **yeni kayıtların sağdan girip sola ilerlediği sürekli bir tabela**. Kaynak profilinin yanında/altında tek satır veya iki satır ayrılabilir.

Örnek olay biçimi:

`00:21.420 · Re4 · hedef +5 c · uygulanan −7 c · vibrato koruma AÇIK · glide geçişi UYGULANDI`

`00:23.180 · Sol3 · kuyruk çakışması · ayrı ses hedefi UYGULANAMADI · ayrıntı`

Bu metinler örnektir, mevcut motor çıktıları değildir. Kaynak okuması, planlanan işlem, gerçekten yürütülen işlem ve sonradan ölçülen çıktı ayrı durumlar olacak. “Vibrato koruma açık”, “vibrato korunduğu ölçüldü” demek değildir; ölçüm yoksa sonuç uydurulmayacak.

- Renkle beraber metin ve simge: cent taşıma mavi, geçiş mor, atak/kuyruk koruma turuncu, belirsizlik sarı; kırmızı yalnız gerçek sorun. Renk tek bilgi taşıyıcısı olmayacak.
- Üzerine gelince veya elle durdurunca tabela durur; ses işleme sürer. Akış hızı ayarlanabilir, ayrıntı ve geçmiş açılabilir. Otomatik olarak sonuna atlayıp okunan metni kaçırmaz.
- DAW ses zamanına ve eklenti gecikmesine göre hizalanır; arayüzün duvar saatiyle gelişi güzel akmaz. DAW durunca oynayan sese ait tabela ilerlemez. Aynı yere dönünce kayıtlı olaylar yeniden gösterilebilir.
- Sürekli aynı işlem her ses bloğunda yazılmaz; bağlı olay özetlenir, sayı güncellemeleri birleştirilir. Ayrıntı geçmişi sürümlü kalır. Böylece tabela saniyede yüzlerce satır üretmez.
- Ses iş parçacığı metin biçimlendirmez, dosya yazmaz veya AI çağırmaz. Sınırlı olay kuyruğuna sayısal kayıt koyar; UI metni oluşturur. Geçmiş ve kuyruk boyutu sınırlandırılır; taşma olursa sessizce tam günlük iddiası yapılmaz.

## 7. Algı ve doğrulama

Doğallık bir cent hedefinin doğruluğundan ayrı değerlendirilir. Musiki bağlamı, beklenen geçiş, çalgının kimliği ve dinleyici deneyimi önemlidir; tek bir sabit 'beyin puanı' tasarlanmıyor.

1. İzinli gerçek kayıtlar; solo kemanla başlayıp yukarıdaki çalgılar, mikrotonal varyantlar, çift ses ve topluluklara genişletme. Referans iyi icra ile hata eklenmiş kayıtlar ve gerçek hatalı icra örnekleri ayrı test türleri.
2. Aynı hedefte doğal düzeltme açık/kapalı, farklı geçiş ve tını adayları karşılaştırılır. İlgisiz loudness farkı tercihi yönlendirmesin diye dinleme örneklerinde seviye eşleştirme yapılır; ham işlenmiş dosyalar ölçüm için ayrıca korunur.
3. Bağımsız perde ölçümü, yanlış/eksik nota ve oktav sayısı, vibrato, geçiş, atak, kuyruk ve stereo davranışı raporlanır. Analiz motorunun kendi verdiği komut cent ölçümü diye sunulmaz.
4. ViSQOL gibi yardımcı ölçüler yalnız uygun, zaman/perde hedefi eşlenmiş referanslarla değerlendirilir. Bilinçli transpozisyon yapılan kaydı eski referansa benzemedi diye kötü saymak veya aynı skoru bütün çalgılara genellemek uygun değildir. Henüz ViSQOL çalıştırılmadı.
5. Seviye eşlenmiş, yöntem adları gizlenmiş örneklerde çalgı kimliği, doğal geçiş, süsleme korunumu ve duyulan artefaktlar ayrı dinleme soruları olur. Uzman icracı ve kullanıcı değerlendirmesi gerekir. Model beyanı gerçek dinlemenin yerini tutmaz.
6. i5/16 GB bilgisayarda hazırlık süresi, RAM, playback CPU ve dropout; 44.1/48 kHz ve güncel Sonar'da yerel VST3 testleri ayrıca yapılır. Ağır sınıflandırma ve yeniden sentez ses callback'inde çalışmaz.

## 8. Öğrenme, kayıt ve ajan

Okuma ve profesyonel plan birbirinden ayrılacak. Önceden okunan zaman aralıkları saklanır; başa dönmek aynı veriyi tekrar öğrenip histogram ağırlığını artırmaz. Yeni dinlenen aralıklar eklenir. Kaynak değişirse eski analizi geçerli saymamak için hafif ses parmak izi ve açık 'Baştan oku' gerekir.

Normal kanal VST3'ü çalınmamış klibin tamamını kendiliğinden okuyamaz. 'Tüm parça burada' düğmesi mevcut kapsama ilişkin kullanıcı işaretidir; hiç ulaşmamış sese erişim sağlamaz. Grafik okunan/okunmayan zaman aralıklarını gösterir.

Nadir nota, yalnız az görüldüğü için yok sayılmaz. Doğrulanmış yüzde 1'lik nota görünür; gerçekten okunmamış nota 'okunmadı' olarak işaretli ama düzenlenebilir olur. Elle eklenen değer, kayıtta ölçüldü gibi sayılmayacak. Sıklık hesabında nota olayları kullanılır; aynı kaynağın harmonikleri ve tekrar oynatılan önbellek çifte sayılmaz. Çok sesli yüzde paydasının toplam ses-olay süresi olduğu belirtilir; bütün oktavlar ortak nota sınıfına katkı verir.

Yeni bilgiyle üretilen profesyonel planın sürümü ve değişen bölgeleri görünür olur; kullanıcı kilitleri korunur, önceki plan geri alınabilir. Tekrar çalmanın kendiliğinden her seferinde daha iyi sonuç vereceği vaat edilmez. Yeni plan yalnız ek veri veya ayar değişikliği varsa hazırlanır; tamamlanan render sürümü tutarlı bir sınırda devreye alınır.

Ajan, tabela ve ölçümlerin aynı olay kimliklerini kullanır. Uygulanan değişiklikleri ve sınanan hipotezleri açıklayabilir, parametre/bölge komutları verebilir ve ölçümleri çalıştırabilir. Hazırlanan raporda sürüm, kaynak/plan kimliği, zaman, hedef, etkin kontroller, gerçek ölçüm, hata ve kullanıcının yorumu bulunur; kopyalanabilir metin dışa aktarılır. Sonar testini yapmadıysa veya insan doğallığını değerlendirmediyse bunları geçti diye yazamaz.

## 9. Uygulama sırası

Önce saklanan okuma ve hedef modeli, eklentiler arası referans, doğal düzeltme düğmesi ve gerçek işlem tabelasının temeli; sonra tek sesli bağlı olay/atak/süsleme davranışı. Ardından çalgı karşılaştırmaları, çok sesli ayırma ve deneysel yerel yeniden sentez gelir. Ajan yalnız mevcut gerçek motor yeteneklerini kullanır; arayüzde görünmesi eksik DSP yeteneğini tamamlamaz. Kullanıcının istediği gibi bu öneriler önce sunulacak, uygulama ve ses değerlendirmesi bunun ardından yapılacak.
