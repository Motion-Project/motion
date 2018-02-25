
/*
 *    translate.c
 *
 *    Rudimentary translations for Web User control interface.
 *
 *    This software is distributed under the GNU Public License Version 2
 *    See also the file 'COPYING'.
 *
 */

#include <locale.h>
#include "motion.h"
#include "translate.h"

struct trns_ctx{
  const char *lang;
  const char *en_word;
  const char *fx_word;
};

/** ABSOLUTELY NO QUOTES PERMITED IN THE TRANSLATED VALUES!!!*/

struct trns_ctx trns_phrases[] = {
     {"en","english word","foreign word"}

/* Spanish */
    ,{"es","All Cameras","Todas Cámaras"}
    ,{"es","All","Todas"}
    ,{"es","Action","Acción"}
    ,{"es","Make Movie","Hacer Pelicula"}
    ,{"es","Snapshot","Instantánea"}
    ,{"es","Cameras","Cámaras"}
    ,{"es","Camera","Cámara"}
    ,{"es","Change Configuration","Cambiar Configuración"}
    ,{"es","Write Configuration","Configuración de Escritura"}
    ,{"es","Tracking","Rastreo"}
    ,{"es","Pause","Pausa"}
    ,{"es","Start","Comienzo"}
    ,{"es","Restart","Reiniciar"}
    ,{"es","Quit","Dejar"}
    ,{"es","Help","Ayuda"}
    ,{"es","pan/tilt","Girar/Inclinación"}
    ,{"es","pan","Girar"}
    ,{"es","tilt","Inclinación"}
    ,{"es","Absolute Change","Cambio Absoluto"}
    ,{"es","Center","Centro"}
    ,{"es","save","Salvar"}
    ,{"es","select option","Seleccionar opción"}
    ,{"es","No Configuration Options","Sin opciones de configuración"}
    ,{"es","Limited Configuration Options","Opciones de configuración limitadas"}
    ,{"es","Advanced Configuration Options","Opciones de configuración avanzada"}
    ,{"es","Restricted Configuration Options","Opciones de configuración restringida"}
    ,{"es","setup_mode","modo de configuración"}

/* Lithuanian */
    ,{"lt","All Cameras","Visi Fotoaparatai "}
    ,{"lt","All","Visi"}
    ,{"lt","Action","Veiksmas"}
    ,{"lt","Make Movie","Padaryti Filmą"}
    ,{"lt","Snapshot","Fotografija"}
    ,{"lt","Cameras","Fotoaparatai"}
    ,{"lt","Camera","Fotoaparatas"}
    ,{"lt","Change Configuration","Kkeisti Konfigūraciją"}
    ,{"lt","Write Configuration","Rašyti Konfigūraciją"}
    ,{"lt","Tracking","Stebėjimas"}
    ,{"lt","Pause","Pauzė"}
    ,{"lt","Start","Pradėti"}
    ,{"lt","Restart","Perkrauti"}
    ,{"lt","Quit","Mesti"}
    ,{"lt","Help","Pagalba"}
    ,{"lt","pan/tilt","Pasukamas/Pakreipti"}
    ,{"lt","pan","Pasukamas"}
    ,{"lt","tilt","Pakreipti"}
    ,{"lt","Absolute Change","Absoliutus Pakeitimas"}
    ,{"lt","Center","Centras"}
    ,{"lt","save","Sutaupyti"}
    ,{"lt","select option","Pasirinkite Parinktį"}
    ,{"lt","No Configuration Options","Nėra konfigūravimo parinkčių"}
    ,{"lt","Limited Configuration Options","Ribotos konfigūravimo parinktys"}
    ,{"lt","Advanced Configuration Options","Išplėstinės konfigūracijos galimybės"}
    ,{"lt","Restricted Configuration Options","Apribotos konfigūravimo parinktys"}
    ,{"lt","setup_mode","sąrankos režimas"}

/* Dutch */
    ,{"nl","All Cameras","Alle Cameras"}
    ,{"nl","All","Alle"}
    ,{"nl","Action","Actie"}
    ,{"nl","Make Movie","Film Maken"}
    ,{"nl","Snapshot","Momentopname"}
    ,{"nl","Cameras","Cameras"}
    ,{"nl","Camera","Fotoaparatas"}
    ,{"nl","Change Configuration","Configuratie wijzigen"}
    ,{"nl","Write Configuration","Schrijf Configuratie"}
    ,{"nl","Tracking","Bijhouden"}
    ,{"nl","Pause","Pauze"}
    ,{"nl","Start","Begin"}
    ,{"nl","Restart","Herstarten"}
    ,{"nl","Quit","Ophouden"}
    ,{"nl","Help","Helpen"}
    ,{"nl","pan/tilt","Draaien/Kantelen"}
    ,{"nl","pan","Draaien"}
    ,{"nl","tilt","Kantelen"}
    ,{"nl","Absolute Change","Absolute Verandering"}
    ,{"nl","Center","Middelpunt"}
    ,{"nl","save","Opslaan"}
    ,{"nl","select option","Selecteer optie"}
    ,{"nl","No Configuration Options","Geen configuratie-opties"}
    ,{"nl","Limited Configuration Options","Enigszins Beperkte configuratie-opties"}
    ,{"nl","Advanced Configuration Options","Geavanceerde configuratie-opties"}
    ,{"nl","Restricted Configuration Options","Zeer Beperkte configuratie-opties"}
    ,{"nl","setup_mode","instelmodus"}


/* Next Language */

/* Termination Entry */
    ,{NULL,NULL,NULL}

};


void translate_word(const char *en_word, char *fx_word, int maxsize, char *lang) {


    int indx;

    /* Set default to english word */
    snprintf(fx_word, maxsize, "%s", en_word);

    indx = 0;
    while (trns_phrases[indx].lang != NULL){
        if ((!strcasecmp(lang, trns_phrases[indx].lang)) &&
            (!strcasecmp(en_word, trns_phrases[indx].en_word))){
            snprintf(fx_word, maxsize,"%s",trns_phrases[indx].fx_word);
        }
        indx++;
    }

    return;

}
