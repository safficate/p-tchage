# Microtonal Pitch Map Pro — 0.1.0

JUCE/C++20 ile yazılmış, tek sesli kaynaklar için offline mikrotonal düzeltme eklentisi. Gerçek analiz, ses transpozisyonu ve WAV render motoru içerir. Ağır işlemler arka planda yapılır; DAW callback'i hazır PCM sesi zaman çizelgesinden okur.

**Test durumu ve teslim edilen platformlar `docs/TEST-REPORT.md` dosyasındadır. Gerçek Sonar testi yapılmadığı sürece Sonar uyumluluğu doğrulanmış değildir.**

Windows x64 test paketi gerçek VST3 bundle, Standalone uygulama ve `MicrotonalPitchMapPro-Setup-x64.exe` kurulum dosyasını içerir. Windows ses motoru, DLL yükleme ve kurulum/kaldırma kontrolleri Wine altında çalıştırıldı. Windows arayüz çizim testi Wine altında tamamlanamadı; gerçek Windows ve Sonar üzerinde kabul testi gerekiyor. Kurulum adımları `WINDOWS-KURULUM.txt`, kapsam ve sonuçlar test raporundadır.

## Kullanım

1. Eklentiyi mono veya stereo ses kanalının FX bölümüne ekleyin. Ses aygıtını ve sürücüsünü Sonar yönetir. Harici ses kartı veya MIDI donanımı gerekmez.
2. `Load WAV` ile en fazla 120 saniyelik tek sesli WAV/AIFF/FLAC yükleyin. Alternatif: Sonar oynatımını başlatıp `Capture`, ardından `Stop capture` kullanın. Yakalama, eklentiye gelen sesi kaydeder; host transport konumu başlangıç olarak saklanır.
3. `Analyze` kullanın. Sarı çizgi ölçülen pitch eğrisidir; renkli bloklar notaları gösterir. Düşük güvenli ölçümler kesin nota kabul edilmez. Spektrogram ve dalga formu kaynak sesten hesaplanır.
4. Örneğin C hedefini `-20 c` yapın. **QUANTIZE** `0` ise merkez perdesi değişmez; `0.5` yarı düzeltme, `1` tam düzeltmedir. Grup `Strength x Quantize` değeri bu ana miktarı çarpar. Hedef ve düzeltme miktarı ayrı kontrollerdir.
5. Değişiklikten sonra offline render otomatik yenilenir; `Render` ile elle de başlatılabilir. `Preview` kaynak dosyasını dinletir. DAW oynatımında düzeltmeyi kullanmak için **DAW Render** düğmesini açın. İçe aktarılan dosyanın başlangıcı varsayılan 0 saniyedir; `Timeline offset` ile konumlandırın. Bu modda yüklenen sesin zaman çizelgesindeki karşılığı kullanılır; farklı bir girişi kendiliğinden analiz etmez.
6. `Export WAV` düzeltilmiş 24-bit WAV oluşturur. Kaynak dosya değiştirilmez. Var olan dosyaların üzerine yazılmaz; yeni ad seçin. `MIDI` monofonik notaları pitch-bend mesajları ve açık +/-2 semiton bend aralığıyla dışa aktarır. MIDI alıcısının aynı bend aralığını desteklemesi gerekir; bu aralığı aşan uzun glide hareketleri bend sınırında kesilir. MIDI velocity değeri bu sürümde sabit 96'dır.

Hedef merkez: `orijinal + strength × (hedef − orijinal)`. Merkez düzeltmesinde orijinal vibrato korunur. Render çıktısı güvenli iç bölgelerde yeniden ölçülür; küçük spectral pitch hataları en fazla dört offline turla azaltılır. Kısa veya güvensiz notalarda bu ek ölçüm uygulanamayabilir. Tightness, Vibrato preservation, seed'li Humanize, geçiş yumuşatma, formant kompanzasyonu ve Wet/Dry bağımsız kontrollerdir. Quantize 0 iken ayrıca Humanize/Tightness açılmışsa bu ayrı işlemler sesi değiştirebilir.

## Nota düzenleme

- Bir notayı seçip dikey sürükleyin; gruptan ayrılır ve bağımsız hedef edinir. `Set note` yanındaki alanlar hedef cent ve strength yüzdesidir. `Rejoin group` tekrar gruba bağlar.
- Shift+tıklama ile birden fazla nota seçin; `Merge` seçilen aralığı birleştirir. `Split` seçili notayı ortadan böler.
- Fare tekerleği zoom; Shift+tekerlek yatay kaydırma. Ctrl+Z / Ctrl+Y nota düzenleme undo/redo işlemleridir. Grup parametreleri DAW'ın otomasyon/undo sistemine bildirilir.
- `Octave mode` ve oktav seçicisiyle C3/C4 gibi hedefleri ayırın. Kapalıyken aynı nota sınıfının bütün oktavları ortak hedef kullanır.
- Referans akort alanına 432, 440, 442 veya 400–480 Hz aralığında başka bir değer girilebilir. Scala içe aktarma yalnızca 12 adımlı, oktavla biten `.scl` dosyalarını destekler; kök C'dir.
- `A: Original` orijinal sesi seçer. `Bypass` DAW girişini değiştirmeden geçirir.

## Proje ve preset

Parametreler, bağımsız nota ayarları, harita, yakalama başlangıcı ve **kaynak PCM ses** plugin state'ine yazılır. Dış dosya silinse bile state kaynağı korur; proje dosyası büyüyebilir. Yeniden açılışta render arka planda yenilenir. **DAW oynatımı/render öncesi `Render ready` durumunu bekleyin.** `Save preset`/`Load preset` aynı veriyi `.mpmap` olarak taşır.

## Derleme

Windows: Visual Studio 2022 C++ Desktop (Windows SDK dahil), CMake 3.24+, Git. Derleyici mimarisi x64 olmalıdır.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

VST3: `build/MicrotonalPitchMapPro_artefacts/Release/VST3/Microtonal Pitch Map Pro.vst3`.

Windows kurulum klasörü: `C:\Program Files\Common Files\VST3\Microtonal Pitch Map Pro.vst3\Contents\x86_64-win\Microtonal Pitch Map Pro.vst3`. **Bütün `.vst3` bundle klasörünü** kopyalayın. `.github/workflows/windows.yml` MSVC Windows derlemesini, testleri ve NSIS kurulum paketini otomatik üretir. Bu workflow eklenmesi, workflow'un çalıştığı veya Sonar testlerinin geçtiği anlamına gelmez.

Linux: GCC 12+/Clang, CMake 3.24+, Ninja, ALSA/X11/Xext/Xinerama/Xrandr/Xcursor/Freetype/Fontconfig/OpenGL geliştirme paketleri. `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, `cmake --build build --parallel 2`, `ctest --test-dir build --output-on-failure`. Grafik testleri için çalışan X display veya Xvfb gerekir.

JUCE ve Signalsmith bağımlılıkları sabit commit'lerle indirilir. `JUCE_DIR`, `FETCHCONTENT_SOURCE_DIR_STRETCH`, `FETCHCONTENT_SOURCE_DIR_LINEAR` ile mevcut checkout'lar kullanılabilir. JUCE 8 **MinGW desteklemez**; Windows'ta MSVC veya uyumlu Clang/MSVC SDK kullanın.

## Sınırlar

Bu sürüm tek sesli bir stem içindir. Polifonik kaynak ayırma, ONNX modelleri, tek pencerede birden çok stem, otomatik makam tanıma, özel transient koruma kontrolü ve ARA uygulanmadı. Birden çok enstrümanda her stem için ayrı eklenti instance'ı kullanın. YIN/autokorelasyon tabanlı analiz; karmaşık seslerde oktav/nota hataları olabilir. Süsleme ve geçiş sınıflandırması ayrı etiketler olarak sunulmaz. Hassasiyet ve doğal tını her kayıt türünde ayrıca dinlenerek değerlendirilmelidir.

Sınır: mono/stereo, en fazla 120 saniye, dosyada 8–192 kHz; esas test hedefleri 44.1/48 kHz. En yüksek playback kalitesi için WAV ve proje örnekleme hızını eşleştirin; farklı hızlarda playback lineer enterpolasyon kullanır, WAV export kaynak hızında kalır. Parametre değişikliği offline render gerektirir; sample-accurate canlı cent otomasyonu hedeflenmez. Yeni render hazırlanırken önceki geçerli render kullanılabilir. Performans hedef laptop üzerinde henüz ölçülmedi.

## Lisans

Bu kaynak sürümü **AGPL-3.0-or-later** olarak sunulur; JUCE 8'in açık kaynak lisans yolunu kullanır. Signalsmith Stretch ve Linear MIT lisanslıdır. Bağımlılık bildirimleri `licenses/` içindedir. Kapalı kaynak dağıtım için JUCE'nin uygun ticari lisansı ve gerekli diğer haklar ayrıca sağlanmalıdır.
