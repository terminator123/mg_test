//DICT
#ifndef _DICT_H
#define _DICT_H
//////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
//
#define KEY_MAX_LEN 64
#define VALUE_MAX_LEN 256
//
struct kvbox {
	char key[KEY_MAX_LEN + 1];
	char value[VALUE_MAX_LEN + 1];
	struct kvbox * next;
};
//
struct dictbox {
	size_t box_size;
	struct kvbox * head;
	//
	pthread_mutex_t mutex;
};

#endif
