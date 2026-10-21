# PitchMap Pro 3.0.1 — Windows x64

[Kurulum dosyasını indir (EXE, 8 MB)](MicrotonalPitchMapPro-Setup-x64.exe?raw=true) · [Tam paket (ZIP, 35 MB)](Plugin-Windows-x64.zip?raw=true)

EXE'yi çalıştırın. ZIP kullanıyorsanız önce tamamını çıkarıp içindeki kurulum EXE'sini çalıştırın. Sonar'da VST3 taramasını yenileyin.

Ana ekranda “hedef notaya akortla” veya “kaynağa cent ekle” seçilir. Analiz kaynak -> hedef -> fark -> plan bilgisini gösterir; Uygula gerçek sesi işler. Ölçülebilen çıkış/sapma aynı paneldedir. DINLE / A-B karşılaştırma içindir. WAV başlangıcı Sonar'daki klibin konumuyla aynı olmalıdır; ARA host konumunu kullanır.

Windows PE / Wine ses, durum, ARA ve kurulum kontrolleri geçti. Native Windows, güncel Sonar ve HP laptop testi yapılmadı. Kaynak ve düzeltilmiş örnekler sentetiktir. Test ayrıntıları tam ZIP'teki TEST-REPORT-3.0.1.md içindedir.

[Karşılık gelen kaynak kod](Source.zip?raw=true) · [44.1 kHz ses örnekleri](AudioExamples-44100.zip?raw=true) · [48 kHz ses örnekleri](AudioExamples-48000.zip?raw=true)

Kaynak commit: `22a7d79b35b55eddf73a511954fcec41451aff91`. AGPL-3.0-or-later. Hash'ler SHA256SUMS.txt içindedir. Önceki 0.2.0 ve 3.0.0 paketleri değiştirilmedi; GitHub Release yayımlanmadı.
