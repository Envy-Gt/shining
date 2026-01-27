#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



#include <Encoder.h>

#define POMPE1_PIN 12
#define POMPE2_PIN 7

bool pompe1Active = false;
unsigned long pompe1Start = 0;


// --- OLED ---
#define SCREEN_WIDTH 128

#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- KY-040 Encoder ---
#define ENCODER_CLK 4
#define ENCODER_DT  5
#define ENCODER_SW  6   // Single push button on KY-040
Encoder myEnc(ENCODER_CLK, ENCODER_DT);

// --- Buzzer ---
#define BUZZER_PIN 3


// --- Stepper 28BYJ-48 (ULN2003) ---
#define STEPPER_PIN1 8
#define STEPPER_PIN2 9
#define STEPPER_PIN3 10
#define STEPPER_PIN4 11

int stepIndex = 0;

// Séquence demi-pas
int stepSequence[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};
unsigned long lastStepTime = 0;
const unsigned long stepInterval = 3; // ms entre pas (réglable)


// --- Timer variables ---
bool timerActif = false;
unsigned long timerDebut = 0;
unsigned long timerDuree = 0;

void setup() {
  Serial.begin(9600);

  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);

  pinMode(POMPE1_PIN, OUTPUT);
  pinMode(POMPE1_PIN, OUTPUT);
  digitalWrite(POMPE1_PIN, LOW);
  digitalWrite(POMPE1_PIN, LOW);


  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("Erreur SSD1306"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println(F("Reglage timer"));
  display.display();

  playEndMusic();
}

void loop() {
  if (!timerActif) {
    int dureeMinutes = setTimer();
    if (dureeMinutes > 0) {
      timerDuree = dureeMinutes * 60000UL;
      timerDebut = millis();
      timerActif = true;

      Serial.print("Timer demarre : ");
      Serial.print(dureeMinutes);
      Serial.println(" min");

      beepBuzzer();
    }
  } else {
    executerTimer();
  }
}

// --- Réglage du timer avec un seul bouton KY-040 ---
int setTimer() {
  int duree = 0;
  bool valide = false;

  long lastEncoderValue = myEnc.read();
  int lastDisplayedDuree = -1;

  unsigned long pressStart = 0;

  while (!valide) {
    // --- ENCODER ROTATION ---
    long encoderValue = myEnc.read();
    long delta = -(encoderValue - lastEncoderValue) / 2;

    if (delta != 0) {
      duree += (delta > 0) ? 10 : -10;
      duree = constrain(duree, 0, 360);
      tone(BUZZER_PIN, 800, 20);
      lastEncoderValue = encoderValue;
    }

    int heures = duree / 60;
    int minutes = duree % 60;

    // --- DISPLAY UPDATE ---
    if (duree != lastDisplayedDuree) {
      lastDisplayedDuree = duree;

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println(F("Reglage du timer :")); 
      display.setCursor(0, 10);
      display.println(F("Tournez encodeur"));
      display.setCursor(0, 20);
      display.println(F("Appui=OK  Long=Annuler"));

      display.setTextSize(2);
      display.setCursor(10, 35);
      if (heures > 0) {
        display.print(heures);
        display.print("h");
        if (minutes < 10) display.print("0");
        display.print(minutes);
        display.print("m");
      } else {
        if (duree < 10) display.print(" ");
        display.print(duree);
        display.print(" min");
      }

      display.drawRect(0, 55, 128, 8, SSD1306_WHITE);
      int progressWidth = map(duree, 0, 360, 0, 126);
      display.fillRect(1, 56, progressWidth, 6, SSD1306_WHITE);

      display.display(); 
    }

    // --- BUTTON PRESS : SHORT = VALIDATE ---
    if (digitalRead(ENCODER_SW) == LOW) {
      delay(30);
      if (digitalRead(ENCODER_SW) == LOW) {

        pressStart = millis();
        while (digitalRead(ENCODER_SW) == LOW) {
          if (millis() - pressStart > 800) {
            // --- LONG PRESS = CANCEL ---
            afficherMessage("Annulation...");
            tone(BUZZER_PIN, 400, 200);
            delay(1000);
            return 0;
          }
        }

        // SHORT PRESS = validate
        beepBuzzer();
        return duree;
      }
    }

    delay(2);
  }

  return duree;
}

// --- Exécution du timer ---
void executerTimer() {
  unsigned long tempsEcoule = millis() - timerDebut;
  unsigned long tempsRestant = (timerDuree > tempsEcoule) ? (timerDuree - tempsEcoule) : 0;

  // --- Pompe 1 : ON pendant 30s au début ---
  if (!pompe1Active) {
    pompe1Active = true;
    pompe1Start = millis();
    digitalWrite(POMPE1_PIN, HIGH);
  }

  if (pompe1Active && millis() - pompe1Start >= 210000UL) {
    digitalWrite(POMPE1_PIN, LOW);
  }

  // --- Cancel with button press ---
  if (digitalRead(ENCODER_SW) == LOW) {
    delay(50);
    if (digitalRead(ENCODER_SW) == LOW) {
      timerActif = false;
      afficherMessage("Timer annule!");
      beepBuzzer();

      // --- Pompe 2 : ON 30s en cas d'annulation ---
      digitalWrite(POMPE2_PIN, HIGH);
      delay(210000);
      digitalWrite(2_PIN, LOW);

      while (digitalRead(ENCODER_SW) == LOW);
      delay(100);
      return;
    }
  }

  // --- Timer finished ---
  if (tempsEcoule >= timerDuree) {
    timerActif = false;
    afficherMessage("Timer termine!");
    playEndMusic();

    // --- Pompe 2 : ON 30s fin du timer ---
    digitalWrite(POMPE2_PIN, HIGH);
    delay(210000);
    digitalWrite(POMPE2_PIN, LOW);

    delay(2000);
    return;
  }

  // --- Stepper movement ---
  runStepper();

  // --- Display each second ---
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();

    int heuresRestantes = tempsRestant / 3600000UL;
    int minutesRestantes = (tempsRestant % 3600000UL) / 60000UL;
    int secondesRestantes = (tempsRestant % 60000UL) / 1000UL;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Timer en cours..."));
    display.setCursor(0, 16);
    display.println(F("Appui = Annuler"));

    display.setTextSize(2);
    display.setCursor(10, 35);

    char buffer[16];
    if (heuresRestantes > 0) {
      sprintf(buffer, "%02dh%02dm%02ds", heuresRestantes, minutesRestantes, secondesRestantes);
    } else if (minutesRestantes > 0) {
      sprintf(buffer, "%02dm%02ds", minutesRestantes, secondesRestantes);
    } else {
      sprintf(buffer, "%02ds", secondesRestantes);
    }
    display.print(buffer);

    display.display();
  }
}



// --- Utilities ---
void afficherMessage(const char* message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 25);
  display.println(message);
  display.display();
}

void beepBuzzer() {
  tone(BUZZER_PIN, 1000, 500);
  delay(500);
}

void playEndMusic() {
  tone(BUZZER_PIN, 262, 200);
  delay(200);
  tone(BUZZER_PIN, 294, 200);
  delay(200);
  tone(BUZZER_PIN, 330, 200);
  delay(200);
  noTone(BUZZER_PIN);
}

void runStepper() {
  if (millis() - lastStepTime >= stepInterval) {
    lastStepTime = millis();
    stepMotor(true); // sens horaire
  }
}

void stepMotor(bool sensHoraire) {
  if (sensHoraire) {
    stepIndex = (stepIndex + 1) % 8;
  } else {
    stepIndex = (stepIndex + 7) % 8;
  }

  digitalWrite(STEPPER_PIN1, stepSequence[stepIndex][0]);
  digitalWrite(STEPPER_PIN2, stepSequence[stepIndex][1]);
  digitalWrite(STEPPER_PIN3, stepSequence[stepIndex][2]);
  digitalWrite(STEPPER_PIN4, stepSequence[stepIndex][3]);
}

  
