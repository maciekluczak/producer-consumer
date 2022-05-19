#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAINBUF 5   //SIZE BUFOR MAIN
#define WZBUF	10  //SIZE BUFOR WOLNYCH/ZAJETYCH MIEJSC
#define INDEXBUF 4  //SIZE BUFOR INDEKSÓW

int main(int argumentsQuantity, char* argumentsArray[]) {

    int pid = getpid(); // ID PROCESU

	sem_t *SemProducer, *SemConsumer;   //SEMAFORY WZAJEMNEGO WYKLUCZANIA PROCESU PRODUCENTA/KONSUMENTA
	sem_t *SemInitFirst, *SemInit;      //SEMAFORY OCHRONNE DO INICJACJI PAMIĘCI WSPÓŁDZIELONYCH
	sem_t *SemEmptyProducer, *SemFullProducer, *SemProducerCounter; //SEMAFORY OCHRONNE SEKCJI KRYTYCZNYCH ODCZYTU/ZAPISU, SEMAFOR ZLICZAJĄCY AKTYWNYCH PRODUCENTÓW

    SemProducer = sem_open("/SemProducer", O_CREAT, 0600, MAINBUF);
    SemConsumer = sem_open("/SemConsumer", O_CREAT, 0600, 0);

    SemInitFirst = sem_open("/initFirst", O_CREAT, 0600, 1);
    SemInit = sem_open("/init",O_CREAT,0600, 0);

    SemEmptyProducer = sem_open("/SemEmptyProducer", O_CREAT, 0600, 1);
    SemFullProducer = sem_open("/SemFullProducer", O_CREAT, 0600, 1);
    SemProducerCounter = sem_open("/SemProducerCounter", O_CREAT, 0600, 0);

    if (SemProducer==SEM_FAILED || SemConsumer==SEM_FAILED ||
        SemInitFirst==SEM_FAILED || SemInit==SEM_FAILED ||
        SemEmptyProducer==SEM_FAILED || SemFullProducer==SEM_FAILED ||
        SemProducerCounter==SEM_FAILED)
    {
        perror("sem_open");
        exit(1);
    }


	int fdPlace,fdBufWZ, fdBufIndex,  *valueOfInit ;        //INICJALIZACJA DESKRYPTORÓW BLOKÓW PAMIĘCI WSPÓŁDZIELONEJ
    fdPlace = shm_open("/MAINBUF", O_CREAT | O_RDWR, 0600);
    fdBufWZ = shm_open("/WZBUF", O_CREAT  | O_RDWR, 0600);
    fdBufIndex = shm_open("/INDEXBUF", O_CREAT | O_RDWR, 0600);

    valueOfInit = malloc(sizeof(int)); // DYNAMICZNA ALOKACJA PAMIĘCI

    int *ptrBufWZ, *ptrIndex,  *ptrPlace; ; //INICJALIZACJA POINTERÓW ODWZOROWANIA PLIKÓW W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP

    int productPlace;   //DANY INDEKS BUFORA MAINBUF


	sem_post(SemProducerCounter); //PODNIESIENIE SemProducerCounter ZLICZAJĄCEGO AKTYWNYCH PRODUENTÓW
	sem_wait(SemInitFirst);       //ZAPEWNIENIE WZAJEMNEGO WYKLUCZENIA DWÓCH PROCESÓW PRODUCENTA DO SEKCJI KRYTYCZNEJ

	sem_getvalue(SemInit, valueOfInit);// ODCZYT WARTOŚCI SemInit, ZAPIS WARTOŚCI POD WSKAŹNIKIEM valueOfInit

	if (*valueOfInit == 0)
	{

		ftruncate(fdPlace, MAINBUF); //USTAWIANIE ZADANEGO ROZMIARU BLOKU PAMIĘCI
		ftruncate(fdBufWZ, WZBUF);
		ftruncate(fdBufIndex, INDEXBUF);

        ptrPlace = mmap(NULL, MAINBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdPlace, 0);     //ODWZOROWANIE PLIKU W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP
        ptrBufWZ = mmap(NULL, WZBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufWZ, 0);
        ptrIndex = mmap(NULL, INDEXBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufIndex, 0);

		if (ptrPlace == MAP_FAILED ||
		ptrBufWZ == MAP_FAILED ||
		ptrIndex == MAP_FAILED)
		{
            perror("mmap");
            exit(1);
 	       	}

		for(int i = 0; i < WZBUF/2; i++){ //WYPEŁNIENIE BUFORA INFORMACJĄ O WOLNYCH MIEJSCACH W GŁÓWNYM BUFORZE
			ptrBufWZ[i] = i;
		}

        // BUFOR INDEKSÓW
		ptrIndex[0] = 0; //PRODUCENT ZAPISZ ZAJĘTE
		ptrIndex[1] = 0; //PRODUCENT ODCZYT WOLNE
		ptrIndex[2] = 0; //KONSUMENT ZAPISZ WOLNE
		ptrIndex[3] = 0; //KONSUMENT ODCZYT ZAJĘTE

		sem_post(SemInit); //ZAPEWNIENIE JEDNOKROTNEGO WYKONANIA INICJALIZACJI BUFORÓW
		sem_post(SemInitFirst); // PODNIESIENIE SEMAFORA OCHRONEGO SEKCJI KRYTYCZNEJ

	}

	else
	    {
        ptrPlace = mmap(NULL, MAINBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdPlace, 0);  //ODWZOROWANIE PLIKU W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP
        ptrBufWZ = mmap(NULL, WZBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufWZ, 0);    //DLA KAŻDEGO NIE PIERWSZEGO PROCESU PRODUCENTA
        ptrIndex = mmap(NULL, INDEXBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufIndex, 0);

		if (ptrPlace == MAP_FAILED ||
		ptrBufWZ == MAP_FAILED ||
		ptrIndex == MAP_FAILED)
		{
            perror("mmap");
            exit(1);
                }
	    }

	sem_post(SemInitFirst); // PODNIESIENIE SEMAFORA OCHRONEGO SEKCJI KRYTYCZNEJ

	for(int  i = 0; i < atoi(argumentsArray[1]); i ++){ //WYKONA SIĘ DANĄ W ARGUMENCIE LICZBĘ RAZY

		printf("\n_______________________________\nProducent %d:\nOczekiwanie...\n\n", pid);

		sem_wait(SemProducer);       //OPUSZCZENIE SemProducer, ZAPEWNIENIE DOPUSZCZENIA ILOŚCI PROCESÓW <= SIZE MAINBUF
		sem_wait(SemEmptyProducer);  // SEMAFOR BINARNY SemEmptyProducer OCHRONA SEKCJI KRYTYCZNEJ ODCZYTU WOLNYCH MIEJSC

		productPlace = ptrBufWZ[ptrIndex[1]];
		printf("Producent %d Odczyt: \n-Wolny Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[1],productPlace);
		sleep(rand()%4+1);
		ptrIndex[1] = (ptrIndex[1] + 1) % MAINBUF;

		sem_post(SemEmptyProducer); //PODNIESIENIE SemEmptyProducer, DOPUSZCZENIE KOLEJNEGO PROCESU PRODUCENTA

		printf("Producent %d --> *Produkcja*\n\n", pid);
		sleep(rand()%5+3);
		ptrPlace[productPlace] = 1;

		sem_wait(SemFullProducer); //SEMAFOR BINARNY SemFullProducer OCHRONA SEKCJI KRYTYCZNEJ ZAPISU ZAJĘTYCH MIEJSC

		ptrBufWZ[ptrIndex[0]+WZBUF/2] = productPlace;
		printf("Producent %d Zapis:\n-Zajety Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[0], productPlace);
		sleep(rand()%4+1);
		ptrIndex[0] = (ptrIndex[0] + 1) % MAINBUF;

		sem_post(SemFullProducer); //PODNIESIENIE SemFullProducer, DOPUSZCZENIE KOLEJNEGO PROCESU PRODUCENTA
		sem_post(SemConsumer); //PODNIESIENIE SemConsumer, DOPUSZCZENIE PROCESU KONSUMENTA DO KONSUMOWANIA

		printf("Producent %d Wyjście\n\n\n", pid);
		sleep(rand()%5+3);
	}
	printf("______________________________________\nProducent %d:  Koniec Pracy Procesu\n\n", pid);

	sem_wait(SemProducerCounter); //ZWALNIANIE SEMAFORA ZLICZAJĄCEGO AKTYWNYCH PROCESÓW PRODUCENTA

    //INICJACJA ZAKOŃCZENIA PROCESU PRODUCENTA
	sem_close(SemProducer);
	sem_close(SemConsumer);
	sem_close(SemInitFirst);
	sem_close(SemInit);
	sem_close(SemEmptyProducer);
	sem_close(SemFullProducer);
	sem_close(SemProducerCounter);

	free(valueOfInit);

	munmap(ptrPlace, MAINBUF);
	munmap(ptrBufWZ, WZBUF);
	munmap(ptrIndex, INDEXBUF);

	return 0;
}
