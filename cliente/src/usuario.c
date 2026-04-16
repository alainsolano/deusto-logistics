#include "usuario.h"
#include <stdio.h>
#include <string.h>
#include "config.h"

Usuario db_usuarios[MAX_USUARIOS] = {
    {"alain_s", "1234"},
    {"admin", "admin"}
};

int total_usuarios = 2;

void cargar_usuarios(void) {
    FILE *f = fopen(g_config.ruta_usuarios, "rb");
    int n = 0;

    if (f == NULL) {
        return;
    }

    if (fread(&n, sizeof(int), 1, f) != 1) {
        fclose(f);
        return;
    }

    if (n >= 1 && n <= MAX_USUARIOS) {
        total_usuarios = n;
        fread(db_usuarios, sizeof(Usuario), total_usuarios, f);
    }

    fclose(f);
}

void guardar_usuarios(void) {
    FILE *f = fopen(g_config.ruta_usuarios, "wb");

    if (f == NULL) {
        return;
    }

    fwrite(&total_usuarios, sizeof(int), 1, f);
    fwrite(db_usuarios, sizeof(Usuario), total_usuarios, f);
    fclose(f);
}

int registrar_usuario(const char *nuevo_user, const char *nuevo_pass, const char *confirm_pass) {
    int i;

    if (nuevo_user == NULL || nuevo_pass == NULL || confirm_pass == NULL) {
        return -1;
    }

    if (strlen(nuevo_user) == 0 || strlen(nuevo_pass) == 0) {
        return -2;
    }

    if (strlen(nuevo_user) > 15 || strlen(nuevo_pass) > 15) {
        return -3;
    }

    if (strcmp(nuevo_pass, confirm_pass) != 0) {
        return -4;
    }

    if (total_usuarios >= MAX_USUARIOS) {
        return -5;
    }

    for (i = 0; i < total_usuarios; i++) {
        if (strcmp(nuevo_user, db_usuarios[i].username) == 0) {
            return -6;
        }
    }

    strncpy(db_usuarios[total_usuarios].username, nuevo_user, 15);
    db_usuarios[total_usuarios].username[15] = '\0';

    strncpy(db_usuarios[total_usuarios].password, nuevo_pass, 15);
    db_usuarios[total_usuarios].password[15] = '\0';

    total_usuarios++;
    guardar_usuarios();

    return 0;
}