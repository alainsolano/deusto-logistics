#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>

/* =========================================================
 * protocolo.h  —  Estructuras binarias compartidas
 * Cliente (C)  <-->  Servidor (C++)
 * Comunicación por sockets TCP/IP con tamaño fijo
 * ========================================================= */

#define PUERTO_SERVIDOR  8080
#define MAX_CLIENTES     10

/* --- Códigos de respuesta del servidor --- */
#define RESP_OK          0
#define RESP_ERROR       1
#define RESP_STOCK_CRITICO 2

/* --- Tipos de operación sobre stock --- */
#define OP_ENTRADA       'E'
#define OP_SALIDA        'S'
#define OP_CONSULTA      'C'

/* --- Tipos de petición al servidor --- */
#define PETICION_LOGIN   1
#define PETICION_STOCK   2

/* =========================================================
 * LoginRequest — enviado por el cliente al iniciar sesión
 * ========================================================= */
typedef struct {
    char nombre_usuario[32];
    char hash_contrasena[65];  /* SHA-256 hex string (64 chars + '\0') */
} LoginRequest;

/* =========================================================
 * LoginResponse — respuesta del servidor al login
 * ========================================================= */
typedef struct {
    int  codigo;               /* RESP_OK | RESP_ERROR */
    char rol[16];              /* "operario" | "admin" */
    char nombre_completo[64];
    char mensaje[64];
} LoginResponse;

/* =========================================================
 * MovimientoStock — operación de stock enviada por el cliente
 * ========================================================= */
typedef struct {
    char  id_producto[16];
    int   cantidad;
    char  tipo_op;             /* 'E' | 'S' | 'C' */
    long  timestamp;
    char  id_operario[32];
} MovimientoStock;

/* =========================================================
 * RespuestaServidor — respuesta del servidor a una operación
 * ========================================================= */
typedef struct {
    int  codigo;               /* RESP_OK | RESP_ERROR | RESP_STOCK_CRITICO */
    int  stock_actual;
    char mensaje[64];
} RespuestaServidor;

/* =========================================================
 * CabeceraPeticion — primer byte enviado para identificar tipo
 * ========================================================= */
typedef struct {
    int tipo;                  /* PETICION_LOGIN | PETICION_STOCK */
} CabeceraPeticion;

#endif /* PROTOCOLO_H */