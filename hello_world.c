//MONGOOSE-TEST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mongoose.h"
//////////////////////////////////////////////////////////////////////////////////
//handler
static int ev_handler(struct mg_connection * conn, enum mg_event ev) {
	//
	switch(ev) {
		case MG_AUTH:
			return MG_TRUE;
		case MG_REQUEST:
			mg_printf_data(conn, "Hello, World.\n");
			mg_printf_data(conn, "uri-> %s, query-> %s\n", conn->uri, conn->query_string);
			return MG_TRUE;
		default:
			return MG_FALSE;
	}
	//
	return 0;
}

///////////////////////
int main(int argc, char * argv[]) {
	struct mg_server * server;
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
}
