#define F_CPU 1000000UL  // 1 MHz (CPU-Takt, falls Fuses nicht anders gesetzt)
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// Button-Pin-Definitionen (an PORTD)
#define BUTTON_BRIGHTNESS PD0  // Button zur Helligkeitsanpassung (active low)
#define BUTTON_MINUTES    PD1  // Button für Minuten (active low)
#define BUTTON_HOURS      PD2  // Button für Stunden (active low)

// Globale Variablen für Uhrzeit und Helligkeit
volatile uint8_t hours      = 12;  // Stunden (0-23)
volatile uint8_t minutes    =  0;  // Minuten (0-59)
volatile uint8_t brightness = 10;  // PWM-Wert (0-255)
volatile uint8_t seconds    =  0;  // Sekunden (wird intern gezählt)

// Variable für den Timeout der Anzeige (in Sekunden).
// Wird bei manueller Eingabe auf 10 gesetzt und in der Timer2-ISR heruntergezählt.
volatile uint8_t display_timeout = 10;

// ----------------- PWM (Timer1) -----------------
// Initialisiert Timer1 im 8-Bit Fast PWM-Modus (nicht-invertierend an OC1A/OC1B)
void init_pwm(void) {
	// Fast PWM 8-Bit: WGM10=1, WGM12=1; nicht-invertierend: COM1A1=1, COM1B1=1
	TCCR1A = (1 << WGM10) | (1 << COM1A1) | (1 << COM1B1);
	TCCR1B = (1 << WGM12) | (1 << CS11);  // Prescaler = 8
	// Setze OC1A (PB1) und OC1B (PB2) als Ausgänge
	DDRB |= (1 << PB1) | (1 << PB2);
}

// Setzt den PWM-Duty-Cycle (Helligkeit)
void set_pwm_brightness(uint8_t bright) {
	OCR1A = bright;
	OCR1B = bright;
}

// ----------------- I/O-Initialisierung -----------------
// - Minuten-LEDs: PORTC, PC0 bis PC5 als Ausgänge
// - Stunden-LEDs: PORTD, PD3 bis PD7 als Ausgänge
// - Buttons: PORTD, PD0 bis PD2 als Eingänge (mit aktiviertem internen Pull-Up)
void init_io(void) {
	// Minuten-LEDs an PORTC: PC0 bis PC5 als Ausgang
	DDRC |= 0x3F;       // Bits 0 bis 5 als Ausgang
	PORTC &= ~0x3F;     // Alle Minuten-LEDs aus
	
	// Stunden-LEDs und Buttons an PORTD:
	// PD0 bis PD2 als Eingang (Buttons) und PD3 bis PD7 als Ausgang (Stunden-LEDs)
	DDRD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2));  // PD0-PD2 als Eingang
	DDRD |=  (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);  // PD3-PD7 als Ausgang
	// Aktiviere interne Pull-Ups für PD0-PD2 (Buttons)
	PORTD |= (1 << PD0) | (1 << PD1) | (1 << PD2);
	// Setze initial die Stunden-LEDs (PD3-PD7) auf 0 (PD0-PD2 bleiben unverändert)
	PORTD &= 0x07;
}

// ----------------- Anzeige der Uhrzeit -----------------
// Aktualisiert die LED-Anzeige:
// - Minuten (6 Bit) werden in PORTC (PC0–PC5) ausgegeben.
// - Stunden (5 Bit) werden in PORTD (PD3–PD7) ausgegeben,
//   wobei die unteren 3 Bits (PD0–PD2, Button-Pins) unverändert bleiben.
void update_time_display(void) {
	// Minuten auf PORTC (PC0–PC5)
	PORTC = (PORTC & 0xC0) | (minutes & 0x3F);
	
	// Stunden auf PORTD (PD3–PD7)
	PORTD = (PORTD & 0x07) | ((hours & 0x1F) << 3);
}

// ----------------- Timer2 (Zeitbasis) -----------------
// Konfiguriert Timer2 asynchron im CTC-Modus mit einem externen 32,768 kHz-Quarz.
// Mit einem Prescaler von 128: 32768/128 = 256, also 256 Ticks pro Sekunde.
// OCR2A wird auf 255 gesetzt, sodass der Compare Match genau 1 Sekunde entspricht.
void init_timer2(void) {
	ASSR |= (1 << AS2); // Asynchroner Betrieb: Timer2 läuft vom externen Quarz
	TCCR2A = (1 << WGM21);  // CTC-Modus: Clear Timer on Compare Match
	TCCR2B = (1 << CS22) | (1 << CS20);  // Prescaler 128 (CS22=1, CS21=0, CS20=1)
	OCR2A = 255;  // 256 Ticks (0..255) entsprechen 1 Sekunde
	TIMSK2 |= (1 << OCIE2A);  // Aktiviert den Compare Match Interrupt für Timer2
	
	// Warte, bis asynchrone Register aktualisiert sind
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
		// Nur wenn die Anzeige aktiv ist, wird sie aktualisiert.
		// (Wenn display_timeout > 0, dann ist die Anzeige an.)
		if(display_timeout > 0) {
			update_time_display();
		}
	}
	// Wenn die Anzeige an ist, wird der Timeout heruntergezählt.
	if(display_timeout > 0) {
		display_timeout--;
	}
}

// ----------------- Sleep-Mode -----------------
// Schaltet die LED-Ausgänge aus und versetzt die CPU in den Power-Save-Modus.
// In diesem Zustand bleiben die Zeitvariablen über Timer2 aktuell,
// aber die Anzeige (LEDs) bleibt aus, bis ein Button betätigt wird.
void go_to_sleep(void) {
	// LEDs ausschalten:
	PORTC &= ~0x3F;  // Minuten-LEDs aus
	PORTD &= 0x07;   // Stunden-LEDs aus (PD0-PD2, die Buttons, bleiben unberührt)
	
	set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	sleep_enable();
	sleep_cpu();  // CPU geht schlafen – sie wird durch einen Interrupt (z.B. Timer2) geweckt
	sleep_disable();
}

// ----------------- Hauptprogramm -----------------
int main(void) {
	init_io();
	init_pwm();
	init_timer2();
	sei();  // Globale Interrupts aktivieren
	
	set_pwm_brightness(brightness);
	update_time_display();
	
	// Anzeige initial eingeschaltet, Timeout auf 10 Sekunden setzen.
	display_timeout = 10;
	
	while (1) {
		// Falls die Anzeige aktiv ist (Timeout > 0), werden manuelle Eingaben ausgewertet.
		if (display_timeout > 0) {
			// Button zur Helligkeitsanpassung (PD0, active low)
			if (!(PIND & (1 << BUTTON_BRIGHTNESS))) {
				brightness += 16;  // Erhöhe PWM-Wert in 16er-Schritten (bei Überlauf startet wieder bei 0)
				set_pwm_brightness(brightness);
				update_time_display();
				display_timeout = 10;  // Timeout zurücksetzen
				_delay_ms(200);  // Entprellung
			}
			
			// Button zur Minutenanpassung (PD1, active low)
			if (!(PIND & (1 << BUTTON_MINUTES))) {
				minutes = (minutes + 1) % 60;
				update_time_display();
				display_timeout = 10;
				_delay_ms(200);
			}
			
			// Button zur Stundenanpassung (PD2, active low)
			if (!(PIND & (1 << BUTTON_HOURS))) {
				hours = (hours + 1) % 24;
				update_time_display();
				display_timeout = 10;
				_delay_ms(200);
			}
		}
		
		// Wenn kein Tastendruck erfolgt und der Timeout abgelaufen ist,
		// soll die Anzeige für immer aus bleiben, bis der Benutzer das nächste Mal eine Taste drückt.
		if (display_timeout == 0) {
			go_to_sleep();
			// Nach dem Aufwachen: Warte in einer Schleife, bis mindestens ein Button betätigt wird.
			while (1) {
				if (!(PIND & (1 << BUTTON_BRIGHTNESS)) ||
				!(PIND & (1 << BUTTON_MINUTES)) ||
				!(PIND & (1 << BUTTON_HOURS))) {
					// Eine Taste wurde gedrückt, also beenden wir diese Schleife.
					break;
				}
				_delay_ms(10);
			}
			// Jetzt schalten wir die Anzeige wieder ein:
			update_time_display();
			display_timeout = 10;
		}
		
		_delay_ms(10);  // Kurze Pause zur Entlastung der CPU
	}
	
	return 0;
}
