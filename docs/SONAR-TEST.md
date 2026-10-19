# Cakewalk Sonar Windows x64 — zorunlu kabul testi

**Durum: ÇALIŞTIRILMADI.** Linux, Wine veya otomatik DSP testleri gerçek Sonar doğrulaması yerine geçmez.

Test tarihinde mevcut en güncel kararlı Windows Sonar sürümünü kullanın; sürümü varsaymayın. Şunları rapora yazın: test tarihi, tam Sonar build numarası, Windows 11 build'i, HP EliteBook/i5 11. nesil/16 GB bilgisi, dahili ses sürücüsü adı/modu, buffer boyutu ve örnekleme hızı.

Her iki projede (44.1 ve 48 kHz), dahili ses sistemiyle ve harici ses/MIDI donanımı olmadan:

1. x64 VST3 bundle'ı standart klasöre kurun. Sonar plugin taramasında bulunmasını ve hata vermemesini doğrulayın.
2. Mono ve stereo kanallara ekleyin; arayüzü açıp kapatın. Varsayılan Quantize 0 ve DAW Render kapalıyken sesin değişmediğini kontrol edin.
3. 440 Hz test WAV'ı yükleyip Analyze çalıştırın. A4 merkezi 440 Hz civarında olmalı. A hedefini +/-20 cent'e, Quantize'ı 0/0.5/1'e getirip gerçek WAV çıktısının frekansını ölçün.
4. Aynı pitch class'ın farklı zaman/oktavlardaki bütün notaları grup hedefini izlemeli. Octave mode ve detached note ayrı hedef uygulamalı.
5. Kanal sesini Capture ile alın; Stop capture, Analyze, Render sırasını izleyin. DAW Render'ı açıp host zaman çizelgesinde doğru konumdan playback başlatın. Loop, seek, stop/start ve buffer boyutu değişikliklerini deneyin.
6. A/B, bypass, Wet/Dry ve formant kontrolünü deneyin. Stereo fazını, transient/vibrato artefaktlarını sesle ve ölçümle değerlendirin. Düşük güvenli/polifonik bölümleri ayrıca inceleyin.
7. `Render ready` sonrası gerçek zamanlı playback, Sonar offline bounce/export ve WAV export sonuçlarını karşılaştırın. Dropout, xrun, peak/clipping, CPU ve RAM ölçümlerini kaydedin.
8. Parametreleri, bağımsız notaları, grup modunu ve zaman konumunu değiştirip Sonar projesini kaydedin. Sonar'ı kapatıp yeniden açın; kaynak dosyasını geçici olarak başka yere taşıyarak gömülü sesin geri yüklenmesini de kontrol edin. Render bitince ses ve harita önceki sonuçla aynı olmalı.
9. Plugin'i kaldırıp tekrar ekleyin, preset'i kaydedip yükleyin. Windows kurulum/kaldırma paketini test edin.

Her adım için geçer/kalır, ölçülen değerler ve hata ayrıntılarını kaydedin. Bu belge bir test prosedürüdür; tamamlanmış test raporu değildir.
