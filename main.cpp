#include <Servo.h>
#include <IRremote.h>

// --- DEFINICJA PINÓW ---
const int PIN_PIR = 2;
const int PIN_SERVO_L = 3;
const int PIN_SERVO_P = 4;
const int PIN_LED = 5;
const int PIN_IR = 6;

// --- DEFINICJA 6 STANÓW MASZYNY ---
enum StanBramy {
  ZAMKNIETY,
  OTWIERANIE,
  OTWARTY,
  ZAMYKANIE,
  UCHYLONY_OTWARTY,
  UCHYLONY_ZAMKNIETY
};

StanBramy aktualnyStan = ZAMKNIETY;

// --- OBIEKTY I ZMIENNE ---
Servo serwoLewe;
Servo serwoPrawe;

int katBramy = 0;                 // 0 = zamknięta, 45 = uchylona, 90 = otwarta
const int KAT_ZAMKNIETY = 0;
const int KAT_UCHYLONY = 45;
const int KAT_OTWARTY = 90;

unsigned long ostatniCzasRuchu = 0;
const int PREDKOSC_SERWA = 20;    // ms na 1 stopień ruchu

// Zmienne do obsługi 1-sekundowego opóźnienia PIR
unsigned long czasWykryciaPir = 0;
bool pirAktywnyWczesniej = false;

// Kody przycisków pilota w Tinkercad (IRremote)
const uint32_t KLAWISZ_1 = 0xEF10BF00; // Przycisk 1 -> Otwórz
const uint32_t KLAWISZ_2 = 0xEE11BF00; // Przycisk 2 -> Zamknij
const uint32_t KLAWISZ_3 = 0xED12BF00; // Przycisk 3 -> Uchyl

void setup() {
  Serial.begin(9600);
  
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LED, OUTPUT);
  
  serwoLewe.attach(PIN_SERVO_L);
  serwoPrawe.attach(PIN_SERVO_P);
  ustawKatBramy(KAT_ZAMKNIETY);

  IrReceiver.begin(PIN_IR, ENABLE_LED_FEEDBACK);
  Serial.println("System bramy gotowy. Stan: ZAMKNIETY");
}

void loop() {
  unsigned long teraz = millis();
  bool wykrytoRuch = (digitalRead(PIN_PIR) == HIGH);
  uint32_t kodPilota = odczytajPilot();

  // --- MASZYNA STANÓW ---
  switch (aktualnyStan) {

    case ZAMKNIETY:
      // Obsługa czujnika PIR: od razu zapal LED, po 1s ciągłego ruchu otwórz
      if (wykrytoRuch) {
        digitalWrite(PIN_LED, HIGH); // Natychmiastowe światło
        if (!pirAktywnyWczesniej) {
          czasWykryciaPir = teraz;
          pirAktywnyWczesniej = true;
          Serial.println("PIR wykryl ruch - zapalam swiatlo, odliczam 1s...");
        } else if (teraz - czasWykryciaPir >= 1000) {
          pirAktywnyWczesniej = false;
          zmienStan(OTWIERANIE, "OTWIERANIE (wyzwolone przez PIR)");
          break;
        }
      } else {
        pirAktywnyWczesniej = false;
        digitalWrite(PIN_LED, LOW); // Brak ruchu w stanie zamkniętym = LED zgaszony
      }

      // Obsługa pilota
      if (kodPilota == KLAWISZ_1) {
        pirAktywnyWczesniej = false;
        zmienStan(OTWIERANIE, "OTWIERANIE (Pilot 1)");
      } else if (kodPilota == KLAWISZ_3) {
        pirAktywnyWczesniej = false;
        katBramy = KAT_UCHYLONY;
        ustawKatBramy(katBramy);
        zmienStan(UCHYLONY_OTWARTY, "UCHYLONY_OTWARTY (Pilot 3)");
      }
      break;

    case OTWIERANIE:
      digitalWrite(PIN_LED, HIGH); // Światło działa podczas otwierania
      
      if (teraz - ostatniCzasRuchu >= PREDKOSC_SERWA) {
        ostatniCzasRuchu = teraz;
        if (katBramy < KAT_OTWARTY) {
          katBramy++;
          ustawKatBramy(katBramy);
        } else {
          digitalWrite(PIN_LED, LOW); // Koniec otwierania -> gaśnie światło
          zmienStan(OTWARTY, "OTWARTY");
        }
      }

      if (kodPilota == KLAWISZ_2) {
        zmienStan(ZAMYKANIE, "ZAMYKANIE (Pilot 2)");
      } else if (kodPilota == KLAWISZ_3) {
        katBramy = KAT_UCHYLONY;
        ustawKatBramy(katBramy);
        zmienStan(UCHYLONY_OTWARTY, "UCHYLONY_OTWARTY (Pilot 3)");
      }
      break;

    case OTWARTY:
      digitalWrite(PIN_LED, LOW); // W stanie OTWARTY światło jest wyłączone
      
      if (kodPilota == KLAWISZ_2) {
        zmienStan(ZAMYKANIE, "ZAMYKANIE (Pilot 2)");
      } else if (kodPilota == KLAWISZ_3) {
        katBramy = KAT_UCHYLONY;
        ustawKatBramy(katBramy);
        zmienStan(UCHYLONY_ZAMKNIETY, "UCHYLONY_ZAMKNIETY (Pilot 3)");
      }
      break;

    case ZAMYKANIE:
      digitalWrite(PIN_LED, HIGH); // Światło działa podczas zamykania

      // Bezpieczeństwo: czujnik PIR wykrył przeszkodę przy zamykaniu
      if (wykrytoRuch) {
        Serial.println("PIR wykryl przeszkode podczas zamykania - cofam!");
        zmienStan(OTWIERANIE, "OTWIERANIE (cofniecie PIR)");
        break;
      }

      if (teraz - ostatniCzasRuchu >= PREDKOSC_SERWA) {
        ostatniCzasRuchu = teraz;
        if (katBramy > KAT_ZAMKNIETY) {
          katBramy--;
          ustawKatBramy(katBramy);
        } else {
          digitalWrite(PIN_LED, LOW); // Koniec zamykania -> gaśnie światło
          zmienStan(ZAMKNIETY, "ZAMKNIETY");
        }
      }

      if (kodPilota == KLAWISZ_1) {
        zmienStan(OTWIERANIE, "OTWIERANIE (Pilot 1)");
      } else if (kodPilota == KLAWISZ_3) {
        katBramy = KAT_UCHYLONY;
        ustawKatBramy(katBramy);
        zmienStan(UCHYLONY_ZAMKNIETY, "UCHYLONY_ZAMKNIETY (Pilot 3)");
      }
      break;

    case UCHYLONY_OTWARTY:
      digitalWrite(PIN_LED, LOW);
      if (kodPilota == KLAWISZ_1) {
        zmienStan(OTWIERANIE, "OTWIERANIE (Pilot 1)");
      } else if (kodPilota == KLAWISZ_2) {
        zmienStan(ZAMYKANIE, "ZAMYKANIE (Pilot 2)");
      }
      break;

    case UCHYLONY_ZAMKNIETY:
      // Jeśli jest uchylony zamknięty, PIR również może otworzyć bramę po 1s
      if (wykrytoRuch) {
        digitalWrite(PIN_LED, HIGH);
        if (!pirAktywnyWczesniej) {
          czasWykryciaPir = teraz;
          pirAktywnyWczesniej = true;
        } else if (teraz - czasWykryciaPir >= 1000) {
          pirAktywnyWczesniej = false;
          zmienStan(OTWIERANIE, "OTWIERANIE (wyzwolone przez PIR)");
          break;
        }
      } else {
        pirAktywnyWczesniej = false;
        digitalWrite(PIN_LED, LOW);
      }

      if (kodPilota == KLAWISZ_1) {
        pirAktywnyWczesniej = false;
        zmienStan(OTWIERANIE, "OTWIERANIE (Pilot 1)");
      } else if (kodPilota == KLAWISZ_2) {
        pirAktywnyWczesniej = false;
        zmienStan(ZAMYKANIE, "ZAMYKANIE (Pilot 2)");
      }
      break;
  }
}

// --- FUNKCJE POMOCNICZE ---

void ustawKatBramy(int kat) {
  serwoLewe.write(kat);
  serwoPrawe.write(180 - kat);
}

void zmienStan(int nowyStan, const char* nazwaStanu) {
  aktualnyStan = (StanBramy)nowyStan;
  Serial.print("Zmiana stanu -> ");
  Serial.println(nazwaStanu);
}

uint32_t odczytajPilot() {
  if (IrReceiver.decode()) {
    uint32_t odebranyKod = IrReceiver.decodedIRData.decodedRawData;
    if (odebranyKod != 0) {
      Serial.print("Kod pilota HEX: 0x");
      Serial.println(odebranyKod, HEX);
    }
    IrReceiver.resume();
    return odebranyKod;
  }
  return 0;
}
