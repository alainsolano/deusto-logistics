#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "menu.h"
#include "usuarios.h"



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
    char nuevo_user[16];
    char nuevo_pass[16];
    char confirm_pass[16];
    int resultado;

    limpiar_pantalla();
    printf(" ╔══════════════════════════════════════════╗\n");
    printf(" ║ REGISTRO DE NUEVO OPERARIO              ║\n");
    printf(" ╠══════════════════════════════════════════╣\n");
    printf(" ╚══════════════════════════════════════════╝\n\n");

    printf(" > Nuevo usuario (max 15 chars): ");
    if (scanf("%15s", nuevo_user) != 1) {
        while (getchar() != '\n');
        return;
    }

    printf(" > Contrasena (max 15 chars): ");
    if (scanf("%15s", nuevo_pass) != 1) {
        while (getchar() != '\n');
        return;
    }

    printf(" > Confirmar contrasena: ");
    if (scanf("%15s", confirm_pass) != 1) {
        while (getchar() != '\n');
        return;
    }

    resultado = registrar_usuario(nuevo_user, nuevo_pass, confirm_pass);

    limpiar_pantalla();
    printf(" ╔══════════════════════════════════════════╗\n");
    printf(" ║ REGISTRO DE NUEVO OPERARIO              ║\n");
    printf(" ╠══════════════════════════════════════════╣\n");

    switch (resultado) {
        case 0:
            printf(" ║ [OK] Operario registrado con exito.     ║\n");
            printf(" ║ Usuario: %-29s║\n", nuevo_user);
            break;
        case -4:
            printf(" ║ [!] Las contrasenas no coinciden.       ║\n");
            break;
        case -5:
            printf(" ║ [!] Limite de operarios alcanzado.      ║\n");
            break;
        case -6:
            printf(" ║ [!] El usuario ya existe.               ║\n");
            break;
        default:
            printf(" ║ [!] Error al registrar el operario.     ║\n");
            break;
    }

    printf(" ╚══════════════════════════════════════════╝\n");
    printf("\n Presione Enter para volver...");
    getchar();
    getchar();
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