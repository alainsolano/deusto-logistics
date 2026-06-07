#ifndef USUARIOS_H
#define USUARIOS_H

#ifdef _WIN32
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

void leer_contrasena(char *buf, int max_len);

#endif
