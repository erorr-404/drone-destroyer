# Drone Destroyer

Arduino Nano game. You take role of **F-16 ukrainian pilot**. You need to destroy drones. It is bad to lose drone.

## Rules
Game has 2 counters: **score** and **money**. At start you get _10$_ and your score is _1_. 

When you shoot with bullet, you pay _1$_. When you launch rocket, you pay _4$_.

Every destroyed drone with bullet gives you _2$_. If you destroy drones with rocket, you get `DRONE_VALUE * destroyedEnemies * DRONE_VALUE_MULT`, where `DRONE_VALUE` is _2$_ and `DRONE_VALUE_MULT` is _2_.

Your target is to get the highest score. You lose when score is _0_ or no money left.

## Needed components

- Arduino Nano
- Passive buzzer
- 0.96 OLED Arduino display module 128х64 with I2C pins
- 2 push buttons
- Potentiometer
- Led
- 2 resistors
- 2 small beradboards or 1 big

## Connection diagram

Potentiometer -> A3
Bullet Button -> 6
Buzzer -> 10
Rocket Button -> 4
Rocket Ready LED -> 5

![image](https://github.com/user-attachments/assets/4ff29afa-6b94-4530-9b9d-c5c3816329da)


