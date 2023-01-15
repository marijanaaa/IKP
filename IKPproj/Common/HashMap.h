#pragma once

#ifndef HASHMAP_H
#define HASHMAP_H

#include <iostream>
#include <cstdlib>
#include <Windows.h>
#define INITIAL_MAP_SIZE 500
using namespace std;

template <class T>
class HashMapNode
{
private:
	char* key;
	T value;
	HashMapNode* next;
	template <class T> friend class HashMap;

public:
	HashMapNode(char* key, T value);		//ctor
	HashMapNode();							//ctor
	~HashMapNode();							//DTOR

};

//Hash map that maps char* strings to generic values
template <class T>
class HashMap
{
private:
	HashMapNode<T>* nodes;					//Nodes array
	CRITICAL_SECTION MapCS;				//Mutex used for map access
	unsigned int size;						//Hash map maximum unique key spots without chained nodes
	unsigned int countUnique;				//Current number of unique key spots in use
	long long GetHash(const char* str);		//Hash function for string hashing

public:
	HashMap(unsigned int size = INITIAL_MAP_SIZE);		//CTOR
	~HashMap();											//DTOR
	void Insert(const char* key, T value);					//Method for inserting a value in hash map, replaces existing one with the same key
	void Delete(const char* key);								//Method for deleting a value in hash map, returns false if not found
	bool Get(const char* key, T* value);						//Method for getting value by key, value is stored in value pointer, returns false if key doesn't exist
	bool DoesKeyExist(const char* key);						//Checks if key exists in hash map
	unsigned int Size();
	char** GetKeys(int* keyCount);							//Returns all keys from hash map, stores number of keys in keyCount, returns NULL if empty
};

template <class T>
HashMapNode<T>::HashMapNode(char* key, T value)
{
	this->key = (char*)malloc((strlen(key) + 3) * sizeof(char));
	strcpy_s(this->key, strlen(key) + 1, key);
	this->value = value;
	this->next = nullptr;
}

template <class T>
HashMapNode<T>::HashMapNode()
{
	this->key = nullptr;
	this->next = nullptr;
}

template <class T>
HashMapNode<T>::~HashMapNode()
{
	if (this->key != nullptr)
		free(this->key);
	if (this->next != nullptr)
		delete (this->next);
}


// Constructor to initialize map
template <class T>
HashMap<T>::HashMap(unsigned int size)
{
	InitializeCriticalSection(&MapCS);
	nodes = new HashMapNode<T>[size];
	for (int i = 0; i < (int)size; i++)
	{
		nodes[i].next = nullptr;
		nodes[i].key = nullptr;
	}

	this->size = size;
	countUnique = 0;
}

// Destructor
template <class T>
HashMap<T>::~HashMap()
{
	delete[] nodes;
	DeleteCriticalSection(&MapCS);
}


//Function for string hashing, polynomial rolling hash
template <class T>
long long HashMap<T>::GetHash(const char* key)
{
	// P and M
	int p = 53; // english alphabet upper and lowercase
	int m = (int)(1e9 + 9); //very large number to prevent collision
	long long powerOfP = 1;
	long long hashVal = 0;
	for (int i = 0; i < (int)strlen(key); i++)
	{
		hashVal = (hashVal + (key[i] - 'a' + 1) * powerOfP) % m;
		powerOfP = (powerOfP * p) % m;
	}
	return hashVal;
}

// Function for getting hash map size
template <class T>
unsigned int HashMap<T>::Size()
{
	return size;
}

// Function for getting keys from hash map
template <class T>
char** HashMap<T>::GetKeys(int* keysCount)
{
	EnterCriticalSection(&MapCS);
	char** keys = (char**)malloc(size * sizeof(char*));
	int keysFound = 0;

	for (int i = 0; i < (int)size; i++)
	{
		if (nodes[i].key == nullptr)continue;
		keys[keysFound] = (char*)malloc((strlen(nodes[i].key) + 3) * sizeof(char));
		strcpy_s(keys[keysFound], strlen(nodes[i].key) + 1, nodes[i].key);
		keysFound++;
		HashMapNode<T>* node = (nodes + i)->next;
		while (node != nullptr)
		{
			keys[keysFound] = (char*)malloc((strlen(node->key) + 3) * sizeof(char));
			strcpy_s(keys[keysFound], strlen(node->key) + 1, node->key);
			keysFound++;

			node = node->next;
		}
	}

	if (keysFound == 0)
	{
		if (keys != NULL)
			free(keys);
		*keysCount = 0;
		LeaveCriticalSection(&MapCS);
		return NULL;
	}
	else
	{
		*keysCount = keysFound;
		LeaveCriticalSection(&MapCS);
		return keys;
	}
}


//Function for checking if key exists in hash map
template <class T>
bool HashMap<T>::DoesKeyExist(const char* key)
{
	unsigned long long hashValue = GetHash(key);
	EnterCriticalSection(&MapCS);
	if (countUnique == 0)
	{
		LeaveCriticalSection(&MapCS);
		return false;
	}

	unsigned int index = hashValue % this->size;
	if (nodes[index].key == nullptr)
	{
		LeaveCriticalSection(&MapCS);
		return false;
	}
	else
	{
		HashMapNode<T>* node = nodes + index;
		do
		{
			if (strcmp(node->key, key) == 0)
			{
				LeaveCriticalSection(&MapCS);
				return true;
			}

			node = node->next;
		} while (node != nullptr);
	}

	LeaveCriticalSection(&MapCS);
	return false;

}

//Function for inserting into hash map, if value exists it will be replaced
template <class T>
void HashMap<T>::Insert(const char* key, T value)
{
	unsigned long long hashValue = GetHash(key);

	EnterCriticalSection(&MapCS);
	int index = hashValue % this->size;
	if (nodes[index].key == nullptr) //If primary node is empty, place value and key there
	{
		nodes[index].key = (char*)malloc((strlen(key) + 3) * sizeof(char));
		strcpy_s(nodes[index].key, strlen(key) + 1, key);
		nodes[index].value = value;
		countUnique++;
		LeaveCriticalSection(&MapCS);

		return;
	}
	else if (strcmp(nodes[index].key, key) == 0)//Rewrite primary node value
	{
		nodes[index].value = value;
		LeaveCriticalSection(&MapCS);
		return;
	}
	else //Collission happened, chain new element
	{
		HashMapNode<T>* node = nodes + index;
		HashMapNode<T>* nodeNext = node->next;
		while (true)
		{
			if (nodeNext != nullptr)
			{
				if (strcmp(nodeNext->key, key) == 0) //Replace existing
				{
					nodeNext->value = value;
					LeaveCriticalSection(&MapCS);
					return;
				}

			}
			else
			{
				break;
			}
			node = node->next;
			nodeNext = nodeNext->next;
		}

		//Add new
		{
			char* keyCopy = _strdup(key);
			HashMapNode<T>* newNode = new HashMapNode<T>(keyCopy, value);
			node->next = newNode;
			countUnique++;
			free(keyCopy);
		}
	}
	LeaveCriticalSection(&MapCS);
	return;
}


//Function for getting value from hash map
template <class T>
bool HashMap<T>::Get(const char* key, T* value)
{
	unsigned  long long hashValue = GetHash(key);

	EnterCriticalSection(&MapCS);
	if (countUnique == 0)
	{
		LeaveCriticalSection(&MapCS);
		return false;
	}

	unsigned int index = hashValue % this->size;
	if (nodes[index].key == nullptr)
	{
		LeaveCriticalSection(&MapCS);
		return false;
	}
	else
	{
		HashMapNode<T>* node = nodes + index;
		do
		{
			if (strcmp(node->key, key) == 0)
			{
				*value = node->value;
				LeaveCriticalSection(&MapCS);
				return true;
			}

			node = node->next;
		} while (node != nullptr);
	}

	LeaveCriticalSection(&MapCS);
	return false;
}


//Function to delete element from hash map
template <class T>
void HashMap<T>::Delete(const char* key)
{
	unsigned long long hashValue = GetHash(key);
	EnterCriticalSection(&MapCS);
	if (countUnique == 0)
	{
		LeaveCriticalSection(&MapCS);
		return;
	}

	unsigned int index = hashValue % this->size;
	if (nodes[index].key == nullptr)
	{
		LeaveCriticalSection(&MapCS);
		return;
	}

	HashMapNode<T>* node = nodes + index;
	if (strcmp(node->key, key) == 0 && node->next == nullptr) //if first element is to be deleted and there are no chained collisions just set it to null
	{
		free(node->key);
		node->key = nullptr;
		countUnique--;
		LeaveCriticalSection(&MapCS);
		return;
	}
	else if (strcmp(node->key, key) == 0 && node->next != nullptr)//if first element is to be deleted and there are chained collisions move last to it's spot
	{
		HashMapNode<T>* nodeNext = node->next;
		while (nodeNext->next != nullptr)
		{
			node = node->next;
			nodeNext = nodeNext->next;
		}

		node->next = nullptr; //Set previous to nullptr
		HashMapNode<T>* first = nodes + index;
		first->value = nodeNext->value; //Copy last's value to first
		free(first->key);
		first->key = (char*)malloc((strlen(nodeNext->key) + 3) * sizeof(char));
		strcpy_s(first->key, strlen(nodeNext->key) + 1, nodeNext->key);//Store last's key in first's

		nodeNext->next = nullptr;
		free(nodeNext->key);
		nodeNext->key = nullptr;
		delete nodeNext; //delete last
		countUnique--;
		LeaveCriticalSection(&MapCS);
		return;
	}

	HashMapNode<T>* nodeNext = node->next;
	while (nodeNext != nullptr)
	{
		if (strcmp(nodeNext->key, key) == 0)
		{
			node->next = nodeNext->next;
			nodeNext->next = nullptr;
			free(nodeNext->key);
			nodeNext->key = nullptr;
			delete nodeNext;
			countUnique--;
			LeaveCriticalSection(&MapCS);
			return;
		}

		node = node->next;
		nodeNext = nodeNext->next;
	}

	LeaveCriticalSection(&MapCS);
	return;
}

#endif