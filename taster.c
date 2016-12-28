/**************************************************************
*Tools für Taster-Abfrage
*
* 2. Teil Routinen von G.J. Lay
* http://www.gjlay.de/pub/c-code/taster.html
**************************************************************/


#include <avr/io.h>
#include <inttypes.h>
#include <util/delay.h> 


/* Einfache Funktion zum Entprellen eines Tasters */
inline uint8_t debounce(volatile uint8_t *port, uint8_t pin)
{
    if ( ! (*port & (1 << pin)) )
    {
        /* Pin wurde auf Masse gezogen, 100ms warten   */
        _delay_ms(50);  // max. 262.1 ms / F_CPU in MHz
        _delay_ms(50); 
        if (!( *port & (1 << pin) ))
        {
            /* Anwender Zeit zum Loslassen des Tasters geben */
            _delay_ms(50);
            _delay_ms(50); 
            return 1;
        }
    }
    return 0;
}

/*
Taster-Abfrage und was dazu gehört

Dieses Modul wird verwendet bei der immer wiederkehrenden Aufgabe der Taster-Abfrage. Der C-Code ist projekt- und hardwareunabhängig – zumindest was 
die Tasterauswertung selbst angeht. Die Erzeugung einer Zeitbasis ist natürlich systemabhängig. Ebenso wie beim Countdown-Zähler und der DCF77-Software 
wird die Arbeitsroutine für die zu erledigende Aufgabe im 10 Millisekunden-Rhythmus ausgeführt. Diese langsame Abtastrate liefert die Entprellung der 
Taster frei Haus. In taster.h wird die Struktur taste_t definiert. Im Hauptprogramm legt man von diesem Typ so viele Variablen an, wie man Taster 
abzufragen hat. Ein Beispiel dazu findet sich ganz unten. Die Struktur taste_t hat folgende für den Anwender interessante Komponenten:

.mode    Dieser Eintrag legt das Verhalten des jeweiligen Tasters fest. Soll jedes Mal gemeldet werden, wenn der Taster gedrückt wird, schreibt man KM_SHORT in dieses Feld. Soll unterschieden werden zwischen langen und kurzen Tastendrucken, schreibt man KM_LONG hinein. Soll der Taster eine Auto-Repeate-Funktion haben, ist der Wert KM_REPEAT. 
.key     Der Wert dieser Komponente wird nach taster kopiert, wenn diese Taste gedrückt wurde. Bei einem langen Tastendruck und falls .mode=KM_LONG ist, wird der Wert .key+KEY_LONG nach taster geschrieben. 

Die Routine, die sich um die Tasterabfrage kümmert, heisst get_taster. Sie hat zwei Parameter. Der erste Parameter ist die Adresse des Tasters, der 
abgefragt werden soll. Der zweite Parameter informiert die Funktion über den Zustand des Ports, an den der Taster angeschlossen ist. Ist der Taster 
gedrückt, wird als zweiter Parameter 0 übergeben. Ist er nicht gedrückt, wird ein Wert ungleich 0 übergeben. get_taster hat keinen Rückgabewert, 
stattdessen wird gegebenenfalls die globale Variable taster auf einen neuen Wert gesetzt. Verwendet man zwei Taster, dann könnte eine Deklaration so aussehen:

taste_t tasten[] =
{
     { .key = 1, .mode = KM_SHORT  }, // erster Taster
     { .key = 2, .mode = KM_REPEAT }  // zweiter Taster (auto-widerholen)
};

Es wird alse ein Arrey mit zwei Taster-Strukturen angelegt und initialisiert. Der Aufruf von get_taster sieht damit folgendermassen aus (falls der 
erste Taster an Port B1 und der zweite an Port D3 angeschlossen ist):

    get_taster (& tasten[0], PINB & (1 << PB1));
    get_taster (& tasten[1], PIND & (1 << PD3));

Der Codeverbrauch des Moduls liegt bei ca. 130 Bytes. Werden einfach nur Tastendrucke ohne Sonderfunktion empfangen, also keine langen Drucke oder 
Auto-Repeate (TASTER_SAME_MODE=KM_SHORT), dann sinkt der Codeverbrauch auf ca. 40 Bytes.

******  main.c  *****

Ein Beispiel wie es aussehen könnte auf einem ATtiny2313. Es wird Timer1 so grogrammiert, daß er 5000 IRQs pro Sekunde auslöst. In der Interrupt-Routine 
wird die Anzahl der IRQs mitgezählt und wenn 10 Millisekunden voll sind, wird get_taster() aufgerufen. Da zwei Taster angeschlossen sind, wird die 
Funktion auch zweimal aufgerufen – einmal für jeden Taster. Dabei werden die Aufrufe nicht in ein und derselben IRQ erledigt, sondern auf zwei 
aufeinanderfolgende IRQs verteilt.
Die Hauptschleife deutet an, wie man die Tasterabfrage macht und die Tastenwerte aus taster abhohlt. Im Beispiel geschieht die Verarztung der 
Taster-Routine(n) aus der Timer-ISR heraus. Ebenso ist vorstellbar, den Aufruf in die Hauptschleife auszulagern und die ISR so zu entlasten. In diesem 
Falle würde man in der ISR nur einen Merker setzen (oder hochzählen), daß 10 Millisekunden voll sind, und dies in der Hauptschleife abhandeln.
Dies ist ein Ausschnitt aus dem Quellcode zu meiner Eieruhr. 

#include <avr/io.h>
#include <avr/interrupt.h>

// Für ältere avr-gcc Versionen
#ifndef SIGNAL
#include <avr/signal.h>
#endif // SIGNAL

#include "taster.h"

#define IRQS_PER_SECOND     5000
#define IRQS_PER_10MS       (IRQS_PER_SECOND / 100)

static void timer1_init();

// Ein paar Werte festlegen um "magische Zahlen" aus dem Code rauszuhalten.
enum
{
    NO_KEY = 0,

    KEY_EINER   = 1,
    KEY_ZEHNER  = 2,

    KEY_EINER_L  = KEY_EINER  + KEY_LONG,
    KEY_ZEHNER_L = KEY_ZEHNER + KEY_LONG
};

// Zwei Tasten, .mode ist KM_LONG
taste_t tasten[] =
{
     [0] = { .key = KEY_EINER,  .mode = KM_LONG }
   , [1] = { .key = KEY_ZEHNER, .mode = KM_LONG }
};

SIGNAL (SIG_OUTPUT_COMPARE1A)
{
    static uint8_t interrupt_num_10ms;

    uint8_t irq_num = interrupt_num_10ms;

    // interrupt_num_10ms erhöhen und mit Maximalwert vergleichen
    if (++irq_num == IRQS_PER_10MS)
    {
        // 10 Millisekunden sind vorbei
        // interrupt_num_10ms zurücksetzen
        irq_num = 0;
    }

    // Alle 10 ms die Taster abfragen (ginge auch zusammen, mach ich aber net ;-)
    if (irq_num == 0)       get_taster (& tasten[0], IS_SET (PORT_EINER));
    if (irq_num == 1)       get_taster (& tasten[1], IS_SET (PORT_ZEHNER));

    interrupt_num_10ms = irq_num;
}

void timer1_init()
{
    // Timer1 ist Zähler: Clear Timer on Compare Match (CTC, Mode #4)
    // Timer1 läuft @ F_CPU: prescale = 1
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS10);

    // PoutputCompareA für gewünschte Timer1 IRQ-Rate
    OCR1A = (unsigned short) ((unsigned long) F_CPU / IRQS_PER_SECOND-1);

    // OutputCompare-Interrupt A für Timer1
    TIMSK = (1 << OCIE1A);
}

int main()
{
    // Taster sind Input mit PullUp (low active)
    SET (PORT_EINER);
    SET (PORT_ZEHNER);

    // Timer1 macht IRQs
    timer1_init();

    // IRQs zulassen
    sei();

    // Hauptschleife
    while (1)
    {
        uint8_t tast = taster;

        if (tast)
            taster = NO_KEY;
        if (tast == KEY_EINER)
        {
            // Taste "Einer" (kurz)
        }
        if (tast == KEY_EINER_L)
        {
            // Taste "Einer" (lang)
        }
        if (tast == KEY_ZEHNER)
        {
            // Taste "Zehner" (kurz)
        }
        if (tast == KEY_ZEHNER_L)
        {
            // Taste "Zehner" (lang)
        }
        ...
    } // main Loop
}





#include "taster.h"

volatile uint8_t taster;

void get_taster (taste_t * const ptast, uint8_t tast)
{
    // Nicht tun, falls die Anwendung den Wert aus `taster' noch nicht
    // abgehohlt hatden
    if (taster)
        return;

    // ist Taster gedrückt? (low aktiv)
    tast = ! tast;
    uint8_t taster_old = ptast->old;

    // Was wurde gedrückt/losgelassen...?
    uint8_t pressed =  taster_old &  tast;
    uint8_t press   = ~taster_old &  tast;
    uint8_t release =  taster_old & ~tast;

    ptast->old = tast;

    tast = 0;

    uint8_t mode  = ptast->mode;
    uint8_t delay = ptast->delay;
    uint8_t key   = ptast->key;

    // Falls der mode für alle Taster *immer* gleich ist, kann man
    // das Define TASTER_SAME_MODE auf den Modus setzen, um durch
    // dieses Wissen weitere Optimierungen zu ermöglichen.
    // Das Define kann man zB beim Aufruf von gcc machen:
    // > gcc ... -DTASTER_SAME_MODE=KM_LONG
#   ifdef TASTER_SAME_MODE
    // Let GCC throw away superflous code if there is some extra
    // knowledge. Even though there is more C code in that case
    // the code size will reduce. Confused?
    mode = TASTER_SAME_MODE;
#   endif // TASTER_SAME_MODE

    // Taste wurde eben gedrückt
    if (press)
    {
        if (mode != KM_LONG)
            tast = key;

        delay = 0;
    }
    // Taste bleibt gedrückt
    else if (pressed)
    {
        if (delay < 0xfe)
            delay++;
    }
    // Taste wurde losgelassen
    else if (release)
    {
        if (mode == KM_LONG && delay != 0xff)
            tast = key;
    }

    // Langer Tastendruck?
    if (mode == KM_LONG)
    {
        if (delay == KEYDELAY_LONG)
        {
            tast = KEY_LONG + key;
            delay = 0xff;
        }
        // Delay zurückschreiben
        ptast->delay = delay;
    }
    // Auto-Repeate
    else if (mode == KM_REPEAT)
    {
        if (delay == KEYREPEAT_DELAY)
        {
            tast = key;
            delay = KEYREPEAT_DELAY - KEYREPEAT;
        }
        // Delay zurückschreiben
        ptast->delay = delay;
    }

    // Tastencode speichern
    taster = tast;
}
*/
