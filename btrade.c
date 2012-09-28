/**
 * ***********************************************************************************
 * ***********************************************************************************
 * *** btrade.c - (C) 2012 by Romano Kleinwächter <romano.kleinwaechter@gmail.com> ***
 * ***********************************************************************************
 * ***********************************************************************************
 *
 * Dieses Tool basiert auf den Daten von http://www.bitcoinmonitor.com welcher von
 * Jan Vornberger entwickelt wurde. Dies ist eine erste frühe Version die sicher
 * noch mit sehr sehr vielen Features für die Datenauswertung gefüllt werden kann.
 *
 * Lizensiert unter der GPLv2 (http://www.gnu.de/documents/gpl-2.0.de.html)
 *
 *
 * /------------------------------------------------------------------------------]
 * | Um meine Arbeit für das Bitcoinnetzwerk zu unterstützen bitte ich um Spenden
 * | zu der unten aufgeführten Bitcoinadresse.
 * |
 * | To support my work for the bitcoinnetwork I would like to receive donations
 * | to the bitcoinaddress below.
 * |
 * | DONATION ADDRESS: 1M2kiTW1Bc2NDrpkRk7TZqU5C8cLx2wjvR
 * \------------------------------------------------------------------------------]
 */

/** ********** INCLUDES ********** */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "btrade.h"
#include "mtgox.h"
#include "bitcoinmonitor.h"
/** ********** /INCLUDES ********* */

/**
 * main, der Anfang vom Ende
 *
 * @param[in] argc Anzahl der Argumente
 * @param[in] argv Argumentliste
 * @return Exitcode
 */
int main(int argc, char **argv)
{
	// Flags prüfen
	if(argc>=2)
	{
		// Versionsausgabe
		if(!strncmp(argv[1],"--version",9))
		{
			printf("Version: %s\n", VERSION);
			return RET_OK;
		}
		// Bitcoinmonitor
		else if(!strncmp(argv[1],"--btm",5))
		{
			// --btm benötig Währung
			if(argc==3)
			{
				// Daten von bitcoinmonitor.com holen und ausgeben
				size_t max_row = 0;
				struct trade **tmatrix = parse_data(&max_row);
				print_data(tmatrix, max_row, argv[2]);
				// Heap aufräumen & normales Ende mitteilen
				free_matrix_data(tmatrix, max_row);
				return RET_OK;
			}
		}
	}	

	// Ungültige Option
	usage();
	return RET_USAGE;
}

/** ********** FUNKTIONEN ********** */
/**
 * Hilfeausgabe
 *
 * @return void
 */
void usage()
{
	printf
	(
		"\n"
		"*******************************************************************************\n"
		"** btrade - (C) 2012 by Romano Kleinwächter <romano.kleinwaechter@gmail.com> **\n"
		"**                                                                           **\n"
		"** Um meine Arbeit für das Bitcoinnetzwerk zu unterstützen bitte ich um      **\n"
		"** Spenden an diese Adresse: 1M2kiTW1Bc2NDrpkRk7TZqU5C8cLx2wjvR              **\n"
		"*******************************************************************************\n"
		"\n"
		"\n"
		"Benutzung:\n"
		"\n"
		"\t%-25s %s\n" // Flags 1
		"\t%-25s %s\n" // Flag 2
		"\n",

		// Beschreibungen
		"--version", "Ausgabe der Programmversion", // Version
		"--btm <EUR, USD, etc>", "Ausgewertete Daten von http://www.bitcoinmonitor.com anzeigen" // bitcoinmonitor.com
	);

	// Rücksprung
	return; // Rückkehr
}

/**
 * Gibt Daten als Base64 kodierten String zurück
 *
 * @param[in] data Zeiger auf Daten
 * @param[in] len Länge der Daten
 * @return Zeiger auf Base64 String
 */
char* base64_encode(void *data, size_t len)
{
	// Parameter testen
	if(data==NULL || len==0) PARAM_FATAL(__FILE__, "base64_encode()");

	// Vorbereitungen
	BIO *raw = NULL; // Zeiger auf binäre Daten
	BIO *b64 = NULL; // Zeiger auf Base64 Daten
	BUF_MEM *praw = NULL; // Zeiger für Rohdaten im Puffer
	char *b64_str = NULL; // Zeiger für Base64 String

	// Speicher allokieren
	b64 = BIO_new(BIO_f_base64()); // Base64 Daten
	raw = BIO_new(BIO_s_mem()); // Rohe binäre Daten im Speicher
	
	// Auf Fehler prüfen
	if(b64==NULL || raw==NULL) Fatal
	(
		"base64_encode(): Es konnte kein Speicher allokiert werden.",
		RET_ALOC_ERROR
	);

	b64 = BIO_push(b64, raw); // Beide Speicherbereiche hintereinander legen und Ende zurückgeben
	if(BIO_write(b64,data,len)<=0) Fatal // Originale Daten nach b64 schreiben
	(
		"base64_encode(): Eingabedaten konnten nicht kopiert werden.",
		RET_OPENSSL_ERROR
	);
	if(BIO_flush(b64)<=0) Fatal // Sicherstellen das alle Daten geschrieben werden
	(
		"base64_encode(): Es konnten nicht alle Eingabedaten geschrieben werden.",
		RET_OPENSSL_ERROR
	);
	BIO_get_mem_ptr(b64, &praw); // BUF_MEM Struktur von b64 in praw abbilden
	if(praw==NULL) Fatal // Fehler
	(
		"base64_encode(): Speicherstruktur konnte nicht abgebildet werden.",
		RET_OPENSSL_ERROR
	);

	// Speicher für Base64 String allokieren, resetten und befüllen
	b64_str = (char*)malloc(praw->length);
	memset(b64_str, 0x00, praw->length);
	memcpy(b64_str, praw->data, praw->length-1);
	b64_str[praw->length-1] = 0x00; // Abschließendes Nullbyte gewährleisten

	// Verwendeten und nicht mehr benötigten Speicher frei geben
	BIO_free_all(b64);

	// Rückgabe der Stringadresse
	return b64_str;
}

/**
 * Dekodiert einen Base64 String
 *
 * @param[in] data Zeiger auf Anfang des Strings
 * @param[out] len Länge der Originaldaten
 * @return Startadresse der Originaldaten
 */
void* base64_decode(char *data, size_t *len)
{
	// Parameter testen
	if(data==NULL || len==NULL) PARAM_FATAL(__FILE__, "base64_decode()");

	// Vorbereitungen
	BIO *b64 = NULL; // Zeiger auf Base64 Daten
	BIO *raw = NULL; // Zeiger auf Originaldaten
	BUF_MEM *praw = NULL; // Zeiger für Rohdaten im Puffer
	void *dec_data = NULL; // Zeiger auf den Anfang der dekodierten Daten

	// Speicher allokieren & vorbereiten
	b64 = BIO_new(BIO_f_base64()); // Base64 Daten
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Sicherstellen das OpenSSL nicht auf Newlines wartet
	raw = BIO_new_mem_buf(data, strlen(data)); // Platz für Originaldaten
	if(b64==NULL || raw==NULL) Fatal // Fehler
	(
		"base64_decode(): Speichstrukturen für Dekodierung konnten nicht erzeugt werden.",
		RET_OPENSSL_ERROR
	);
	raw = BIO_push(b64, raw); // Speicher zusammenlegen
	BIO_get_mem_ptr(raw, &praw); // BUF_MEM Struktur von b64 in praw abbilden
	if(praw==NULL) Fatal // Fehler
	(
		"base64_decode(): Speicherstruktur konnte nicht abgebildet werden.",
		RET_OPENSSL_ERROR
	);

	// Speicher für dekodierte Daten allokieren & vorbereiten
	dec_data = malloc(praw->length);
	memset(dec_data, 0x00, praw->length);
	int ret = BIO_read(raw, dec_data, praw->length); // Daten dekodiert in den Speicher lesen
	if(ret <= 0) Fatal // Fehler
	(
		"base64_decode(): String konnte nicht dekodiert werden. Es konnten keine Daten gelesen werden",
		RET_OPENSSL_ERROR
	);
	*len = (size_t)ret; // Länge merken
	BIO_free_all(raw); // Nich mehr benötigten Speicher frei geben

	// Zeiger auf dekodierte Daten zurückgeben
	return dec_data;
}

/**
 * Fatale Fehler die eine sofortige Beendigung des Programms zur Folge haben.
 *
 * @param[in] msg Nachricht die auf stderr ausgegeben wird
 * @param[in] code Rückgabecode
 */
void Fatal(char *msg, s_int code)
{
	fprintf(stderr, "[!!] FATAL: %s\n", msg);
	exit(code);
}

/**
 * Fatale Fehler aufgrund falscher Funktionsparameter
 *
 * @param[in] place Wo der Fehler passiert ist
 */
void ParamFatal(char *place)
{
	const char fatal[] = ": Funktionsparameter ungültig";
	char msg[strlen(place)+strlen(fatal)+1];
	memset(&msg, 0x00, sizeof(msg));
	snprintf(msg, sizeof(msg), "%s%s", place, fatal);
	Fatal(msg, RET_PARAM_ERROR);
}
/** ********** /FUNKTIONEN ********* */
