#ifndef ULLVS_HTTPD_H
#define ULLVS_HTTPD_H

void server_callback (SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query, SoupClientContext *context, gpointer data);

#endif
