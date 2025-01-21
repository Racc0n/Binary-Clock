#define F_CPU 1000000UL  // Quarz-Takt (1 MHz, falls keine Fuses angepasst wurden)
#include <avr/io.h>
#include <util/delay.h>

// Definition der Pins
#define LED_PORT1 PORTC  // LEDs 1-8 auf PORTC
#define LED_PORT2 PORTD  // LEDs 9-11 auf PORTD (einschließlich PD3 und PD4)
#define BUTTON_PIN PINB  // Taster an PORTB (SW1-SW3)

// Taster-Definitionen
#define BUTTON1 PB0  // Taster 1 (SW1)
#define BUTTON2 PB3  // Taster 2 (SW2)
#define BUTTON3 PB4  // Taster 3 (SW3)

void init_pwm() {
    // Timer1 konfigurieren: Fast PWM, 8-Bit, nicht-invertiert auf OC1A und OC1B
    TCCR1A = (1 << WGM10) | (1 << COM1A1) | (1 << COM1B1);  // Fast PWM, OC1A und OC1B nicht-invertierend
    TCCR1B = (1 << WGM12) | (1 << CS11);  // Prescaler = 8, Fast PWM
    DDRB |= (1 << PB1) | (1 << PB2);  // PB1 (OC1A) und PB2 (OC1B) als Ausgang
}

void init_io() {
    // LEDs konfigurieren
    DDRC = 0xFF;   // Alle Pins von PORTC als Ausgang (für LEDs 1-8)
    DDRD = 0xFF;   // PD0-PD4 als Ausgang (für LEDs 9-11 und zusätzliche LEDs an D3 und D4)
    PORTC = 0x00;  // LEDs 1-8 ausschalten
    PORTD = 0x00;  // LEDs 9-11 (einschließlich D3 und D4) ausschalten
}

void set_pwm_brightness(uint8_t brightness) {
    OCR1A = brightness;  // Setzt den Duty Cycle für OC1A
    OCR1B = brightness;  // Setzt den Duty Cycle für OC1B
}
 
void turn_on_led(uint8_t led) {
    if (led < 8) {
        LED_PORT1 = (1 << led);  // LEDs 1-8 an PORTC
        LED_PORT2 = 0x00;       // LEDs 9-11 und D3/D4 ausschalten
    } else if (led == 11) {
        LED_PORT1 = 0x00;       // LEDs 1-8 ausschalten
        LED_PORT2 = (1 << 3);   // LED an PD3
    } else if (led == 12) {
        LED_PORT1 = 0x00;       // LEDs 1-8 ausschalten
        LED_PORT2 = (1 << 4);   // LED an PD4
    } else if (led == 13) {
        LED_PORT1 = 0x00;       // LEDs 1-8 ausschalten
        LED_PORT2 = (1 << 5);   // LED an PD5
    } else if (led == 14) {
        LED_PORT1 = 0x00;       // LEDs 1-8 ausschalten
        LED_PORT2 = (1 << 6);   // LED an PD6
    } else if (led == 15) {
        LED_PORT1 = 0x00;       // LEDs 1-8 ausschalten
        LED_PORT2 = (1 << 7);   // LED an PD7
    }
}

int main(void) {
    init_io();
    init_pwm();

    uint8_t current_led = 0;  // Start mit LED 1
    uint8_t direction = 1;   // Richtung (1 = vorwärts, 0 = rückwärts)
    uint8_t brightness = 10;  // Anfangshelligkeit (50%)

    while (1) {
        // LEDs nacheinander ein- und ausschalten
        turn_on_led(current_led);

        // Setze die Helligkeit der LEDs
        set_pwm_brightness(brightness);

        _delay_ms(200);  // Verzögerung von 200 ms

        // Nächste LED bestimmen
        if (direction) {
            current_led++;  // Vorwärts
            if (current_led >= 16) {  // Es gibt jetzt 13 LEDs
                current_led = 0;  // Zurück zur ersten LED
            }
        }
    }

    return 0;
}