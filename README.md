# dipl

## Alati
- ESP32 
- INMP441 I2S Mikrofon
- ESP-IDF
	- ESP-DSP biblioteka

![plan](plan1.png)

## Part 1 - Štimer
- Čitati s mikrofona
- Pretvorba u broj (frekvencija -> FFT preko ESP-DSP biblioteke)
- Feedback korisniku - Serial 

## Part 2 - Analiza snimljenog zvuka

## Part 3 - Bolji prikaz feedbacka
- Display, web...

## Potencijalni problemi
- Snaga ESP-a - možda bude potrebno neki jači mikrokontroler
- Performanse mikrofona - potrebno testirati
- Propusnost protokola ESP-NOW ako se koristi
