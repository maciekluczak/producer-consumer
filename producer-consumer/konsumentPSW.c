#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MAINBUF 5 //SIZE BUFOR MAIN
#define WZBUF 10  //SIZE BUFOR WOLNYCH/ZAJETYCH MIEJSC
#define INDEXBUF 4 //SIZE BUFOR INDEKSÓW

int main(void) {
    int pid = getpid();

	sem_t *SemProducer, *SemConsumer;    //SEMAFORY WZAJEMNEGO WYKLUCZANIA PROCESU PRODUCENTA/KONSUMENTA
    sem_t *SemEmptyConsumer, *SemFullConsumer, *SemProducerCounter; //SEMAFORY OCHRONNE SEKCJI KRYTYCZNYCH ODCZYTU/ZAPISU, SEMAFOR ZLICZAJĄCY AKTYWNYCH PRODUCENTÓW

    SemProducer = sem_open("/SemProducer", O_CREAT, 0600, MAINBUF);
    SemConsumer = sem_open("/SemConsumer", O_CREAT, 0600, 0);
    SemEmptyConsumer = sem_open("/SemEmptyConsumer", O_CREAT, 0600, 1);
    SemFullConsumer = sem_open("/SemFullConsumer", O_CREAT, 0600, 1);
    SemProducerCounter = sem_open("/SemProducerCounter", O_CREAT, 0600, 0);

    if (SemProducer==SEM_FAILED || SemConsumer==SEM_FAILED ||
        SemEmptyConsumer==SEM_FAILED || SemFullConsumer==SEM_FAILED ||
        SemProducerCounter==SEM_FAILED){

        perror("sem_open");
        exit(1);
    }

    int productPlace;   //DANY INDEKS BUFORA MAINBUF

	int *valueOfSemConsumer, *valueOfSemProducer;
                                            // DYNAMICZNA ALOKACJA PAMIĘCI
    valueOfSemConsumer = malloc(sizeof(int));
    valueOfSemProducer = malloc(sizeof(int));

	int fdPlace, fdBufWZ, fdIndex;          //INICJALIZACJA DESKRYPTORÓW BLOKÓW PAMIĘCI WSPÓŁDZIELONEJ

    fdPlace = shm_open("/MAINBUF", O_CREAT | O_RDWR, 0600);
    fdBufWZ = shm_open("/WZBUF", O_CREAT  | O_RDWR, 0600);
    fdIndex = shm_open("/INDEXBUF", O_CREAT | O_RDWR, 0600);

    int *ptrBufWZ,  *ptrIndex, *ptrPlace;   //INICJALIZACJA POINTERÓW ODWZOROWANIA PLIKÓW W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP
                                            //ODWZOROWANIE PLIKU W PAMIĘCI ZA POMOCĄ FUNKCJI MMAP
    ptrPlace = mmap(NULL, MAINBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdPlace, 0);
    ptrBufWZ = mmap(NULL, WZBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdBufWZ, 0);
    ptrIndex = mmap(NULL, INDEXBUF, PROT_READ | PROT_WRITE, MAP_SHARED, fdIndex, 0);

    if (ptrPlace == MAP_FAILED ||
        ptrBufWZ == MAP_FAILED ||
        ptrIndex == MAP_FAILED) {

        perror("mmap");
        exit(1);
    }


	sem_getvalue(SemConsumer, valueOfSemConsumer);              //POBRANIE WARTOŚCI SemConsumer DO valueOfSemConsumer
	sem_getvalue(SemProducerCounter, valueOfSemProducer);       //POBRANIE WARTOŚCI O AKTYWNYCH PRODUCENTACH

	if(*valueOfSemConsumer == 0 && *valueOfSemProducer == 0){   //SEKCJA WARUNKOWA

		printf("\nKonsument %d: Brak aktywnych Producentów\n\nKoniec Pracy Procesu\n\n", pid);

		    //INICJACJA KOŃCZENIA PROCESU KONSUMENTA KIEDY BRAK AKTYWNYCH PRODUCENTÓW ORAZ KONSUMENTÓW
		    sem_close(SemProducer);
	        sem_close(SemConsumer);
        	sem_close(SemEmptyConsumer);
        	sem_close(SemFullConsumer);
        	sem_close(SemProducerCounter);

		    sem_unlink("/SemProducer");
		    sem_unlink("/SemConsumer");
      		sem_unlink("/SemEmptyConsumer");
        	sem_unlink("/SemFullConsumer");
        	sem_unlink("/SemProducerCounter");

            free(valueOfSemConsumer);
       	    free(valueOfSemProducer);

		return 0;
	}

	while(1){

		printf("\n_______________________________\nKonsument %d: Oczekiwanie...\n\n", pid);

		sem_wait(SemConsumer);  //OPUSZCZENIE SemConsumer, ZAPEWNIENIE DOPUSZCZENIA ILOŚCI PROCESÓW <= UTWORZONYCH PRODUKTÓW
		sem_wait(SemFullConsumer);// SEMAFOR BINARNY SemFullConsumer OCHRONA SEKCJI KRYTYCZNEJ ODCZYTU ZAJĘTYCH MIEJSC

		productPlace = ptrBufWZ[ptrIndex[3] + WZBUF/2];
		printf("Konsument %d Odczyt: \n-Zajęty Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[3],productPlace);
		ptrIndex[3] = (ptrIndex[3] + 1) % MAINBUF;

		sem_post(SemFullConsumer); //PODNIESIENIE SemFullConsumer, DOPUSZCZENIE KOLEJNEGO PROCESU KONSUMENTA DO ODCZYTU


		printf("Konsument %d--> *Konsumowanie*\n\n", pid);
		sleep(rand()%5+4);
		ptrPlace[productPlace] = 0;

		sem_wait(SemEmptyConsumer);  //SEMAFOR BINARNY SemEmptyConsumer OCHRONA SEKCJI KRYTYCZNEJ ZAPISU WOLNYCH MIEJSC

        ptrBufWZ[ptrIndex[2]] = productPlace;
		printf("Producent %d Zapis:\n-Wolny Indeks: %d\n-Miejsce Docelowe: %d\n\n", pid, ptrIndex[2], productPlace);
		ptrIndex[2] = (ptrIndex[2] + 1) % MAINBUF;

		sem_post(SemEmptyConsumer); //PODNIESIENIE SemEmptyConsumer, DOPUSZCZENIE KOLEJNEGO KONSUMENTA DO ZAPISU
		sem_post(SemProducer); //PODNIESIENIE SemProducer, DOPUSZCZENIE PROCESU PRODUCENTA DO PRODUKCJI


        printf("Konsument %d Wyjście\n\n\n", pid);
		sleep(rand()%5+4);

		//PROCES KONSUMENTA PĘTLA: OCZEKUJE PRODUKTÓW
	}

    //INICJACJA ZAKOŃCZENIA PROCESU KONSUMENTA
	sem_close(SemProducer);
	sem_close(SemConsumer);
	sem_close(SemEmptyConsumer);
	sem_close(SemFullConsumer);
	sem_close(SemProducerCounter);

	sem_unlink("/SemProducer");
	sem_unlink("/SemConsumer");
	sem_unlink("/SemEmptyConsumer");
	sem_unlink("/SemFullConsumer");
	sem_unlink("/SemProducerCounter");

    free(valueOfSemConsumer);
	free(valueOfSemProducer);

	shm_unlink("/MAINBUF");
	shm_unlink("/WZBUF");
	shm_unlink("/INDEXBUF");

	munmap(ptrPlace, MAINBUF);
	munmap(ptrBufWZ, WZBUF);
	munmap(ptrIndex, INDEXBUF);

	return 0;
}

