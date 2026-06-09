#include "red.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CERRAR_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CERRAR_SOCKET close
#endif

/* Envio / recepcion completos.
   Devuelven 0 si OK, -1 si la conexion se cae.  */
static int enviar_todo(socket_t s, const void *buf, int len) {
    const char *p = (const char *)buf;
    int enviados = 0;
    while (enviados < len) {
        int n = (int)send(s, p + enviados, len - enviados, 0);
        if (n <= 0) return -1;
        enviados += n;
    }
    return 0;
}

static int recibir_todo(socket_t s, void *buf, int len) {
    char *p = (char *)buf;
    int recibidos = 0;
    while (recibidos < len) {
        int n = (int)recv(s, p + recibidos, len - recibidos, 0);
        if (n <= 0) return -1;
        recibidos += n;
    }
    return 0;
}

int red_iniciar(void) {
#ifdef _WIN32
    WSADATA wsa;
    return (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) ? 0 : -1;
#else
    return 0;
#endif
}

void red_finalizar(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

socket_t red_conectar(const char *host, int puerto) {
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCKET_INVALIDO) {
        return SOCKET_INVALIDO;
    }

    struct sockaddr_in dir;
    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_port   = htons((unsigned short)puerto);

    if (inet_pton(AF_INET, host, &dir.sin_addr) != 1) {
        CERRAR_SOCKET(s);
        return SOCKET_INVALIDO;
    }

    if (connect(s, (struct sockaddr *)&dir, sizeof(dir)) != 0) {
        CERRAR_SOCKET(s);
        return SOCKET_INVALIDO;
    }

    return s;
}

void red_cerrar(socket_t s) {
    if (s != SOCKET_INVALIDO) {
        CERRAR_SOCKET(s);
    }
}

/* Envia una cabecera con el tipo de peticion. */
static int enviar_cabecera(socket_t s, int tipo) {
    CabeceraPeticion cab;
    cab.tipo = (int32_t)tipo;
    return enviar_todo(s, &cab, sizeof(cab));
}

/* ── LOGIN ── */
int red_login(socket_t s, const char *usuario, const char *pass,
              LoginResponse *resp) {
    LoginRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.nombre_usuario, usuario, sizeof(req.nombre_usuario) - 1);
    strncpy(req.contrasena,     pass,    sizeof(req.contrasena) - 1);

    if (enviar_cabecera(s, PETICION_LOGIN) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)  return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0) return -1;
    return 0;
}

/* ── REGISTRO ── */
int red_registro(socket_t s, const char *usuario, const char *pass,
                 RegistroResponse *resp) {
    RegistroRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.nombre_usuario, usuario, sizeof(req.nombre_usuario) - 1);
    strncpy(req.contrasena,     pass,    sizeof(req.contrasena) - 1);

    if (enviar_cabecera(s, PETICION_REGISTRO) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)     return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)  return -1;
    return 0;
}

/* ── MOVIMIENTO ── */
int red_movimiento(socket_t s, const MovimientoStock *mov,
                   RespuestaServidor *resp) {
    if (enviar_cabecera(s, PETICION_MOVIMIENTO) != 0) return -1;
    if (enviar_todo(s, mov, sizeof(*mov)) != 0)       return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)    return -1;
    return 0;
}

/* ── CONSULTA ── */
int red_consulta(socket_t s, const char *id_producto,
                 ConsultaResponse *resp) {
    ConsultaRequest req;
    memset(&req, 0, sizeof(req));
    strncpy(req.id_producto, id_producto, sizeof(req.id_producto) - 1);

    if (enviar_cabecera(s, PETICION_CONSULTA) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)     return -1;
    if (recibir_todo(s, resp, sizeof(*resp)) != 0)  return -1;
    return 0;
}

/* ── HISTORIAL ── */
int red_historial(socket_t s, const char *filtro_producto,
                  HistorialItem *items, int max_items, int *n_items) {
    HistorialRequest req;
    memset(&req, 0, sizeof(req));
    if (filtro_producto != NULL) {
        strncpy(req.filtro_producto, filtro_producto,
                sizeof(req.filtro_producto) - 1);
    }

    if (enviar_cabecera(s, PETICION_HISTORIAL) != 0) return -1;
    if (enviar_todo(s, &req, sizeof(req)) != 0)      return -1;

    int32_t n = 0;
    if (recibir_todo(s, &n, sizeof(n)) != 0) return -1;

    int recibidos = 0;
    for (int32_t i = 0; i < n; i++) {
        HistorialItem item;
        if (recibir_todo(s, &item, sizeof(item)) != 0) return -1;
        if (recibidos < max_items) {
            items[recibidos++] = item;
        }
    }

    *n_items = recibidos;
    return 0;
}

/* ── RESUMEN ── */
int red_resumen(socket_t s, ResumenItem *items, int max_items, int *n_items) {
    if (enviar_cabecera(s, PETICION_RESUMEN) != 0) return -1;

    int32_t n = 0;
    if (recibir_todo(s, &n, sizeof(n)) != 0) return -1;

    int recibidos = 0;
    for (int32_t i = 0; i < n; i++) {
        ResumenItem item;
        if (recibir_todo(s, &item, sizeof(item)) != 0) return -1;
        if (recibidos < max_items) {
            items[recibidos++] = item;
        }
    }

    *n_items = recibidos;
    return 0;
}

int red_alta_producto(socket_t s, const AltaProductoRequest *req, AltaProductoResponse *resp) {
    if (enviar_cabecera(s, PETICION_ALTA_PRODUCTO) != 0) {
        return -1;
    }

    if (enviar_todo(s, req, sizeof(*req)) != 0) {
        return -1;
    }

    if (recibir_todo(s, resp, sizeof(*resp)) != 0) {
        return -1;
    }

    return 0;
}
