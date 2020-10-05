#include <algorithm>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <vector>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define ACK_TAG 1
#define REQK_TAG 2
#define REQM_TAG 2
#define REQS_TAG 2

#define MASC 4
#define MAGNE 7
#define SALKA 5

int size,rank; /* nie trzeba zerować, bo zmienna globalna statyczna */
MPI_Datatype MPI_PAKIET_T;
MPI_Status status;
int routine = 1; //0 - start, czekanie na partnera, 1, 2, 3 - czekanie na zasoby, 4 - taniec, 5 - odpoczynek
struct msgtype{
    int time;       
    int src;
	int	resource;
};
int lamportClock = 0;
struct msgtype msgrec;
struct msgtype msgsend;
struct msgtype msgcoll;
pthread_mutex_t reqMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t printMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lampMut = PTHREAD_MUTEX_INITIALIZER;
int defresources[3] = {MASC, MAGNE, SALKA}; // tablica z calkowita iloscia zasobow
int acks[3] = {0};
bool sent[3] = {false};
int queue1[size] = {0};
int queue2[size] = {0};
int queue3[size] = {0};
//tablica o rozmiarze rownym ilosci procesow, jesli queue[x] jest rowne 0 to proces x nie jest w kolejce, jak 1 to jest
void addToQueue(int src, int rsr){
	switch(rsr){
		case 1:
			queue1[src] = 1;
		break;
		case 2:
			queue2[src] = 1;
		break;
		case 3:
			queue3[src] = 1;
		break;
	}
}
bool isCollected(int restype){
	int num = size - acks[restype];
	if(num <= defresources[restype]){
		return true;
	}
	return false;
}

void collect(int res){
	if(!sent[res]){
		incLamport();
		msgcoll.time = lamportClock;
		msgcoll.resource = res;
		msgcoll.src = rank;
		int tag;
		switch(res){
			case 1:
				tag = REQK_TAG;
				break;
			case 2:
				tag = REQM_TAG;
				break;
			case 3:
				tag = REQS_TAG;
		}
		for(int i=0; i < size; i++){
			if(i != rank){
				MPI_Send(&request, 1, MPI_PAKIET_T,  i, tag, MPI_COMM_WORLD);
			}
		}
		sent[res] = true;
	}
}

void dance(){
	srand (time(NULL));
	pthread_mutex_lock(&printMut);
	printf("Jestem proces %d i zaczynam tanczyc\n", rank);
	pthread_mutex_unlock(&printMut);
	int tosleep = rand() % 10 + 1;
	sleep(tosleep);
}

void incLamport(){
	pthread_mutex_lock(&lampMut);
	lamportClock++;
	pthread_mutex_unlock(&lampMut);
}

void * listening(void * arg){
	while(true){
		MPI_Recv(&msgrec, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	}
	switch(routine){
		case 1:
		case 2:
		case 3:
			if (status.MPI_TAG == ACK_TAG && msg.resource == routine) {
				pthread_mutex_lock(&reqMut);
				acks[routine]++;
				pthread_mutex_unlock(&reqMut);
			}
			else if ((status.MPI_TAG == REQM_TAG && routine == 1)
				|| (status.MPI_TAG == REQK_TAG && routine == 2)
				|| (status.MPI_TAG == REQS_TAG && routine == 3)){
				if((msgrec.time < lamportClock) || (msgrec.time == lamportClock && rank > msgrec.src){
					incLamport();
					msgsend.time = lamportClock;
					msgsend.resource = routine;
					msgsend.src = rank;
					MPI_Send(&msgsend, 1, MPI_PAKIET_T, msgrec.src, ACK_TAG, MPI_COMM_WORLD);
				}
				else{
					addToQueue(msgrec.src, routine);
				}
			}
			else if ((status.MPI_TAG == REQK_TAG && routine < 2) || (status.MPI_TAG == REQS_TAG && routine < 3)){
				incLamport();
				msgsend.time = lamportClock;
				msgsend.resource = routine;
				msgsend.src = rank;
				MPI_Send(&msgsend, 1, MPI_PAKIET_T, msgrec.src, ACK_TAG, MPI_COMM_WORLD);
			}
			else if (status.MPI_TAG == REQM_TAG || status.MPI_TAG == REQK_TAG || status.MPI_TAG == REQS_TAG){
				addToQueue(msgrec.src, routine);
			}
		break;
		case 4:
			if (status.MPI_TAG == REQM_TAG || status.MPI_TAG == REQK_TAG || status.MPI_TAG == REQS_TAG){
				addToQueue(msgrec.src, msgrec.resource);
			}
		case 5:
			incLamport();
			msgsend.time = lamportClock;
			msgsend.src = rank;
			msgsend.resource = msgrec.resource;
			MPI_Send(&msgsend, 1, MPI_PAKIET_T, msgrec.src, ACK_TAG, MPI_COMM_WORLD);
	}
}

void doStuff(){
	bool collected;
	while(true){
		if(!isCollected(0)){
			collect(0);
		}
		else if (!isCollected(1)){
			routine = 2;
			collect(1);
		}
		else if (!isCollected(2)){
			routine = 3;
			collect(2);
		}
		else{
			routine = 4;
		}
		
		if (routine == 4){
			pthread_mutex_lock(&reqMut);
			acks[0] = 0;
			acks[1] = 0;
			acks[2] = 0;
			pthread_mutex_unlock(&reqMut);
			sent[0] = false;
			sent[1] = false;
			sent[2] = false;
			dance();
			
			incLamport();
			for(int i=0; i < size; i++){
				msgsend.time = lamportClock;
				msgsend.src = rank;
				if(queue1[i] == 1){
					msgsend.resource = 1;
					MPI_Send(&msgsend, 1, MPI_PAKIET_T, i, ACK_TAG, MPI_COMM_WORLD);
					queue1[i] = 0;
				}
				if(queue2[i] == 1){
					msgsend.resource = 2;
					MPI_Send(&msgsend, 1, MPI_PAKIET_T, i, ACK_TAG, MPI_COMM_WORLD);
					queue2[i] = 0;
				}
				if(queue3[i] == 1){
					msgsend.resource = 3;
					MPI_Send(&msgsend, 1, MPI_PAKIET_T, i, ACK_TAG, MPI_COMM_WORLD);
					queue3[i] = 0;
				}
			}
			routine = 5;
		}
		if (routine == 5){
			srand (time(NULL));
			int tosleep = rand() % 10 + 1;
			sleep(tosleep);
			routine = 1;
		}
	}
}

void destruction(){
	pthread_mutex_destroy( &lampMut);
	pthread_mutex_destroy( &reqMut);
	MPI_Type_free(&MPI_PAKIET_T);
	MPI_Finalize();
}