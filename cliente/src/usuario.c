#include "usuario.h"
#include <stdio.h>

void leer_contrasena(char *buf, int max_len) {
    int i = 0;

#ifdef _WIN32
    int c;
    while (i < max_len - 1) {
        c = _getch();
        if (c == '\r' || c == '\n') break;
        if (c == '\b') {
            if (i > 0) { i--; printf("\b \b"); }
        } else {
            buf[i++] = (char)c;
            printf("*");
        }
    }
    printf("\n");
#else
    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_lflag &= ~(ECHO | ICANON);
    new_t.c_cc[VMIN]  = 1;
    new_t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

    int c;
    while (i < max_len - 1) {
        c = getchar();
        if (c == '\n' || c == EOF) break;
        if (c == 127 || c == '\b') {   /* backspace */
            if (i > 0) { i--; printf("\b \b"); fflush(stdout); }
        } else {
            buf[i++] = (char)c;
            printf("*"); fflush(stdout);
        }
    }
    printf("\n");
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
#endif

    buf[i] = '\0';
}
