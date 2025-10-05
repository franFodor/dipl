<h1 align="center">Diplomski</h1>
 
## Sadržaj
- [Alati](#alati)
- [Razvoj](#razvoj)
- [Potencijalni problemi](#potencijalni-problemi)
- [Dodatni materijali](#dodatni-materijali)

## Alati
- ESP32 
- INMP441 I2S Mikrofon
- ESP-IDF
	- ESP-DSP biblioteka

![plan](./docs/plan1.png)

## Razvoj
### Part 1 - Testiranje mikrofona

### Part 2 - Štimer
- Čitati s mikrofona
- Pretvorba u broj (frekvencija -> FFT preko ESP-DSP biblioteke)
- Feedback korisniku - Serial Monitor 
### Part 3 - Analiza snimljenog zvuka

### Part 4 - Bolji prikaz feedbacka
- Display, web...

## Potencijalni problemi
- Snaga ESP-a - možda bude potrebno neki jači mikrokontroler
- Performanse mikrofona - potrebno testirati
- Propusnost protokola ESP-NOW ako se koristi

## Dodatni materijali
### Dokumentacija
- [ESP - I2S](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html)
- [ESP - DSP](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html)

### Youtube/ostalo
- [YT - How to use INMP441](https://www.youtube.com/watch?v=UkJIMCtsypo)
- [YT - ESP32 Sound - Working with I2S](https://www.youtube.com/watch?v=m-MPBjScNRk)

- [GH - ESP32 Guitar Tuner](https://github.com/LucasWanJZ/ESP32-Guitar-Tuner/tree/main)

- [Serial Monitor Plotter](https://web-serial-plotter.atomic14.com/) 
