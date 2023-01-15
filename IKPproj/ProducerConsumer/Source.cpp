#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

#include "../Common/Queue.h"

#define SAFE_DELETE_HANDLE(a) if(a){CloseHandle(a);}

template <class T>
class ProducerConsumer
{
private:
	HANDLE hEmptyCounter;
	HANDLE hFullCounter;
	HANDLE* finishSignal;
	Queue<T> Data;
public:
	ProducerConsumer(HANDLE *f, int size);        //CTOR
	~ProducerConsumer();							//DTOR
	int Produce(T input);						//Remove element from the queue front, returns false if queue is empty
	int Consume(T* res);			//Remove element from the queue front and places it as return value in passed pointer. Returns false if queue is empty
	void Clear();
};

// Constructor to initialize queue
template <class T>
ProducerConsumer<T>::ProducerConsumer(HANDLE *f, int size):Data(size)
{
	finishSignal = f;
	hEmptyCounter = CreateSemaphore(0, size, size, NULL);
	hFullCounter = CreateSemaphore(0, 0, size, NULL);
}

// Constructor to initialize queue
template <class T>
ProducerConsumer<T>::~ProducerConsumer()
{
	SAFE_DELETE_HANDLE(hEmptyCounter);
	SAFE_DELETE_HANDLE(hFullCounter);
}

template <class T>
void ProducerConsumer<T>::Clear() {
	SAFE_DELETE_HANDLE(hEmptyCounter);
	SAFE_DELETE_HANDLE(hFullCounter);
	hEmptyCounter = CreateSemaphore(0, Data.GetCapacity(), Data.GetCapacity(), NULL);
	hFullCounter = CreateSemaphore(0, 0, Data.GetCapacity(), NULL);

	Data.Clear();
}
template <class T>
int ProducerConsumer<T>::Produce(T input) {
	const int semaphoreNum = 2;
	HANDLE semaphores[semaphoreNum] = { *finishSignal, hEmptyCounter };

	int result = WaitForMultipleObjects(semaphoreNum, semaphores, false, 10);
	if (result == WAIT_OBJECT_0 + 1) {
		Data.Enqueue(input);
		ReleaseSemaphore(hFullCounter, 1, NULL);
		return 0;
	}
	else if (result == WAIT_OBJECT_0) {
		return -1;
	}

	return 1;
}

template <class T>
int ProducerConsumer<T>::Consume(T *res) {
	const int semaphoreNum = 2;
	HANDLE semaphores[semaphoreNum] = { *finishSignal, hFullCounter };

	int result = WaitForMultipleObjects(semaphoreNum, semaphores, false, 10);
	if (result == WAIT_OBJECT_0+1) {
		Data.DequeueGet(res);
		ReleaseSemaphore(hEmptyCounter, 1, NULL);
		return 0;
	}
	else if (result == WAIT_OBJECT_0) {
		return -1;
	}

	return 1;
}
