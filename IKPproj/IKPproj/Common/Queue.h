#pragma once

#ifndef QUEUE_H
#define QUEUE_H


#include <iostream>
#include <cstdlib>
#include <Windows.h>
using namespace std;

// Class for queue
template <class T>
class Queue
{
private:
	CRITICAL_SECTION QueueCS;	  //Queue mutex
	T* elements;					// array to store queue elements
	int capacity;				// maximum capacity of the queue
	int front;					// front points to front element in the queue (if any)
	int rear;					// rear points to last element in the queue
	int count;					// current size of the queue
public:
	int GetCapacity();
	Queue(int size = QUEUE_SIZE);        //CTOR
	~Queue();							//DTOR
	bool Dequeue();						//Remove element from the queue front, returns false if queue is empty
	bool DequeueGet(T* value);			//Remove element from the queue front and places it as return value in passed pointer. Returns false if queue is empty
	bool Enqueue(T value);				//Add value to the queue rear, returns false if queue is full
	bool GetFront(T* value);			//Get element from the front of the queue, returns true if queue is not empty
	int Size();							//Get queue size
	bool isEmpty();						//Check if empty
	bool isFull();						//Check if full
	void Clear();
};

// Constructor to initialize queue
template <class T>
Queue<T>::Queue(int size)
{
	InitializeCriticalSection(&QueueCS);
	elements = new T[size];
	capacity = size;
	front = 0;
	rear = -1;
	count = 0;
}

//Destructor to destroy queue
template <class T>
Queue<T>::~Queue()
{
	DeleteCriticalSection(&QueueCS);
	delete[] elements;
}

// Clear removes all elements from queue
template <class T>
void Queue<T>::Clear()
{
	DeleteCriticalSection(&QueueCS);
	delete[] elements;

	InitializeCriticalSection(&QueueCS);
	elements = new T[capacity];
	front = 0;
	rear = -1;
	count = 0;
}

// GetCapacity returns queue capacity
template <class T>
int Queue<T>::GetCapacity() {
	return capacity;
}


// Utility function to remove front element from the queue
template <class T>
bool Queue<T>::Dequeue()
{
	EnterCriticalSection(&QueueCS);
	if (isEmpty())
	{
		LeaveCriticalSection(&QueueCS);
		return false;
	}

	front = (front + 1) % capacity;
	count--;
	LeaveCriticalSection(&QueueCS);
	return true;
}

// Utility function to remove front element from the queue
template <class T>
bool Queue<T>::DequeueGet(T* value)
{
	EnterCriticalSection(&QueueCS);
	if (isEmpty())
	{
		LeaveCriticalSection(&QueueCS);
		return false;
	}
	*value = elements[front];
	front = (front + 1) % capacity;
	count--;
	LeaveCriticalSection(&QueueCS);
	return true;
}


// Utility function to add an item to the queue
template <class T>
bool Queue<T>::Enqueue(T value)
{
	EnterCriticalSection(&QueueCS);
	if (isFull())
	{
		
		LeaveCriticalSection(&QueueCS);
		return false;
	}

	rear = (rear + 1) % capacity;
	elements[rear] = value;
	count++;
	LeaveCriticalSection(&QueueCS);
	return true;
}

// Utility function to return front element in the queue
template <class T>
bool Queue<T>::GetFront(T* value)
{
	EnterCriticalSection(&QueueCS);
	if (isEmpty())
	{
		LeaveCriticalSection(&QueueCS);
		return false;
	}
	*value = elements[front];
	LeaveCriticalSection(&QueueCS);
	return true;
}

// Utility function to return the size of the queue
template <class T>
int Queue<T>::Size()
{
	return count;
}

// Utility function to check if the queue is empty 
template <class T>
bool Queue<T>::isEmpty()
{
	return (Size() == 0);
}

// Utility function to check if the queue is full
template <class T>
bool Queue<T>::isFull()
{
	return (Size() == capacity);
}

#endif // !QUEUE_H
