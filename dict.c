#include "dict.h"
#include "mongoose.h"
//dict_init
struct dictbox * dict_init() {
	struct dictbox * dbox = 0;
	//
	if((dbox = (struct dictbox *)malloc(sizeof(struct dictbox))) == NULL) {
		fprintf(stderr, "ERROR: %s %d %s: malloc error\n", __FILE__, __LINE__, __FUNCTION__);
		exit(1);
	}
	//
	dbox->box_size = 0;
	dbox->head = 0;
	//
	pthread_mutex_init(&(dbox->mutex), NULL);
	//
	return dbox;
}

//dict_free
void dict_free(struct dictbox * dbox) {
	struct kvbox * box = 0;
	if(dbox) {
		while(dbox->head) {
			box = dbox->head;
			dbox->head = box->next;
			free(box);
		}
		//
		pthread_mutex_destroy(&(dbox->mutex));
		//
		free(dbox);
	}
}

//dict_seek
struct kvbox * dict_seek(struct dictbox * dbox, char * key) {
	struct kvbox * box = dbox->head;
	while(box) {
		if(strcmp(box->key, key) == 0) {
			break;
		}
		box = box->next;
	}
	//
	return box;
}

//dict_add
struct kvbox * dict_add(struct dictbox * dbox, char * key, char * value) {
	struct kvbox * box = 0;
	//
	if(strlen(key) > KEY_MAX_LEN || strlen(value) > VALUE_MAX_LEN) {
		fprintf(stderr, "WARN: %s %d %s: %s %s: data is too long\n", __FILE__, __LINE__, __FUNCTION__, key, value);
		return 0;
	}
	//
	box = dict_seek(dbox, key);
	if(box) {
		strcpy(box->value, value);
	} else {
		if((box = (struct kvbox *)malloc(sizeof(struct kvbox))) == NULL) {
			fprintf(stderr, "ERROR: %s %d %s: malloc error\n", __FILE__, __LINE__, __FUNCTION__);
			exit(1);
		}
		strcpy(box->key, key);
		strcpy(box->value, value);
		//
		box->next = dbox->head;
		dbox->head = box;
		//
		dbox->box_size += 1;
	}
	//
	return box;
}

//dict_del
int dict_del(struct dictbox * dbox, char * key) {
	struct kvbox * box = 0;
	struct kvbox * par = 0;
	//
	box = dbox->head;
	//
	while(box) {
		if(strcmp(box->key, key) == 0) {
			break;
		}
		par = box;
		box = box->next;
	}
	//
	if(box) {
		if(par == 0) {
			dbox->head = box->next;
		} else {
			par->next = box->next;
		}
		free(box);
		dbox->box_size -= 1;
	}
	//
	return 0;
}

//dict_view
int dict_view(struct dictbox * dbox) {
	struct kvbox * box = dbox->head;
	size_t i = 0;
	//
	printf("%zu %p\n", dbox->box_size, dbox->head);
	while(box) {
		printf("%zu\t%s\t%s\n", i ++, box->key, box->value);
		box = box->next;
	}
	//
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//dict_load
int dict_load(struct dictbox * dbox, char * dict_file) {
	FILE * fp = 0;
	char buf[8192] = "";
	char * key = 0;
	char * value = 0;
	///////////////
	if((fp = fopen(dict_file, "r")) == NULL) {
		fprintf(stderr, "ERROR: %s %d %s: %s: fail to open file\n", __FILE__, __LINE__, __FUNCTION__, dict_file);
		exit(1);
	}
	//
	while(fgets(buf, sizeof(buf), fp)) {
		while(*(buf + strlen(buf) - 1) == '\r' || *(buf + strlen(buf) - 1) == '\n') {
			*(buf + strlen(buf) - 1) = '\0';
			if(strlen(buf) < 1) {
				break;
			}
		}
		//
		if(strlen(buf) < 1) {
			continue;
		}
		//
		key = buf;
		value = strchr(buf, '\t');
		if(value == NULL) {
			fprintf(stderr, "WARN: %s %d %s: %s -> %s: value is empty\n", __FILE__, __LINE__, __FUNCTION__, buf, key);
			continue;
		}
		//
		*value = '\0';
		value += 1;
		//
		dict_add(dbox, key, value);
	}
	//
	fclose(fp);
	//
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
static struct dictbox * dbox = 0;
//handler
static int ev_handler(struct mg_connection * conn, enum mg_event ev) {
	struct kvbox * box = 0;
	char key[KEY_MAX_LEN + 1];
	char value[VALUE_MAX_LEN + 1];
	//
	switch(ev) {
		case MG_AUTH:
			return MG_TRUE;
		case MG_REQUEST:
			mg_printf_data(conn, "URI -> %s, QUERY -> %s\n", conn->uri, conn->query_string);
			if(strcmp(conn->uri, "/add.do") == 0) {
				mg_get_var(conn, "key", key, sizeof(key));
				mg_get_var(conn, "value", value, sizeof(value));
				mg_printf_data(conn, "ADD: %s -> %s, %zu\n", key, value, dbox->box_size);
				//
				pthread_mutex_lock(&(dbox->mutex));
				box = dict_add(dbox, key, value);
				pthread_mutex_unlock(&(dbox->mutex));
				//
				if(box) {
					mg_printf_data(conn, "%zu ADD OK\n", dbox->box_size);
				} else {
					mg_printf_data(conn, "%zu ADD FAIL\n", dbox->box_size);
				}
			} else if(strcmp(conn->uri, "/del.do") == 0) {
				mg_get_var(conn, "key", key, sizeof(key));
				//
				pthread_mutex_lock(&(dbox->mutex));
				dict_del(dbox, key);
				pthread_mutex_unlock(&(dbox->mutex));
				//
				mg_printf_data(conn, "DEL %zu %s\n", dbox->box_size, key);
			} else if(strcmp(conn->uri, "/seek.do") == 0) {
				mg_get_var(conn, "key", key, sizeof(key));
				//
				box = dict_seek(dbox, key);
				if(box) {
					mg_printf_data(conn, "SEEK: %s -> %s\n", key, box->value);
				} else {
					mg_printf_data(conn, "SEEK: %s ->\n", key);
				}
			}
			return MG_TRUE;
		default:
			return MG_FALSE;
	}
	//
	return 0;
}

///////////////////////
//test <test.dict>
int main(int argc, char * argv[]) {
	struct mg_server * server;
	//
	if(argc < 2) {
		printf("demo: %s <test.dict>\n", argv[0]);
		exit(0);
	}
	//
	dbox = dict_init();
	dict_load(dbox, argv[1]);
	//
	server = mg_create_server(NULL, ev_handler);
	//
	mg_set_option(server, "listening_port", "8089");
	//
	for(;;) {
		mg_poll_server(server, 1000);
	}
	//
	mg_destroy_server(&server);
	//
	dict_free(dbox);
	//
	return 0;
}

/*
//test <test.dict>
int main(int argc, char * argv[]) {
	struct dictbox * dbox = 0;
	char * dict_file = 0;
	char key[KEY_MAX_LEN] = "";
	char value[VALUE_MAX_LEN] = "";
	int i = 0;
	//
	if(argc < 2) {
		printf("demo: %s <test.dict>\n", argv[0]);
		exit(0);
	}
	//
	dict_file = argv[1];
	//
	dbox = dict_init();
	dict_load(dbox, dict_file);
	//
	for(i = 0; i < 100; i ++) {
		sprintf(key, "%06d", i);
		sprintf(value, "%012d", i * i * i);
		dict_add(dbox, key, value);
	}
	//
	for(i = 0; i < 50; i ++) {
		sprintf(key, "%06d", i * 3);
		dict_del(dbox, key);
	}
	//
	for(i = 90; i < 100; i ++) {
		sprintf(key, "%06d", i);
		dict_del(dbox, key);
	}
	//
	for(i = 0; i < 20; i ++) {
		sprintf(key, "%06d", i);
		sprintf(value, "%018d", i * i);
		dict_add(dbox, key, value);
	}
	//
	dict_load(dbox, dict_file);
	//
	dict_view(dbox);
	//
	dict_free(dbox);
	//
	return 0;
}
*/
