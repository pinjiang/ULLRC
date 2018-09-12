#ifndef _H
#define _H

#include <fcgio.h>

FCGX_Request req;

bool parseRequest();

std::string formResponse(const char* uri, Resource & r);

void handleRequest();

#endif
