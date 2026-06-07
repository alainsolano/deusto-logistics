#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "menu.h"
#include "usuario.h"
#include "config.h"
#include "logger.h"
#include "red.h"

void limpiar_pantalla(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void ventana_bienvenida(void) {
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

/* Devuelve: 1 = login OK | 0 = credenciales agotadas | -1 = se perdio el servidor */
int ventana_login(socket_t sock, char *usuario_logueado) {
    char user[32], pass[64];
    int intentos = 3;

    while (intentos > 0) {
        limpiar_pantalla();
        printf("  ╔══════════════════════════════════════════╗\n");
        printf("  ║           ACCESO RESTRINGIDO             ║\n");
        printf("  ╚══════════════════════════════════════════╝\n\n");
        printf("  Intentos restantes: %d\n", intentos);
        printf("  > Usuario: ");
        if (scanf("%31s", user) != 1) { while (getchar() != '\n'); return 0; }
        while (getchar() != '\n');  /* vaciar \n del buffer */
        printf("  > Contrasena: ");
        leer_contrasena(pass, sizeof(pass));

        LoginResponse resp;
        memset(&resp, 0, sizeof(resp));
        if (red_login(sock, user, pass, &resp) != 0) {
            log_error_conexion("Se perdio la conexion durante el login");
            return -1;
        }

        log_login(user, resp.codigo == RESP_OK);

        if (resp.codigo == RESP_OK) {
            strncpy(usuario_logueado, user, 31);
            usuario_logueado[31] = '\0';
            printf("\n  [OK] Bienvenido, %s (%s).\n",
                   resp.nombre_completo[0] ? resp.nombre_completo : user,
                   resp.rol);
            printf("  Presione Enter para continuar...");
            getchar();
            return 1;
        }

        intentos--;
        printf("\n  [!] %s\n", resp.mensaje[0] ? resp.mensaje : "Credenciales incorrectas.");
        printf("  Presione Enter para reintentar...");
        getchar();
    }

    return 0;
}

/* Devuelve 0 si OK, -1 si se perdio el servidor */
int ventana_registro(socket_t sock) {
    char nuevo_user[32];
    char nuevo_pass[64];
    char confirm_pass[64];

    limpiar_pantalla();
    printf(" ╔══════════════════════════════════════════╗\n");
    printf(" ║       REGISTRO DE NUEVO OPERARIO         ║\n");
    printf(" ╚══════════════════════════════════════════╝\n\n");

    printf(" > Nuevo usuario (max 31 chars): ");
    if (scanf("%31s", nuevo_user) != 1) {
        while (getchar() != '\n');
        return 0;
    }
    while (getchar() != '\n');  /* vaciar \n del buffer */

    printf(" > Contrasena: ");
    leer_contrasena(nuevo_pass, sizeof(nuevo_pass));

    printf(" > Confirmar contrasena: ");
    leer_contrasena(confirm_pass, sizeof(confirm_pass));

    if (strcmp(nuevo_pass, confirm_pass) != 0) {
        printf("\n [!] Las contrasenas no coinciden. Registro cancelado.\n");
        printf("\n Presione Enter para volver...");
        getchar();
        return 0;
    }

    RegistroResponse resp;
    memset(&resp, 0, sizeof(resp));
    if (red_registro(sock, nuevo_user, nuevo_pass, &resp) != 0) {
        log_error_conexion("Se perdio la conexion durante el registro");
        return -1;
    }

    {
        char log_msg[160];
        snprintf(log_msg, sizeof(log_msg),
            "REGISTRO | usuario: %s | resultado: %s",
            nuevo_user, resp.codigo == RESP_OK ? "OK" : "FALLIDO");
        escribir_log(log_msg);
    }

    limpiar_pantalla();
    printf(" ╔══════════════════════════════════════════╗\n");
    printf(" ║       REGISTRO DE NUEVO OPERARIO         ║\n");
    printf(" ╠══════════════════════════════════════════╣\n");
    if (resp.codigo == RESP_OK)
        printf(" ║ [OK] %-36s║\n", resp.mensaje[0] ? resp.mensaje : "Operario registrado.");
    else
        printf(" ║ [!] %-37s║\n", resp.mensaje[0] ? resp.mensaje : "Error al registrar.");
    printf(" ╚══════════════════════════════════════════╝\n");
    printf("\n Presione Enter para volver...");
    getchar();
    return 0;
}

void ventana_acerca_de(void) {
    limpiar_pantalla();
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║       DEUSTO LOGISTICS TERMINAL          ║\n");
    printf("  ╚══════════════════════════════════════════╝\n");
    printf("  \nAutor: Grupo 6\n");
    printf("  Arquitectura: cliente (C) <--> servidor (C++) por TCP/IP\n");
    printf("\n  Presione Enter para volver...");
    getchar();
}

int main(void) {
    int opcion;
    char user_act[32];

    if (cargar_config("datos/config.txt") != 0) {
        printf("Aviso: no se pudo leer datos/config.txt, se usan valores por defecto.\n");
    }

    /* El cliente DEPENDE del servidor: sin conexion no se puede operar. */
    if (red_iniciar() != 0) {
        printf("Error: no se pudo inicializar la pila de red.\n");
        return 1;
    }

    printf("Conectando con el servidor %s:%d ...\n",
           g_config.host_servidor, g_config.puerto_servidor);

    socket_t sock = red_conectar(g_config.host_servidor, g_config.puerto_servidor);
    if (sock == SOCKET_INVALIDO) {
        printf("\n  [X] No se puede conectar con el servidor en %s:%d.\n",
               g_config.host_servidor, g_config.puerto_servidor);
        printf("      Arranque el servidor e intentelo de nuevo.\n\n");
        log_error_conexion("No se pudo conectar con el servidor al iniciar");
        red_finalizar();
        return 1;
    }

    while (1) {
        ventana_bienvenida();

        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n');
            continue;
        }

        int rc = 0;
        switch (opcion) {
            case 1: {
                int r = ventana_login(sock, user_act);
                if (r == 1) {
                    rc = menu_principal(sock, user_act);
                } else if (r == -1) {
                    rc = -1;
                }
                break;
            }
            case 2:
                rc = ventana_registro(sock);
                break;
            case 3:
                ventana_acerca_de();
                break;
            case 0:
                red_cerrar(sock);
                red_finalizar();
                return 0;
            default:
                printf("\n[!] Opcion no valida. Presione Enter para continuar...");
                getchar(); getchar();
                break;
        }

        if (rc == -1) {
            printf("\n  [X] Se perdio la conexion con el servidor. El cliente se cerrara.\n");
            red_cerrar(sock);
            red_finalizar();
            return 1;
        }
    }

    red_cerrar(sock);
    red_finalizar();
    return 0;
}
