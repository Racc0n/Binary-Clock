//2403

#define F_CPU 1000000UL  // 1 MHz
#include <avr/io.h>
#include <util/delay.h>

// Button-Pin-Definitionen (alle an PORTD)
#define BUTTON_BRIGHTNESS PD0  // Brightness-Button an PD0
#define BUTTON_MINUTES    PD1  // Minuten-Button an PD1
#define BUTTON_HOURS      PD2  // Stunden-Button an PD2

// Globale Variablen für Zeit und Helligkeit
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

// Setzt den PWM-Duty-Cycle für beide PWM-Kanäle
void set_pwm_brightness(uint8_t bright) {
	OCR1A = bright;
	OCR1B = bright;
}

// I/O-Initialisierung:
// - PORTC: Stunden-LEDs an PC0-PC4 und Minuten-MSB an PC5
// - PORTD: Minuten-LSB an PD3-PD7 und Button-Pins an PD0-PD2 (mit Pull-Up)
void init_io(void) {
	// PORTC als Ausgang für Stunden und ein Teil der Minuten
	DDRC = 0xFF;
	PORTC = 0x00;
	
	// PORTD: Setze PD3 bis PD7 als Ausgang (Minuten-LEDs) und PD0-PD2 als Eingang (Buttons)
	DDRD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2));  // PD0-PD2 als Eingang
	DDRD |=  (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);  // PD3-PD7 als Ausgang
	// Interne Pull-Ups für die Button-Pins aktivieren
	PORTD |= (1 << PD0) | (1 << PD1) | (1 << PD2);
	
	// Initialer Zustand der Minuten-LEDs an PD3-PD7 auf 0 setzen
	PORTD &= 0x07;  // Nur die unteren 3 Bits (PD0-PD2) bleiben unverändert
}

// Aktualisiert die LED-Anzeige der Zeit:
// - Stunden: Anzeige auf PC0-PC4
// - Minuten: MSB auf PC5 (PORTC) und die unteren 5 Bits auf PD3-PD7
void update_time_display(void) {
	// Stunden: In die unteren 5 Bits von PORTC schreiben (oben bleiben unverändert)
	PORTC = (PORTC & 0xE0) | (hours & 0x1F);
	
	// Minuten: Bit 5 (MSB) auf PC5
	if (minutes & 0x20)
	PORTC |= (1 << PC5);
	else
	PORTC &= ~(1 << PC5);
	
	// Minuten: Bits 0-4 in PD3 bis PD7 schreiben.
	// Erhalte die unteren 3 Bits von PORTD (PD0-PD2, wo die Buttons hängen) und überschreibe nur PD3-PD7.
	PORTD = (PORTD & 0x07) | ((minutes & 0x1F) << 3);
}

int main(void) {
	init_io();
	init_pwm();
	
	set_pwm_brightness(brightness);
	update_time_display();
	
	while (1) {
		// Button für PWM-Helligkeit (PD0)
		if (!(PIND & (1 << BUTTON_BRIGHTNESS))) {  // active low
			brightness += 16;  // Inkrementiere Helligkeit in 16er-Schritten (bei Überlauf beginnt wieder bei 0)
			set_pwm_brightness(brightness);
			_delay_ms(200);    // Einfache Entprellung
		}
		
		// Button für Minuten (PD1)
		if (!(PIND & (1 << BUTTON_MINUTES))) {
			minutes = (minutes + 1) % 60;  // Inkrementiere Minuten, wrap-around bei 60
			update_time_display();
			_delay_ms(200);
		}
		
		// Button für Stunden (PD2)
		if (!(PIND & (1 << BUTTON_HOURS))) {
			hours = (hours + 1) % 24;  // Inkrementiere Stunden, wrap-around bei 24
			update_time_display();
			_delay_ms(200);
		}
		
		_delay_ms(10);  // Kurze Pause, um die CPU zu entlasten
	}
	
	return 0;
}
