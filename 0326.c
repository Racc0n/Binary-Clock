#define F_CPU 1000000UL  // 1 MHz (CPU-Takt, falls Fuses nicht anders gesetzt)
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// Button-Pin-Definitionen (an PORTD)
#define BUTTON_BRIGHTNESS PD0  // PD0: Wird nun nur als Wakeup genutzt,
                               // die Helligkeitsanpassung erfolgt nur bei gleichzeitiger Betätigung von PD0 und PD1.
#define BUTTON_MINUTES    PD1  // PD1: Minute (allein = Minute, zusammen mit PD0 = Helligkeit)
#define BUTTON_HOURS      PD2  // PD2: Stunde

// Globale Variablen für Uhrzeit
volatile uint8_t hours   = 12;  // Stunden (0-23)
volatile uint8_t minutes =  0;  // Minuten (0-59)
volatile uint8_t seconds =  0;  // Sekunden (intern gezählt)

// Timeout für die Anzeige (in Sekunden); wird bei manueller Eingabe auf 10 gesetzt.
volatile uint8_t display_timeout = 10;

// ----------------- Helligkeitssteuerung -----------------
// 5 Helligkeitsstufen für Minuten- und Stunden-LEDs, getrennt konfiguriert
uint8_t brightness_levels_minutes[5] = {0, 50, 100, 150, 255};  // Minuten-LEDs sollen heller sein
uint8_t brightness_levels_hours[5]   = {240, 243, 245, 250, 255}; // Stunden-LEDs sollen dunkler sein

volatile uint8_t brightness_index = 2; // Start mit mittlerer Stufe

// ----------------- PWM (Timer1) -----------------
// Timer1 im 8-Bit Fast PWM-Modus
// Wir nutzen OC1A (z. B. PB1) für die Minuten-LEDs und
// OC1B (z. B. PB2) für die Stunden-LEDs.
void init_pwm(void) {
    TCCR1A = (1 << WGM10) | (1 << COM1A1) | (1 << COM1B1);
    TCCR1B = (1 << WGM12) | (1 << CS11);  // Prescaler = 8
    DDRB |= (1 << PB1) | (1 << PB2);       // Setze OC1A (PB1) und OC1B (PB2) als Ausgänge
}

void set_pwm_minutes(uint8_t bright) {
    OCR1A = bright;
}

void set_pwm_hours(uint8_t bright) {
    OCR1B = bright;
}

// ----------------- I/O-Initialisierung -----------------
// - Minuten-LEDs: PORTC (PC0 bis PC5) als Ausgänge
// - Stunden-LEDs: PORTD (PD3 bis PD7) als Ausgänge
// - Buttons: PORTD (PD0 bis PD2) als Eingänge (mit aktiviertem internen Pull-Up)
void init_io(void) {
    DDRC |= 0x3F;       // PC0-PC5 als Ausgang (Minuten-LEDs)
    PORTC &= ~0x3F;     // Alle Minuten-LEDs aus

    DDRD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2));  // PD0-PD2 als Eingang (Buttons)
    DDRD |= (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7); // PD3-PD7 als Ausgang (Stunden-LEDs)
    PORTD |= (1 << PD0) | (1 << PD1) | (1 << PD2);       // Aktiviere interne Pull-Ups an PD0-PD2
    PORTD &= 0x07;  // Setze initial PD3-PD7 (Stunden-LEDs) auf 0, PD0-PD2 bleiben unverändert
}

// ----------------- Anzeige der Uhrzeit -----------------
// Zeigt Minuten (6 Bit) auf PORTC (PC0–PC5) und Stunden (5 Bit) auf PORTD (PD3–PD7) an.
void update_time_display(void) {
    PORTC = (PORTC & 0xC0) | (minutes & 0x3F);
    PORTD = (PORTD & 0x07) | ((hours & 0x1F) << 3);
}

// ----------------- Timer2 (Zeitbasis) -----------------
// Timer2 im asynchronen CTC-Modus mit externem 32,768 kHz-Quarz.
// Prescaler 128: 32768/128 = 256 Ticks pro Sekunde, OCR2A = 255 ? 1 Sekunde.
void init_timer2(void) {
    ASSR |= (1 << AS2);
    TCCR2A = (1 << WGM21);
    TCCR2B = (1 << CS22) | (1 << CS20);
    OCR2A = 255;
    TIMSK2 |= (1 << OCIE2A);
    while (ASSR & ((1 << TCR2BUB) | (1 << TCR2AUB) | (1 << OCR2AUB) | (1 << TCN2UB)));
}

// Timer2 Compare Match ISR (wird einmal pro Sekunde aufgerufen)
ISR(TIMER2_COMPA_vect) {
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes++;
        if (minutes >= 60) {
            minutes = 0;
            hours = (hours + 1) % 24;
        }
        if(display_timeout > 0) {
            update_time_display();
        }
    }
    if(display_timeout > 0) {
        display_timeout--;
    }
}

// ----------------- Sleep-Mode -----------------
// Diese Funktion schaltet die LED-Ausgänge aus,
// deaktiviert ungenutzte Peripherie und versetzt den MCU
// in den Power-Save-Modus. Nach dem Aufwachen wird eine Verzögerung
// eingelegt, um den externen Quarz stabilisieren zu lassen, und die
// Peripherie wieder reaktiviert.
void go_to_sleep(void) {
    // LEDs ausschalten:
    PORTC &= ~0x3F;  // Minuten-LEDs aus
    PORTD &= 0x07;   // Stunden-LEDs aus (PD0-PD2 bleiben als Eingänge für die Buttons unverändert)
    
    // Deaktiviere ungenutzte Module (z. B. ADC und Timer0)
    PRR |= (1 << PRADC) | (1 << PRTIM0);
    
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sleep_enable();
    sleep_cpu();  // MCU geht schlafen; Timer2 läuft weiterhin
    sleep_disable();
    
    // Nach dem Aufwachen: Warte ca. 15ms für die Einschwingzeit des externen Quarzoszillators
    _delay_ms(15);
    
    // Reaktivieren der zuvor deaktivierten Module
    PRR &= ~((1 << PRADC) | (1 << PRTIM0));
}

// ----------------- Hauptprogramm -----------------
int main(void) {
    init_io();
    init_pwm();
    init_timer2();
    sei();  // Globale Interrupts aktivieren

    // Setze initial die PWM-Werte gemäß brightness_index
    set_pwm_minutes(brightness_levels_minutes[brightness_index]);
    set_pwm_hours(brightness_levels_hours[brightness_index]);
    update_time_display();
    display_timeout = 10;

    while (1) {
        // Manuelle Eingaben werden nur ausgewertet, wenn die Anzeige aktiv ist.
        if (display_timeout > 0) {
            // Wenn PD0 UND PD1 gleichzeitig gedrückt werden, erfolgt die Helligkeitsanpassung.
            if ( (!(PIND & (1 << BUTTON_BRIGHTNESS))) && (!(PIND & (1 << BUTTON_MINUTES))) ) {
                brightness_index = (brightness_index + 1) % 5;
                set_pwm_minutes(brightness_levels_minutes[brightness_index]);
                set_pwm_hours(brightness_levels_hours[brightness_index]);
                update_time_display();
                display_timeout = 10;  // Timeout zurücksetzen
                _delay_ms(200);  // Entprellung
            }
            // Falls nur PD1 gedrückt wird (ohne PD0), erfolgt die Minutenanpassung.
            else if ( (!(PIND & (1 << BUTTON_MINUTES))) && (PIND & (1 << BUTTON_BRIGHTNESS)) ) {
                minutes = (minutes + 1) % 60;
                update_time_display();
                display_timeout = 10;
                _delay_ms(200);
            }
            // PD2 (Stundenanpassung) bleibt wie gehabt.
            if (!(PIND & (1 << BUTTON_HOURS))) {
                hours = (hours + 1) % 24;
                update_time_display();
                display_timeout = 10;
                _delay_ms(200);
            }
        }
        
        // Falls kein Tastendruck erfolgt und der Timeout abgelaufen ist,
        // wird in den Sleep-Mode gewechselt.
        if (display_timeout == 0) {
            go_to_sleep();
            // Nach dem Aufwachen: Warte, bis mindestens ein Button gedrückt wird.
            while (1) {
                if (!(PIND & (1 << BUTTON_BRIGHTNESS)) ||
                    !(PIND & (1 << BUTTON_MINUTES)) ||
                    !(PIND & (1 << BUTTON_HOURS))) {
                    break;  // Eine Taste wurde gedrückt.
                }
                _delay_ms(10);
            }
            update_time_display();
            display_timeout = 10;
        }
        
        _delay_ms(10);  // Kurze Pause zur Entlastung der CPU
    }
    
    return 0;
}
