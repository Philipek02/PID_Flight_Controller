# PID Flight Controller (STM32)

## **Praca inżynierska: Projekt i implementacja regulatora PID dla kontrolera lotu drona**

Projekt implementacji regulatora **PID / PD / P** dla wielowirnikowego drona, zrealizowany na mikrokontrolerze **STM32L432KC**.  
Repozytorium zawiera kompletną implementację firmware, materiały pomocnicze oraz dokumentację wykorzystaną w pracy inżynierskiej.

## Cel projektu
Celem projektu było:
- zaprojektowanie i implementacja algorytmów regulacji **P / PD / PID**,
- stabilizacja orientacji drona w osiach **Roll / Pitch / Yaw**,
- implementacja sterowania w czasie rzeczywistym na mikrokontrolerze,
- weryfikacja działania regulatorów na rzeczywistym obiekcie.

Projekt stanowi część **pracy inżynierskiej**.

---

## Zastosowane algorytmy
- Regulator **P**
- Regulator **PD**
- Regulator **PID**
- Pętla sprzężenia zwrotnego oparta o dane z IMU
- Filtracja i normalizacja sygnałów wejściowych

Schematy i wizualizacje regulatorów znajdują się w katalogu `materiały pomocnicze`.

---

## Sprzęt
- **Mikrokontroler:** STM32L432KC
- **Czujniki:** IMU (żyroskop + akcelerometr)
- **Sterowanie:** Aparatura RC
- **Napędy:** Silniki BLDC + ESC (BLHeli)
- **Komunikacja:** UART, I2C, TIM (PWM)

---

## Oprogramowanie
- **IDE:** STM32CubeIDE
- **Framework:** STM32 HAL + CMSIS
- **Język:** C
- **Konfiguracja:** STM32CubeMX (`.ioc`)

---

## Struktura projektu

PID_Flight_Controller-main/  
├── IMPLEMENTACJA/  
│   ├── Core/  
│   ├── Drivers/  
│   ├── Debug/  
│   ├── IMPLEMENTACJA.ioc  
│   └── STM32L432KCUX_FLASH.ld  
├── materiały pomocnicze/  
│   ├── diagramy blokowe/  
│   ├── schematy połączeń/  
│   ├── wizualizacje regulatorów/  
│   └── zdjęcia stanowiska/  
├── Praca_Inżynierska_Filip_Pańczak.pdf  
└── README.md



---

## Uruchomienie projektu
1. Otwórz `STM32CubeIDE`
2. Załaduj projekt z katalogu `IMPLEMENTACJA`
3. Sprawdź konfigurację pinów w `IMPLEMENTACJA.ioc`
4. Zbuduj projekt (`Build`)
5. Wgraj firmware na STM32 (`Debug / Run`)

---

## Strojenie regulatora PID
Parametry regulatora:
- `Kp`
- `Ki`
- `Kd`

Dobierane eksperymentalnie — wizualizacje oraz przebiegi dostępne w katalogu:
materiały pomocnicze/

---

## Testy
- Testy statyczne na stanowisku
- Testy dynamiczne z aparaturą RC
- Analiza odpowiedzi układu na zakłócenia
- Podgląd danych przez UART

---

## Dokumentacja
- Praca inżynierska (PDF)
- Schematy połączeń
- Diagramy blokowe algorytmów
- Zdjęcia i wizualizacje stanowiska testowego

---

## Status projektu
Projekt **ukończony** — rozwijany w ramach pracy inżynierskiej.  
Repozytorium ma charakter **edukacyjny i badawczy**.

---

## Autor
**Filip Pańczak**  
Praca inżynierska – elektronika / systemy wbudowane / sterowanie

---

## Licencja
Projekt udostępniony do celów edukacyjnych.  
Wykorzystanie komercyjne wymaga zgody autora.
