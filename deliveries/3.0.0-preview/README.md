# PitchMap Pro 3.0 — Windows x64 önizleme

[**Windows VST3 + kurulum paketini indir (18 MB)**](Plugin-Windows-x64.zip?raw=1). ZIP’i tamamen çıkarıp `MicrotonalPitchMapPro-Setup-x64.exe` dosyasını çalıştırın. Manuel kurulum için VST3 klasörünün tamamı da pakettedir. VST3, standalone, lisanslar, test kayıtları ve isteğe bağlı DLL kontrol araçları birlikte gelir.

Üst sıra kaynak nota parçalarını, alt sıra ayarlanabilir hedef tonları çalar. Klip analizi → hedefi gözden geçir → uygula → dinle → düzelt. Yeni bölgesel motor offline çalışır; canlı kanal mevcut streaming motorunu kullanır. Normal FX tüm proje kliplerini kendiliğinden alamaz; bunun için desteklenen ARA/Region FX veya WAV yükleme gerekir.

- [Karşılık gelen kaynak ve sabitlenmiş bağımlılıklar (33 MB)](Source.zip?raw=1)
- [44,1 kHz sentetik önce/sonra/AB sesleri (30 MB)](AudioExamples-44100.zip?raw=1)
- [48 kHz sentetik önce/sonra/AB sesleri (33 MB)](AudioExamples-48000.zip?raw=1)
- [Gerçek test raporu ve kalan işler](../../docs/TEST-REPORT-3.0.md)
- [SHA-256 dosya özetleri](SHA256SUMS.txt)

Kaynak commit: `13cde1dca660ae068943e3861ec4e2bb83f4c087`. Linux 6/6 test grubu; Windows PE/Wine bölgesel ses testleri ve gerçek DLL’in 29 ses senaryosu geçti. Kurulan DLL’in hash’i paketle eşleşti; kurulum/kaldırma denendi. 36 kontrollü sentetik örnek vardır; gerçek enstrüman kaydı veya insan dinleme paneli değildir. Sabit ±3/5/20 cent örneklerinde bağımsız spektral hata en çok 0,94 cent; her kayıtta bu hassasiyet garanti edilmez.

**Sonar, native Windows masaüstü ve hedef HP bilgisayar testleri yapılmadı.** Kalıcı sidechain haritası öğrenme, AI asistanı, bağımsız polifonik düzeltme ve özel piyano modeli tamamlanmadı. Bazı karmaşık bölgelerde hassasiyet teyidi yoktur; raporda açıkça listelenir. Bu bir mühendislik önizlemesidir. Eski 0.2.0 dağıtım dosyaları değiştirilmedi; yeni GitHub Release yayımlanmadı.

AGPL-3.0-or-later. Tam karşılık gelen kaynak ve bağımlılık lisansları sağlanır.
