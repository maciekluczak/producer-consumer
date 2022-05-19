#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAINBUF 5
#define WZBUF 10
#define INDEXBUF 4

int main(int argc, char* argv[]) {

    int pid = getpid(); //POBIERANIE IDENTYFIKATORA PROCESU

	sem_t *SemProducer, *SemConsumer; // INICJALIZACJA SEMAFORÓW PRODUCENTA I KONSUMENTA
    sem_t *SemEmptyConsumer, *SemFullConsumer, *SemProducerCounter; // INICJALIZACJA SEMAFORÓW OCHRONNYCH DO SEKCJI KRYTYCZNYCH/ TABLICY WOLNYCH/ZAJĘTYCH MIEJSC, LICZNIK PRODUCENTÓW

    SemProducer = sem_open("/SemProducer", O_CREAT, 0600, MAINBUF); // INICJALIZACJA GŁÓWNEGO OCHRONNEGO SEMAFORA O ROZMIARACH MAINBUF, DOPUSZCZA 5 PRODUCENTÓW JEDNOCZEŚNIE
    SemConsumer = sem_open("/SemConsumer", O_CREAT, 0600, 0); // SEMAFOR OCHRONNY, OPUSZCZONY, PODNOSZONY PO ZAPISIE PRODUKTU PRZEZ PRODUCENTA
    SemEmptyConsumer = sem_open("/SemEmptyConsumer", O_CREAT, 0600, 1); //SEMAFOR OCHRONNY SEKCJI KRYTYCZNEJ ZAPISU TABLICY WOLNYCH DLA KONSUMENTA
    SemFullConsumer = sem_open("/SemFullConsumer", O_CREAT, 0600, 1); // SEMAFOR  OCHRONNY  SEKICJI KRYTYCZNEJ ODCZYTU DO TABLICY ZAJĘTYCH MIEJSC DLA KONSUMENTA
    SemProducerCounter = sem_open("/SemProducerCounter", O_CREAT, 0600, 0); // SEMAFOR ZLICZAJĄCY ILOŚĆ AKTYWNYCH PRODUCENTÓW

	int *valueOfSemConsumer, *valueOfSemProducer; //INICJALIZACJA WSKAŹNIKÓW NA ANONIMOWE OBSZARY PAMIĘCI

    //MALLOC PRZYDZIELA NOWĄ DYNAMICZNIE ALOKOWANĄ PAMIĘĆ W TRYBIE PRYWATNYM ANONIMOWYM, ZWRACA WSKAŹNIK NA NOWY BLOK PAMIĘCI
    valueOfSemConsumer = malloc(sizeof(int)); //WSKAŹNIK NA WARTOŚĆ KONSUMENTA
    valueOfSemProducer = malloc(sizeof(int));// WSKAŹNIK NA ILOŚĆ PRODUCENTÓW

	int fdPlace, fdBufWZ, fdIndex; // INICJALIZACAJA OBSZARÓW PAMIĘCI WSPÓŁDZIELONEJ BUFORÓW MAINBUF WZBUF INDEXBUF
    fdPlace = shm_open("/MAINBUF", O_CREAT | O_RDWR, 0600); // FUNKCJA SHM_OPEN TWORZY BLOKI PAMIĘCI WSPÓŁDZIELONEJ, FUNKCJA ZWRACA JEJ DESKRYPTOR
    fdBufWZ = shm_open("/WZBUF", O_CREAT  | O_RDWR, 0600);
    fdIndex = shm_open("/INDEXBUF", O_CREAT | O_RDWR, 0600);

    int *ptrBufWZ,  *ptrIndex;  //INICJALIZACJA WSKAŹNIKÓW NA OBSZARY PAMIĘCI WSPÓŁDZIELONEJ
    char *ptrPlace;              //POINTER/ WSKAŹNIK NA BLOK PAMIĘCI BUFMAIN

    //Operacja MMAP() powoduje, że do obszaru pliku można odnosić się jak do zwykłej tablicy bajtów w pamięci
    ptrPlace = mmap(NULL, MAINBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdPlace, 0); //ODWZOROWANIE PLIKU W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP
    ptrBufWZ = mmap(NULL, WZBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufWZ, 0);    // MAPUJE NA WIRTUALNY ADRES PRZESTRZENI
    ptrIndex = mmap(NULL, INDEXBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdIndex, 0);

	int productPlace; //ZMIENNA PRZECHOWUJĄCA INDEX PRODUKTU DO POBRANIA

	if (SemProducer==SEM_FAILED || SemConsumer==SEM_FAILED ||        //ZASTOSOWANIE SEKCJI WARUNKOWEJ IF WRAZIE WYSTĄPIENIA BŁĘDU OD STOSOWANYCH SEMAFORÓW
	SemEmptyConsumer==SEM_FAILED || SemFullConsumer==SEM_FAILED ||
	SemProducerCounter==SEM_FAILED){

	        perror("sem_open");     //Do obsługi błędów: perror, bada wartość zmiennej errno i wyświetla tekstowy opis błędu, który wystąpił.
                exit(1);
        }

	sem_getvalue(SemConsumer, valueOfSemConsumer); //POBIERA WARTOŚĆ SEMAFORA KONSUMENTA *SPRAWDZA CZY JEST JAKIŚ AKTYWNY, ZAPISUJE WARTOŚĆ DO ZMIENNEJ WSKAŹNIKOWEJ
	sem_getvalue(SemProducerCounter, valueOfSemProducer); // POBIERANIE WARTOŚĆ SEMAFORA LICZNIKA PRODUCENTÓW, SPRAWDZA CZY JEST AKTYWNY PRODUCENT

	if(*valueOfSemConsumer == 0 && *valueOfSemProducer == 0){ // WARUNEK SPRAWDZAJĄCY CZY JEST  ZEZWOLENIE NA PROCES KONSUMENTA I PRODUCENTA, JEŻELI NIE  TO KOŃCZY PRACE
	                                                            //- BRAK PRODUCENTA: BRAK NOWYCH PRODUKTÓW
	                                                            //-BRAK KONSUMENTA:  BRAK STARYCH PRODUKTÓW

		printf("Konsument %d: Brak aktywnych Producentów\nKoniec Pracy Procesu\n\n", pid);


		    sem_close(SemProducer);     //KONIEC PRACY KONSUMENTA- ZWALNIANIE SEMAFORÓW I ZMIENNYCH WSKAŹNIKOWYCH
	        sem_close(SemConsumer);
        	sem_close(SemEmptyConsumer);
        	sem_close(SemFullConsumer);
        	sem_close(SemProducerCounter);

		    sem_unlink("/SemProducer"); // USUWA PLIK Z REFERENCJĄ DO DANEGO SEMAFORA
		    sem_unlink("/SemConsumer");
      		sem_unlink("/SemEmptyConsumer");
        	sem_unlink("/SemFullConsumer");
        	sem_unlink("/SemProducerCounter");

            free(valueOfSemConsumer); //ZWALNIANIE WARTOŚCI PAMIĘCI WSKAŹNIKA

       	    free(valueOfSemProducer);

		return 0;
	}


	if (ptrPlace == MAP_FAILED || //SEKCJA WARUNKOWA SPRAWDZAJĄCA WYSTĄPIENIE BŁĘDU
	ptrBufWZ == MAP_FAILED ||
	ptrIndex == MAP_FAILED) {

                perror("mmap"); //Do obsługi błędów: perror, bada wartość zmiennej errno i wyświetla tekstowy opis błędu, który wystąpił.
                exit(1);
        }

	while(1){

		printf("\n_______________________________\nKonsument %d: Oczekiwanie...\n\n", pid);

		sem_wait(SemConsumer); // OPUSZCZENIE SEMAFORA OCHRONEGO, DOPUSZCZA REALIZACJE TYLE RAZY ILE PRODUCENTÓW ODŁOŻYŁO SWÓJ PRODUKT W PROCESIE PRODUCENTA
		sem_wait(SemFullConsumer);// OCHRONA SEKCJI KRYTYCZNEJ ODCZYTU ZAJĘTYCH INDEKSÓW

		productPlace = ptrBufWZ[ptrIndex[3] + WZBUF/2]; // OZNACZENIE INDEKSU POBRANIA WARTOŚĆ Z BUFMAIN
		// MIEJSCE JEST WZNACZANE ZA POMOCĄ WARTOŚCI Z POD BUFORA WZBUF[WARTOŚĆ INDEKSU BUF INDEKS(WSKAZUJE POZYCJE) + 5 PONIEWAŻ JEST TO SEKCJA DLA KONSUMENTA)]
		printf("Konsument %d Odczyt: \n-Wolny Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[3],productPlace);
		ptrIndex[3] = (ptrIndex[3] + 1) % MAINBUF; // OZNACZENIE DO OCZYTU KOLEJNEJ WARTOŚCI WZBUF

		sem_post(SemFullConsumer); //PODNIESIENIE SEMAFORA, ZWOLNIENIE SEKCJI KRYTYCZNEJ ODCZYTU ZAJĘTYCH MIEJSC


		printf("Konsument %d--> *Konsumowanie*\n\n", pid);
		sleep(rand()%5+4);
		ptrPlace[productPlace] = 0; // OZNACZENIE NA MAINBUF ZKONSUMOWANIA PRODUKTU

		sem_wait(SemEmptyConsumer); // OPUSZCZENIE SEMAFORA DO SEKCJI KRYTYCZNEJ ZAPISU

		ptrBufWZ[ptrIndex[2]] = productPlace; // ZAPISANIE W BUFORZE PRODUCENTA WZBUF WOLNEGO MIEJSCA
		printf("Producent %d Zapis:\n-Zajety Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[2], productPlace);
		ptrIndex[2] = (ptrIndex[2] + 1) % MAINBUF; // ZAPISANIE POD WARTOSCIA INDEKSU WOLNEGO PRODUKTU +1 MODULO ILOŚĆ MIEJSC NA GŁÓWNYM BUFORZE

		sem_post(SemEmptyConsumer); // PODNOSZENIE SEMAFORA SEKCJI KRYTYCZNEJ DO ZAPISU WOLNYCH MIEJSC
		sem_post(SemProducer);      // ZEZWOLENIE NA PROCES PRODUCENTA PO ZAKOŃCZENIU PRACY KONSUMENTA W SEKCJACH KRYTYCZNYCH

		printf("Konsument %d Wyjście\n\n\n", pid);
		sleep(rand()%5+4);
	}


	sem_close(SemProducer); //KONIEC PRACY KONSUMENTA- ZWALNIANIE SEMAFORÓW I ZMIENNYCH WSKAŹNIKOWYCH
	sem_close(SemConsumer);
	sem_close(SemEmptyConsumer);
	sem_close(SemFullConsumer);
	sem_close(SemProducerCounter);

	sem_unlink("/SemProducer"); // USUWA PLIK Z REFERENCJĄ DO DANEGO SEMAFORA
	sem_unlink("/SemConsumer");
	sem_unlink("/SemEmptyConsumer");
	sem_unlink("/SemFullConsumer");
	sem_unlink("/SemProducerCounter");

    free(valueOfSemConsumer); //ZWALNIANIE WARTOŚCI PAMIĘCI WSKAŹNIKA
	free(valueOfSemProducer);

	shm_unlink("/MAINBUF"); //USUWANIE BLOKU PAMIĘCI WSPÓŁDZIELONEJ
	shm_unlink("/WZBUF");
	shm_unlink("/INDEXBUF");

	munmap(ptrPlace, MAINBUF); //usuwa mapowanie dla wskazanego zakresu adresu
	munmap(ptrBufWZ, WZBUF);
	munmap(ptrIndex, INDEXBUF);

	return 0;
}

