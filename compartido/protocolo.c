#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "menu.h"

typedef struct {
    char username[16];
    char password[16];
} Usuario;

/* ── Base de datos de usuarios en memoria (max 64) ── */
#define MAX_USUARIOS 64

Usuario db_usuarios[MAX_USUARIOS] = {
    {"alain_s", "1234"},
    {"admin",   "admin"}
};
int total_usuarios = 2;

#define FICHERO_USUARIOS "datos/usuarios.dat"

void cargar_usuarios() {
    FILE *f = fopen(FICHERO_USUARIOS, "rb");
    if (f == NULL) return;
    int n = 0;
    fread(&n, sizeof(int), 1, f);
    if (n >= 1 && n <= MAX_USUARIOS) {
        total_usuarios = n;
        fread(db_usuarios, sizeof(Usuario), total_usuarios, f);
    }
    fclose(f);
}

void guardar_usuarios() {
    FILE *f = fopen(FICHERO_USUARIOS, "wb");
    if (f == NULL) return;
    fwrite(&total_usuarios, sizeof(int), 1, f);
    fwrite(db_usuarios, sizeof(Usuario), total_usuarios, f);
    fclose(f);
}

void limpiar_pantalla() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void ventana_bienvenida() {
    limpiar_pantalla();
    printf("  ╔══════════════════════════════════════════════════════╗\n");
    printf("  ║                DEUSTO LOGISTICS S.L.                 ║\n");
    printf("  ║          Sistema de Gestion de Inventario            ║\n");
    printf("  ╠══════════════════════════════════════════════════════╣\n");
    printf("  ║                                                      ║\n");
    printf("  ║   [1] Iniciar Sesion                                 ║\n");
    printf("  ║   [2] Registrar Nuevo Operario                       ║\n");
    printf("  ║   [3] Acerca de                                      ║\n");
    printf("  ║   [0] Salir del Programa                             ║\n");
    printf("  ║                                                      ║\n");
    printf("  ╚══════════════════════════════════════════════════════╝\n");
    printf("  Seleccione su opcion: ");
}

int ventana_login(char* usuario_logueado) {
    char user[16], pass[16];
    int intentos = 3;
    while (intentos > 0) {
        limpiar_pantalla();
        printf("  ╔══════════════════════════════════════════╗\n");
        printf("  ║           ACCESO RESTRINGIDO             ║\n");
        printf("  ╚══════════════════════════════════════════╝\n\n");
        printf("  Intentos restantes: %d\n", intentos);
        printf("  > Usuario: ");
        scanf("%15s", user);
        printf("  > Contrasena: ");
        scanf("%15s", pass);
        for (int i = 0; i < total_usuarios; i++) {
            if (strcmp(user, db_usuarios[i].username) == 0 &&
                strcmp(pass, db_usuarios[i].password) == 0) {
                strcpy(usuario_logueado, user);
                return 1;
            }
        }
        intentos--;
        printf("\n  [!] Credenciales incorrectas. Enter para reintentar...");
        getchar(); getchar();
    }
    return 0;
}

void ventana_registro() {
    char nuevo_user[16], nuevo_pass[16], confirm_pass[16];
    limpiar_pantalla();
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║       REGISTRO DE NUEVO OPERARIO         ║\n");
    printf("  ╠══════════════════════════════════════════╣\n");
    if (total_usuarios >= MAX_USUARIOS) {
        printf("  ║  [!] Limite de operarios alcanzado.      ║\n");
        printf("  ╚══════════════════════════════════════════╝\n");
        printf("\n  Presione Enter para volver...");
        getchar(); getchar();
        return;
    }
    printf("  ╚══════════════════════════════════════════╝\n\n");
    printf("  > Nuevo usuario (max 15 chars): ");
    if (scanf("%15s", nuevo_user) != 1) { while(getchar()!='\n'); return; }
    for (int i = 0; i < total_usuarios; i++) {
        if (strcmp(nuevo_user, db_usuarios[i].username) == 0) {
            printf("\n  [!] El usuario '%s' ya existe.\n", nuevo_user);
            printf("  Presione Enter para volver...");
            getchar(); getchar();
            return;
        }
    }
    printf("  > Contrasena (max 15 chars): ");
    if (scanf("%15s", nuevo_pass) != 1) { while(getchar()!='\n'); return; }
    printf("  > Confirmar contrasena: ");
    if (scanf("%15s", confirm_pass) != 1) { while(getchar()!='\n'); return; }
    if (strcmp(nuevo_pass, confirm_pass) != 0) {
        limpiar_pantalla();
        printf("  ╔══════════════════════════════════════════╗\n");
        printf("  ║       REGISTRO DE NUEVO OPERARIO         ║\n");
        printf("  ╠══════════════════════════════════════════╣\n");
        printf("  ║  [!] Las contrasenas no coinciden.       ║\n");
        printf("  ║      Registro cancelado.                 ║\n");
        printf("  ╚══════════════════════════════════════════╝\n");
        printf("\n  Presione Enter para volver...");
        getchar(); getchar();
        return;
    }
    strncpy(db_usuarios[total_usuarios].username, nuevo_user, 15);
    strncpy(db_usuarios[total_usuarios].password, nuevo_pass, 15);
    total_usuarios++;
    guardar_usuarios();
    limpiar_pantalla();
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║       REGISTRO DE NUEVO OPERARIO         ║\n");
    printf("  ╠══════════════════════════════════════════╣\n");
    printf("  ║                                          ║\n");
    printf("  ║  [OK] Operario registrado con exito.     ║\n");
    printf("  ║                                          ║\n");
    printf("  ║  Usuario: %-29s║\n", nuevo_user);
    printf("  ║  Total operarios: %-21d║\n", total_usuarios);
    printf("  ║                                          ║\n");
    printf("  ╚══════════════════════════════════════════╝\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
}

void ventana_acerca_de() {
    limpiar_pantalla();
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║       DEUSTO LOGISTICS TERMINAL          ║\n");
    printf("  ╚══════════════════════════════════════════╝\n");
    printf("  \nAutor: Grupo 6\n");
    printf("\n  Presione Enter para volver...");
    getchar(); getchar();
}

int main() {
    cargar_usuarios();
    int opcion;
    char user_act[16];
    while (1) {
        ventana_bienvenida();
        if (scanf("%d", &opcion) != 1) {
            while(getchar() != '\n');
            continue;
        }
        switch(opcion) {
            case 1:
                if (ventana_login(user_act)) menu_principal(0, user_act);
                break;
            case 2: ventana_registro(); break;
            case 3: ventana_acerca_de(); break;
            case 0: return 0;
        }
    }
    return 0;
}