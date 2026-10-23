# 3.1.1 — arayüz dili

Sağ üst köşedeki dil menüsü: English, 中文（简体）, Deutsch, Türkçe, Français, العربية, Español, Português, Italiano. Yeni örnek ilk açılışta sistem dilinden bağımsız olarak İngilizce açılır. Seçim eklenti örneğine aittir ve Sonar proje durumuyla kaydedilir; dil parametresi olmayan eski proje İngilizce açılır. Başka kanaldaki eklentinin dili değişmez.

Ana ekran, ek ayar sayfaları, düğmeler, durum açıklamaları, işlem geçmişi ve teknik rapor başlıkları yerel katalogdan çevrilir. Bunun için İnternet veya AI anahtarı gerekmez. Nota adları, cent/sayı değerleri, Hz/ms/dB, MIDI/VST3 ve standart simgeler korunur. Dil seçimi ses parametrelerini, hedef centleri veya hazırlanmış sesin geçerliliğini değiştirmez.

Kullanıcı, kanal ve model adları ile sohbetin geçmişte yazılmış içeriği otomatik çevrilmez. Yeni AI isteği seçilen dilde yanıt ister; servis yine ayrı API anahtarı gerektirir ve anahtarsız test edilmiş sayılmaz. Tarihsel eski klip editörüyle açılan v1–v5 projelerin eski iş akışı korunur; bu dil menüsü yeni canlı çalışma arayüzündedir.

Latin, Çince ve Arapça arayüz yazıları için Noto tabanlı OFL lisanslı alt yazı tipleri gömülüdür; temel arayüz metni sistem yazı tipinin bulunmasına bağlı değildir. Arapça birleşik harf ve sağdan sola yazı düzeniyle çizilir; nota/zaman grafiğinin yönü değişmez. Paket içi Çince font arayüz kataloğunun karakterlerini kapsar; kullanıcı tarafından girilen katalog dışı karakterlerde işletim sistemi font desteği gerekir. Font kaynakları, hash değerleri ve alt küme üretim betiği Assets/Fonts içindedir.

Bu sürüm perde işleme algoritmasına yeni özellik eklemez. Önceki 3.1.0 ses ölçümleri ve sınırlamaları geçerlidir: bağımsız polifonik nota ayırma ve garanti edilmiş doğal yeniden icra yoktur. Gerçek Windows/Sonar/HP laptop testi bu bulut ortamında yapılmadı. 3.1.1 doğrulama sonuçları TEST-REPORT-3.1.1.md dosyasına kaydedilir.
