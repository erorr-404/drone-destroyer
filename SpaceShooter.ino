#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pitches.h"

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define POT A3
#define BTN 6
#define BUZ 10
#define ROCKET_BTN 4
#define ROCKET_LED 5

#define MAX_BULLETS 16
#define MAX_ENEMIES 8
#define ROCKET_COOLDOWN 5000
#define EXPLOSION_RAD 50
#define BULLET_COST 1
#define ROCKET_COST 4
#define DRONE_VALUE 2
#define DRONE_VALUE_MULT 2
#define SCORE_INCREASEMENT 1
#define SCORE_DECREASEMENT 2
#define SPEED_INCREASEMENT_TIME 30000

unsigned int potValue;
int jetPos;
bool buttonState;
bool buttonPrevState;

bool drawExplosion;
int explosionRad = 5;

unsigned int score = 0;
unsigned int budget = 10;
unsigned short int speed = 3;
unsigned long int lastSpeedIncreasementTime = 0;

unsigned int highestScore;

// 'jet', 15x15px
const unsigned char epd_bitmap_jet[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x0e, 0x00, 0x8f, 0x00, 0xcf, 0x80, 0xff, 0xf0, 0x7f, 0xfe,
  0xff, 0xf0, 0xcf, 0x80, 0x8f, 0x00, 0x0e, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'enemy', 15x15px
const unsigned char epd_bitmap_enemy[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x7c, 0x01, 0xfc, 0x07, 0xf8, 0x7f, 0xfa, 0xff, 0xfe,
  0x7f, 0xfa, 0x07, 0xf8, 0x01, 0xfc, 0x00, 0x7c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00
};

// 'rocket', 15x7px
const unsigned char epd_bitmap_rocket[] PROGMEM = {
  0x30, 0x00, 0x3c, 0x08, 0x7f, 0xfc, 0xff, 0xfe, 0x7f, 0xfc, 0x3c, 0x08, 0x30, 0x00
};

const int megalovaniaLength = 63;
const int noteDuration = 150;
int noteNumber = 0;
long int lastNoteTime = 0;
const int megalovaniaNotes[megalovaniaLength]{
  NOTE_D3, NOTE_D3, NOTE_D4, 0, NOTE_A3, 0, NOTE_GS3, 0, NOTE_G3, 0, NOTE_F3, 0, NOTE_D3, NOTE_F3,
  NOTE_G3, NOTE_C3, NOTE_C3, NOTE_D4, 0, NOTE_A3, 0, 0, NOTE_GS3, NOTE_G3, 0, NOTE_F3, 0, NOTE_D3,
  NOTE_F3, NOTE_G3, NOTE_B2, NOTE_B2, NOTE_D4, 0, NOTE_A3, 0, 0, NOTE_GS3, 0, NOTE_G3, 0, NOTE_F3, 0,
  NOTE_D3, NOTE_F3, NOTE_G3, NOTE_AS2, NOTE_AS2, NOTE_D4, 0, NOTE_A3, 0, 0, NOTE_GS3, 0, NOTE_G3, 0,
  NOTE_F3, 0, NOTE_D3, 0, NOTE_F3, NOTE_G3
};

// struct to save bullet data
struct bulletDataType {
  int x;
  int y;
  bool isActive;
};

// array of all bullets
bulletDataType bullets[MAX_BULLETS];

// rocket structure
struct rocketDataType {
  int x;
  int y;
  bool isActive;
};

// rocket instance
rocketDataType rocket = { 0, 0, false };
bool rocketReady = true;
long int lastRocketFiredTime;

// enemy struct
struct enemyDataType {
  int x;
  int y;
  bool isActive;
};

// array of all enemies
enemyDataType enemies[MAX_ENEMIES];

void setup() {
  Serial.begin(9600);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(ROCKET_BTN, INPUT_PULLUP);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000);  // Pause for 2 seconds
}

void loop() {
  // get data from button and potentiometer
  potValue = analogRead(POT);
  jetPos = map(potValue, 0, 1024, -14, 64);
  buttonState = digitalRead(BTN);

  if (buttonState == LOW and buttonPrevState == HIGH) {
    fireBullet(jetPos);
    playFireSound();
  }

  if (digitalRead(ROCKET_BTN) == LOW) {
    fireRocket(jetPos);
  }

  if (random(100) < 5) {
    spawnEnemy();
  }

  // update rocket
  updateRocket(2);
  checkRocketExplosion(EXPLOSION_RAD);

  // update bullets and enemies positions
  updateBullets(3);
  updateEnemies(speed);
  checkCollisions();

  // clear display
  display.clearDisplay();
  // render a jet
  display.drawBitmap(0, jetPos, epd_bitmap_jet, 15, 15, 1);
  // render all active bullets, rocket and enemies
  drawBullets();
  drawEnemies();
  drawRocket();

  if (drawExplosion) {
    drawExplosionRadius(explosionRad);
    
    if (explosionRad >= 30) {
      explosionRad = 0;
      drawExplosion = false;
    }

    explosionRad += 5;
  }
  renderScore();
  renderBudget();

  // display frame on OLED display
  display.display();
  buttonPrevState = buttonState;

  if (millis() - lastRocketFiredTime >= ROCKET_COOLDOWN) {
    digitalWrite(ROCKET_LED, HIGH);
    rocketReady = true;
  }

  // music handle
  if (millis() - lastNoteTime >= noteDuration) {
    tone(BUZ, megalovaniaNotes[noteNumber], noteDuration);
    if (noteNumber >= megalovaniaLength) {
      noteNumber = 0;
    } else {
      noteNumber++;
    }
    lastNoteTime = millis();
  }

  if (millis() - lastSpeedIncreasementTime >= SPEED_INCREASEMENT_TIME) {
    speed += 1;
    lastSpeedIncreasementTime = millis();
  }
}

void renderBudget() {
  display.setCursor(0, SCREEN_HEIGHT-9);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("BDGT: ");
  display.println(budget);
}

void playExplosionSound() {
  tone(BUZ, 1000, 50);
}

void playFireSound() {
  tone(BUZ, 300, 50);
}

void playDestroySound() {
  tone(BUZ, 500, 50);
}

void renderScore() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  display.println(score);
}

void fireRocket(int jetPos) {
  if (rocketReady && budget >= ROCKET_COST) {
    rocket = { 15, jetPos, true };
    rocketReady = false;
    lastRocketFiredTime = millis();
    budget -= ROCKET_COST;
  }
  digitalWrite(ROCKET_LED, LOW);
}

void drawExplosionRadius(int radius) {
    if (rocket.isActive) return; // Only draw if the rocket has exploded

    display.drawCircle(rocket.x + 7, rocket.y + 3, radius, WHITE);
}


void drawRocket() {
  if (rocket.isActive) {
    display.drawBitmap(rocket.x, rocket.y - 3, epd_bitmap_rocket, 15, 7, 1);
  }
}

void updateRocket(int movement) {
  if (rocket.isActive) {
    rocket.x += movement;
  }
}

void checkRocketExplosion(int explosionRadius) {
    if (!rocket.isActive) return; // No explosion if rocket is not active

    // Check if rocket collides with any enemy
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].isActive) {
            if (rocket.x + 15 >= enemies[i].x && rocket.x <= enemies[i].x + 15 &&
                rocket.y + 7 >= enemies[i].y && rocket.y <= enemies[i].y + 15) {
                
                unsigned int destroyedEnemies = 0;
                // Explosion happens, destroy all enemies in range
                for (int j = 0; j < MAX_ENEMIES; j++) {
                    if (enemies[j].isActive) {
                        int dx = enemies[j].x - rocket.x;
                        int dy = enemies[j].y - rocket.y;
                        int distanceSquared = dx * dx + dy * dy;

                        // Destroy enemies inside the explosion radius
                        if (distanceSquared <= explosionRadius * explosionRadius) {
                            enemies[j].isActive = false;
                            score += SCORE_INCREASEMENT;
                            destroyedEnemies += 1;
                        }
                    }
                }

                playExplosionSound(); // Explosion sound
                budget += DRONE_VALUE * destroyedEnemies * DRONE_VALUE_MULT;
                drawExplosion = true;
                rocket.isActive = false; // Remove rocket after explosion
                if (score > highestScore) highestScore = score;
                return; // Exit function since rocket is gone
            }
        }
    }
}


void fireBullet(int jetPos) {
  if (budget >= BULLET_COST) {
    for (int i = 0; i < MAX_BULLETS; i++) {
      if (!bullets[i].isActive) {
        bullets[i] = { 16, jetPos + 7, true };
        budget -= BULLET_COST;
        break;
      }
    }
  } else {
    gameOverScreen();
  }
}

void drawBullets() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].isActive) {
      display.drawLine(bullets[i].x, bullets[i].y, bullets[i].x + 3, bullets[i].y, WHITE);
    }
  }
}

// move all bullets to the right for specific value
void updateBullets(int bulletMovement) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].isActive) {
      bullets[i].x += bulletMovement;
      if (bullets[i].x >= SCREEN_WIDTH) {
        bullets[i].isActive = false;
      }
    }
  }
}

void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].isActive) {
      enemies[i] = { 128, random(0, 49), true };
      break;
    }
  }
}

void updateEnemies(int movement) {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].isActive) {
      enemies[i].x -= movement;
      if (enemies[i].x <= -15) {
        enemies[i].isActive = false;
        if (score >= SCORE_DECREASEMENT) {
          score -= SCORE_DECREASEMENT;
        } else {
          gameOverScreen();
        }
      }
    }
  }
}

void drawEnemies() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].isActive) {
      display.drawBitmap(enemies[i].x, enemies[i].y, epd_bitmap_enemy, 15, 15, WHITE);
    }
  }
}

void checkCollisions() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].isActive) continue;

    for (int j = 0; j < MAX_ENEMIES; j++) {
      if (!enemies[j].isActive) continue;

      if (bullets[i].x + 3 >= enemies[j].x && bullets[i].x <= enemies[j].x + 15 && bullets[i].y >= enemies[j].y && bullets[i].y <= enemies[j].y + 15) {
        bullets[i].isActive = false;
        enemies[j].isActive = false;
        score += SCORE_INCREASEMENT;
        budget += DRONE_VALUE;
        if (score > highestScore) highestScore = score;
        playDestroySound();
      }
    }
  }
}

void gameOverScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 10);
  display.println("Game Over");
  display.setTextSize(1);
  display.print("  Highest score: ");
  display.println(highestScore);
  display.display();
  for (;;)
    ;  // Don't proceed, loop forever
}

void debuging() {
  Serial.print("Rocket at: ");
  Serial.print(rocket.x);
  Serial.print(", ");
  Serial.println(rocket.y);

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].isActive) {
      Serial.print("Enemy at: ");
      Serial.print(enemies[i].x);
      Serial.print(", ");
      Serial.println(enemies[i].y);
    }
  }
}

void inputDebug(unsigned int potValue, bool buttonState) {
  // write data to display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.print("Potentiometer: ");
  display.println(potValue);
  display.print("Button: ");
  if (buttonState == HIGH) {
    display.println("Unpressed");
  } else {
    display.println("Pressed");
  }
  display.display();
}
