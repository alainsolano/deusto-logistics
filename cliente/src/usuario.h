#ifndef USUARIOS_H
#define USUARIOS_H

typedef struct {
    char username[16];
    char password[16];
} Usuario;

#define MAX_USUARIOS 64

extern Usuario db_usuarios[MAX_USUARIOS];
extern int total_usuarios;

void cargar_usuarios(void);
void guardar_usuarios(void);
int registrar_usuario(const char *nuevo_user, const char *nuevo_pass, const char *confirm_pass);

#endif