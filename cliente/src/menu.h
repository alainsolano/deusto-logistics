#ifndef MENU_H
#define MENU_H

#include "../../compartido/protocolo.h"

#ifdef _WIN32
    #include <winsock2.h>
    typedef int SOCKET_T;
#else
    typedef int SOCKET_T;
#endif

void menu_principal(SOCKET_T sock, const char* id_operario);

#endif