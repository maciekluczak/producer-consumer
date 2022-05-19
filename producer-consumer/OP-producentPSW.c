#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAINBUF 5           //ROZMIAR BUFORA GŁÓWNEGO- 5, W TYM BUFORZE PRODUCENT SKŁADA PRODUKTY, KONSUMER KONSUMUJE JE
#define WZBUF	10          //ROZMIAR BUFORA WOLNE ZAJĘTE, PRZECHOWUJE BUFORY DLA PRODUCENTA (WOLNE)[0,4], I KONSUMENTA (ZAJĘTE)[5,9]

#define INDEXBUF 4          //ROZMIAR BUFORA Z 4 INDEKSAMI [0,1] PRODUCENT INDEKS [0] ZAPISZ ZAJETE 1 ODCZYT WOLNE
                            //[2,3] KONSUMENT INDEKS 2 ZAPIS WOLNE, INDEKS [3] ODCZYT ZAJETE

int main(int argc, char* argv[]) {

    int pid = getpid(); //DESKRYPTOR/ ID PROCESU

	sem_t *SemProducer, *SemConsumer;   //SEMAFOR PRODUCENTA PODNIESIONY NA START DO ROMIARU MAINBUFOR- 5, SEMAFOR KONSUMENTA NA START WARTOŚĆ 0
	sem_t *SemInitFirst, *SemInit;      // SEMAFOR DLA PIERWSZEGO PRODUCENTA W CELU ZAINICJOWANIA BUFORA
	sem_t *SemEmptyProducer, *SemFullProducer, *SemProducerCounter; // SEMAFOR OD ILOŚCI WOLNYCH MIEJSC, OD ILOŚCI PEŁNYCH MIEJSC, SEMAFOR LICZĄCY ILOŚĆ PRODUCENTÓW

    // TWORZENIE SEMAFORÓW NAZWANYCH SEM_OPEN(NAZWA, OFLAG, PRAWA DOSTĘPU, WARTOŚĆ POCZĄTKOWA
    // sem_open zwaraca wskaźnik do struktury sem_t gdzie w specjalnym folderze /dev/shm/ powstaje plik "sem.nazwaSemafora"

    SemProducer = sem_open("/SemProducer", O_CREAT, 0600, MAINBUF); //INICJALIZACJA SEMAFORU PRODUCENTA 0600 PEŁNY DOSTĘP DO ODCZYTU ORAZ ZAPISU PLIKU DLA UŻYTKOWNIKA
    SemConsumer = sem_open("/SemConsumer", O_CREAT, 0600, 0);       //INICJALIZACJA SEMAFORU KONSUMENTA OPUSZCZONEGO DO 0 NA START

    SemInitFirst = sem_open("/initFirst", O_CREAT, 0600, 1); // INICJALIZACJA SEMAFORA DLA PIERWSZEGO PRODUCENTA (SEMAFOR BINARNY!) WARTOŚĆ 1 NA START
    SemInit = sem_open("/init",O_CREAT,0600, 0);             // INICJALIZACJA SEMAFORA WARUNKOWY, INICJUJĄCY BUFORY

    SemEmptyProducer = sem_open("/SemEmptyProducer", O_CREAT, 0600, 1);     //SEMAFOR CHRONI BUFOR OD WOLNYCH MIEJSC (WZBUF), USTAWIONY NA START NA 1
    SemFullProducer = sem_open("/SemFullProducer", O_CREAT, 0600, 1);       //SEMAFOR CHRONI BUFOR OD ZAJĘTYCH  MIEJSC (WZBUF)
    SemProducerCounter = sem_open("/SemProducerCounter", O_CREAT, 0600, 0); //SEMAFOR ZLICZA ILOŚĆ PROCESÓW PRODUCENTA


	int fdPlace,fdBufWZ, fdBufIndex,  *valueOfInit ;                //INICJALIZACJA DESKRYPTORÓW, *WSKAŹNIKA ZWRACANEJ WARTOŚCI FUNKCJI MALLOC

    // FUNKCJA SHM_OPEN TWORZY BLOKI PAMIĘCI WSPÓŁDZIELONEJ, FUNKCJA ZWRACA JEJ DESKRYPTOR
    fdPlace = shm_open("/MAINBUF", O_CREAT | O_RDWR, 0600);         //INICJALIZACJA BLOKU PAMIĘCI WSPÓŁDZIELONEJ GŁÓWNEGO BUFORA MAINBUF
    fdBufWZ = shm_open("/WZBUF", O_CREAT  | O_RDWR, 0600);          //INICJALIZACJA BLOKU PAMIECI WSPÓŁDZIELONEJ BUFORA WOLNE/ZAJETE WZBUF
    fdBufIndex = shm_open("/INDEXBUF", O_CREAT | O_RDWR, 0600);     //INICJALIZACJA BLOKU PAMIĘCI WSPÓŁDZIELONEJ DLA BUFORA Z INDEKSAMI

    valueOfInit = malloc(sizeof(int));  //MALLOC PRZYDZIELA NOWĄ DYNAMICZNIE ALOKOWANĄ PAMIĘĆ W TRYBIE PRYWATNYM ANONIMOWYM, ZWRACA WSKAŹNIK NA NOWY BLOK PAMIĘCI

    int productPlace, *ptrBufWZ, *ptrIndex ; //DEFINIOWANIE POINTERÓW NA NOWE BLOKI PAMIĘCI BUFWZ, BUFINDEX PRZYDZIELANYCH PÓŹNIEJ, ORAZ INT OD MIEJSCA ZŁOŻENIA PRODUKTU
	char *ptrPlace;     //POINTER/ WSKAŹNIK NA BLOK PAMIĘCI BUFMAIN



	if (SemProducer==SEM_FAILED || SemConsumer==SEM_FAILED ||         //ZASTOSOWANIE SEKCJI WARUNKOWEJ IF WRAZIE WYSTĄPIENIA BŁĘDU OD STOSOWANYCH SEMAFORÓW
	SemInitFirst==SEM_FAILED || SemInit==SEM_FAILED ||
	SemEmptyProducer==SEM_FAILED || SemFullProducer==SEM_FAILED ||
	SemProducerCounter==SEM_FAILED)
	{
	        perror("sem_open"); //Do obsługi błędów: perror, bada wartość zmiennej errno i wyświetla tekstowy opis błędu, który wystąpił.

                exit(1);
        }

	sem_post(SemProducerCounter); //WYWOŁANIE PIERWSZEGO SEMAFORA, PODNIESIENIE +1, SEMAFOR ZLICZA PIERWSZEGO PRODUCENTA

	sem_wait(SemInitFirst); // SEMAFOR INICJALIZACJI PIERWSZEGO PRODUCENTA ZOSTAJE OPUSZCZONY W CELU ZAPEWNIENIA
	                        // OCHRONY SEKCJI KTÓRA POWINNA WYWOŁAĆ SIĘ JEDYNIE RAZ- INICJALIZACJA BUFORÓW

	sem_getvalue(SemInit, valueOfInit); // POBIERANIE WARTOŚCI SemInit DO WSKAŹNIKA ValueOfInit W CELU ZAPEWNIENIA OCHRONY W INICJALIZACJI BUFORA

	if (*valueOfInit == 0) //WYKONA SIĘ TYLKO RAZ
	{

		ftruncate(fdPlace, MAINBUF);        //WYZNACZANIE ROZMIARÓW WCZEŚNIEJ ZAINICJONOWANYCH BLOKÓW PAMIĘCI WSPÓŁDZIELONYCH ZA POMOCĄ:
		ftruncate(fdBufWZ, WZBUF);          // FUNKCJA FTRUNCATE(DESKRYPTOR BLOKU PAMIECI, <SIZE>ZDEFINIOWANY POWYŻEJ DLA KAŻDEGO Z BUFORÓW PAMIECI)
		ftruncate(fdBufIndex, INDEXBUF);    //Funkcja ftruncate() powoduje ustawienie zadanego rozmiaru pliku bez alokacji przestrzeni dyskowej

        ptrPlace = mmap(NULL, MAINBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdPlace, 0); //ODWZOROWANIE PLIKU W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP
        ptrBufWZ = mmap(NULL, WZBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufWZ, 0);   // MAPUJE NA WIRTUALNY ADRES PRZESTRZENI
        ptrIndex = mmap(NULL, INDEXBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufIndex, 0);
        //Operacja MMAP() powoduje, że do obszaru pliku można odnosić się jak do zwykłej tablicy bajtów w pamięci

		if (ptrPlace == MAP_FAILED ||   //ZASTOSOWANIE SEKCJI WARUNKOWEJ IF WRAZIE WYSTĄPIENIA BŁĘDU Z ODZOROWANIE PLIKU W PAMIECI
		ptrBufWZ == MAP_FAILED ||
		ptrIndex == MAP_FAILED)
		{
            perror("mmap"); //Do obsługi błędów: perror, bada wartość zmiennej errno i wyświetla tekstowy opis błędu, który wystąpił.
            exit(1);
 	       	}

		for(int i = 0; i < WZBUF/2; i++){       //PĘTLA FOR O ROZMIARZE POŁOWY BOUFORA WZBUF, WYPEŁNIA BUFOR INDEKSAMI WOLNYCH MIEJSC W MAIN BUFORZE
		                                        // [1,2,3,4,5,0,0,0,0,0], WYPEŁNIONE INDEKSY W PIERWSZEJ POŁOWIE -WOLNE MIEJSCA
            ptrBufWZ[i] = i;                    // PRODUCENT POTRZEBUJE JE ZAPEŁNIĆ

		}
        //ZAINICJOWANY BUFOR INDEKSÓW WYPEŁNIAMY ZERAMI (0) BUFOR INDEKS SŁUŻY DO
		ptrIndex[0] = 0;      //PRODUCENT ZAPISZ ZAJĘTE
		ptrIndex[1] = 0;      //PRODUCENT ODCZYT WOLNE
		ptrIndex[2] = 0;      //KONSUMENT ZAPISZ WOLNE
		ptrIndex[3] = 0;      //KONSUMENT ODCZYT ZAJĘTE

		sem_post(SemInit);       //PODNOSZENIE SEMAFORA INICJALIZACJI * PĘTLA POWYŻSZA I TAK WYKONA SIĘ JEDEN RAZ PONIEWAŻ SEMAFOR JEST RÓŻNY OD ZERA
		sem_post(SemInitFirst); //PODNOSZENIE SEMAFORA PIERWSZEGO PRODUCENTA, DOPUSZCZA INNYM PROCESOM DOSTANIE SIĘ DO WARUNKU PONIŻSZEGO ELSE
		                        //!SEMAFOR SemInitFirst CHRONI SEKCJĘ KRYTYCZNĄ, NIE DOPUŚCI NIGDY DWA PROCESY PRODUCENTA DO REALIZACJI INICJOWANIA BUFORÓW
		                        //!czyli WYPEŁNIENIA ICH POCZĄTKOWYMI WARTOŚCIAMI (SPEŁNIONE WZAJEMNE WYKLUCZANIE PROCESÓW), NATOMIAST
		                        //!SEMAFOR SemInit ZAPEWNIA INICJALIZACJI TYLKO I WYŁĄCZNIE 1 PROCESOWI GDYŻ NIGDY NIE OSIĄGNIE 0 -nazewnictwo powinno być chyba odwrotnie

	}

	else        //INICJALIZACJA ODWZOROWANIE PLIKU W PAMIĘCI DLA KAŻDEGO KOLEJNEGO PROCESU PRODUCENTA
	    {
        ptrPlace = mmap(NULL, MAINBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdPlace, 0);     //|ODWZOROWANIE PLIKU W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP
        ptrBufWZ = mmap(NULL, WZBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufWZ, 0);       //| MAPUJE NA WIRTUALNY ADRES PRZESTRZENI
        ptrIndex = mmap(NULL, INDEXBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufIndex, 0); //|ODWZOROWUJE JUŻ WCZEŚNIEJ UTWORZONY BLOK PAMIĘCI WSPÓŁDZIELONY JAKO PLIK
        //Operacja MMAP() powoduje, że do obszaru pliku można odnosić się jak do zwykłej tablicy bajtów w pamięci
		if (ptrPlace == MAP_FAILED || //MAP_FAILED ZWRACA BŁĄD
		ptrBufWZ == MAP_FAILED ||
		ptrIndex == MAP_FAILED)
		{
            perror("mmap"); //Do obsługi błędów: perror, bada wartość zmiennej errno i wyświetla tekstowy opis błędu, który wystąpił.
            exit(1);
                }
	    }

	sem_post(SemInitFirst); //PODNOSZENIE SEMAFORA SEKCJI KRYTYCZNEJ, DOPUSZCZENIE INNYCH PROCESÓW DO INICJALIZACJI PLIKÓW PAMIĘCI WSPÓŁDZIELONEJ

	for(int  i = 0; i < atoi(argv[1]); i ++){   //FOR WYKONYWANE Z DOKŁADNOŚCIĄ DO PODANEJ LICZBY ARGUMENTÓW PRZED WYWOŁANIEM PROCESU
	                                            //ATOI() ZMIENIA STRING NA LICZBE

		printf("\n_______________________________\nProducent %d:\nOczekiwanie...\n\n", pid);
        //! SEMAFOR SemProducer OGRANICZA MOŻLIWOŚĆ WEJŚCIA DO MAX 5 PRODUCENTÓW DO SEKCJI,
        //! DANA SEKCJA JEST PODZIELONA NA SEKCJE KRYTYCZNE DO ODCZYTU I ZAPISU
        //! KTÓRE SĄ CHRONIONE SEMAFORAMI BINARNYMI, SPEŁNIONY JEST WARUNEK WZAJEMNEGO WYKLUCZANIA SIĘ
		sem_wait(SemProducer);          //OPUSZCZENIE SEMAFORA SEKCJI KRYTYCZNEJ DLA PRODUCENTA- CAŁY PROCES, WARTOŚĆ SEMAFORA MAINBUF SIZE - 5
		sem_wait(SemEmptyProducer);     //OPUSZCZENIE SEMAFORA SEKCJI KRYTYCZNEJ  ODCZYTANIE WOLNYCH MIEJSC

		productPlace = ptrBufWZ[ptrIndex[1]];       //!MIEJSCE SKŁADOWANIA PRODUKTU POBIERANE Z ODCZYTU WARTOŚCI ptrIndex DLA INDEKSU WZBUF,
		                                            //CZYLI, Z POD WARTOŚCI ptrIndex[1] (odczyt Wolnych miejsc) ODCZYTYWANA JEST WARTOŚĆ INDEKSU WZBUF
		                                            // BRAKUJĄCYM PRODUKTEM DO UZUPEŁNIENIA

		printf("Producent %d Odczyt: \n-Wolny Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[1],productPlace);
		sleep(rand()%4+1); // ODCZEKANIE
		ptrIndex[1] = (ptrIndex[1] + 1) % MAINBUF; //WARTOŚĆ PTRINDEX[1] TO AKTUALNA WARTOŚĆ + 1 MODULO ROZMIAR BUFORA SKŁADOWANIA PRODUKTÓW, WSKAZUJE NASTĘPNE WOLNE MIEJSCE

		sem_post(SemEmptyProducer); //SEKCJA KRYTYCZNA ODCZYTU ZOSTAŁA ZREALIZOWANA, PODNOSZENIE SEMAFORA

		printf("Producent %d --> *Produkcja*\n\n", pid);
		sleep(rand()%5+3);
		ptrPlace[productPlace] = 1; //POSTAWIENIE W WYZNACZONYM MIEJSCU NIEPUSTEGO PRODUKTU, KAŻDY PROCES POSIADA WSKAŹNIK NA INNĄ KOMÓRKĘ TABLICY

		sem_wait(SemFullProducer); //OPUSZCZANIE SEMAFORA PRZED SEKCJĄ KRYTYCZNĄ DLA ZAPISU DANYCH W WSPÓLDZIELONEJ PAMIĘCI ZAPISU

		ptrBufWZ[ptrIndex[0]+WZBUF/2] = productPlace; //ZAZNACZENIE W SEKCJI DLA ZAJĘTYCH BUFORA WZBUF ŻE NA DANYM MIEJSCU ZOSTAŁ ODŁOŻONY PRODUKT
		printf("Producent %d Zapis:\n-Zajety Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[0], productPlace);
		sleep(rand()%4+1);
		ptrIndex[0] = (ptrIndex[0] + 1) % MAINBUF;// NASTĘPNY ELEMENT Z TABLICY DO ZAPISU

		sem_post(SemFullProducer); //PODNOSZENIE SEMAFORA SEKCJI KRYTYCZNEJ
		sem_post(SemConsumer); //ZEZWOLENIE NA DOSTĘP KONSUMENTA PO UTWORZENIU PIERWSZEGO PRODUKTU

		printf("Producent %d Wyjście\n\n\n", pid);
		sleep(rand()%5+3);
	}
	printf("______________________________________\nProducent %d:  Koniec Pracy Procesu\n\n", pid);

	sem_wait(SemProducerCounter); //ZWALNIANIE ILOŚCI AKTYWNYCH PROCESÓW PRODUCENTA

	sem_close(SemProducer); //ZAMYKANIE SEMAFORÓW
	sem_close(SemConsumer);
	sem_close(SemInitFirst);
	sem_close(SemInit);
	sem_close(SemEmptyProducer);
	sem_close(SemFullProducer);
	sem_close(SemProducerCounter);

	free(valueOfInit);    //ZWALNIA PAMIĘĆ POINTERA

	munmap(ptrPlace, MAINBUF); //usuwa mapowanie dla wskazanego zakresu adresu
	munmap(ptrBufWZ, WZBUF);
	munmap(ptrIndex, INDEXBUF);

	return 0;
}
