//0325
#define F_CPU 1000000UL  // 1 MHz
#include <avr/io.h>
#include <util/delay.h>

// Button-Pin-Definitionen (an PORTD)
#define BUTTON_BRIGHTNESS PD0  // Button zur Helligkeitsanpassung (active low)
#define BUTTON_MINUTES    PD1  // Button für Minuten (active low)
#define BUTTON_HOURS      PD2  // Button für Stunden (active low)

// Globale Variablen für Uhrzeit und Helligkeit
volatile uint8_t hours      = 12;  // Startwert für Stunden (0-23)
volatile uint8_t minutes    =  0;  // Startwert für Minuten (0-59)
volatile uint8_t brightness = 10;  // PWM-Wert (0-255)

// PWM-Initialisierung: Timer1 als 8-Bit Fast PWM, nicht-invertierend (OC1A/OC1B)
void init_pwm(void) {
    // Fast PWM 8-Bit: WGM10=1, WGM12=1; nicht-invertierend: COM1A1=1, COM1B1=1
    TCCR1A = (1 << WGM10) | (1 << COM1A1) | (1 << COM1B1);
    TCCR1B = (1 << WGM12) | (1 << CS11);  // Prescaler = 8
    // Setze OC1A (PB1) und OC1B (PB2) als Ausgang
    DDRB |= (1 << PB1) | (1 << PB2);
}

// Setzt den PWM-Duty-Cycle (Helligkeit)
void set_pwm_brightness(uint8_t bright) {
    OCR1A = bright;
    OCR1B = bright;
}

// I/O-Initialisierung:
// - Minuten-LEDs: PORTC, PC0 bis PC5 als Ausgänge
// - Stunden-LEDs: PORTD, PD3 bis PD7 als Ausgänge
// - Buttons: PORTD, PD0 bis PD2 als Eingänge (mit aktiviertem internen Pull-Up)
void init_io(void) {
    // Minuten-LEDs an PORTC: PC0-PC5 als Ausgang
    DDRC |= 0x3F;       // Bits 0 bis 5 als Ausgang
    PORTC &= ~0x3F;     // Alle Minuten-LEDs aus
    
    // Stunden-LEDs und Buttons an PORTD:
    // Setze PD0-PD2 als Eingang (Buttons) und PD3-PD7 als Ausgang (Stunden-LEDs)
    DDRD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2));  // PD0-PD2 als Eingang
    DDRD |=  (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);  // PD3-PD7 als Ausgang
    // Aktiviere die internen Pull-Ups für PD0-PD2 (Buttons)
    PORTD |= (1 << PD0) | (1 << PD1) | (1 << PD2);
    
    // Setze initial die Stunden-LEDs (PD3-PD7) auf 0, ohne die Button-Pins zu verändern.
    PORTD &= 0x07;  // Behalte PD0-PD2 unverändert, lösche PD3-PD7.
}

// Aktualisiert die LED-Anzeige der Zeit:
// - Minuten: werden als 6-Bit-Wert in PORTC (PC0–PC5) ausgegeben.
// - Stunden: werden als 5-Bit-Wert in PORTD (PD3–PD7) ausgegeben,
//   wobei die unteren 3 Bits (PD0–PD2, Button-Pins) unverändert bleiben.
void update_time_display(void) {
    // Minuten auf PORTC (PC0–PC5)
    PORTC = (PORTC & 0xC0) | (minutes & 0x3F);
    
    // Stunden auf PORTD (PD3–PD7):
    // Behalte die unteren 3 Bits (PD0-PD2) und setze PD3-PD7 neu
    PORTD = (PORTD & 0x07) | ((hours & 0x1F) << 3);
}

int main(void) {
    init_io();
    init_pwm();
    
    set_pwm_brightness(brightness);
    update_time_display();
    
    while (1) {
        // Button zur Helligkeitsanpassung (PD0, active low)
        if (!(PIND & (1 << BUTTON_BRIGHTNESS))) {
            brightness += 16;  // Inkrementiere PWM-Wert in 16er-Schritten (bei Überlauf startet wieder bei 0)
            set_pwm_brightness(brightness);
            _delay_ms(200);    // Entprellung
        }
        
        // Button zur Minutenanpassung (PD1, active low)
        if (!(PIND & (1 << BUTTON_MINUTES))) {
            minutes = (minutes + 1) % 60;  // Erhöhe Minuten, wrap-around bei 60
            update_time_display();
            _delay_ms(200);
        }
        
        // Button zur Stundenanpassung (PD2, active low)
        if (!(PIND & (1 << BUTTON_HOURS))) {
            hours = (hours + 1) % 24;  // Erhöhe Stunden, wrap-around bei 24
            update_time_display();
            _delay_ms(200);
        }
        
        _delay_ms(10);  // Kurze Pause, um die CPU zu entlasten
    }
    
    return 0;
}
