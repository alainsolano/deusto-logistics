#ifndef MENU_H
#define MENU_H

#include "red.h"

/* Recibe el socket ya conectado al servidor, el nombre del operario
   autenticado y su rol ("admin" | "operario").
   Devuelve 0 si el operario cierra sesion normalmente,
   -1 si se perdio la conexion con el servidor. */
int menu_principal(socket_t sock, const char *id_operario, const char *rol);

#endif
