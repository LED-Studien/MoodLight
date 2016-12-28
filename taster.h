#ifndef _TASTER_H_
#define _TASTER_H_

#include <inttypes.h>

// `taster' bekannt machen
// Wurde keine Taste gedr�ckt, dann enth�lt diese Variable den Wert 0.
// Wirde eine Taste gedr�ckt, enth�lt diese Variable die Komponente
// .key des gedr�ckten Tasters (bzw. den um KEY_LONG vergr��erten
// von .key, wenn es sich um einen langen Tastendruck handelt).
// Es werden nur dann neue Tastendrucke angenommen, wenn taster=0 ist.
// Hat man in der Anwandung alse ein Taster-Ereignis verarbeitet,
// muss man taster=0 setzen. `taster' berh�lt sich also wie ein
// Tastaturpuffer der L�nge 1.
extern volatile uint8_t taster;

// Die drei m�glichen Betriebsmodi eines Tasters
enum
{
    KM_SHORT,
    KM_LONG,
    KM_REPEAT,
    KM_SKIP
};

// Ein Taster
typedef struct
{
    // Betriebsmodus
    // KM_SHORT   einfach nur Tastendruck melden
    // KM_LONG    Tastendruck mit Unterscheidung lang/kurz
    // KM_REPEAT  bleibt die Taste gedr�ckt, so wird immer wieder der gleiche
    //            Taster-Wert geliefert, �hnlich wie bei einer PC-Tastatur
    // KM_SKIP    Verwendet in get_taster() den vormaligen Wert von 'tast' f�r
    //            den Fall, da� momentan kein Wert verf�gbar ist
    uint8_t mode;

    // Wird der Taster gedr�ckt, wird dieser Wert in `taster' geschrieben.
    // bzw. bei einem langen Tastendruck .key + KEY_LONG
    // Dieser Wert muss ungleich Null sein.
    uint8_t key;

    // Wird intern gebraucht
    uint8_t delay;
    uint8_t old;
} taste_t;

// Alle 10ms (oder so) aufrufen
extern void get_taster (taste_t * const, uint8_t);

// Zeiten (in Einheiten von 10 Millisekunden) f�r:
// Anfangsverz�gerung bis Auto-Repeate loslegt (.mode = KM_REPEAT)
#define KEYREPEAT_DELAY 50
// Zeitverz�gerung zwischen zwei Auto-Repeates (.mode = KM_REPEAT)
#define KEYREPEAT       30
// Ab dieser Dauer ist ein Tastendruck nicht mehr "kurz",
// sondern "lang" (.mode = KM_LONG)
#define KEYDELAY_LONG   50

// Wird auf .key draufaddiert, wenn es sich bei (.mode = KM_LONG)
// um einen langen Tastendruck handelt.
// Bei mehr als 4 Tastern muss dieser Wert vergr��ert werden!
#define KEY_LONG 4

#endif /* _TASTER_H_ */
