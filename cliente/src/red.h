#ifndef RED_H
#define RED_H

#include "../../compartido/protocolo.h"

/* Tipo de socket portable */
#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET socket_t;
    #define SOCKET_INVALIDO  INVALID_SOCKET
#else
    typedef int socket_t;
    #define SOCKET_INVALIDO  (-1)
#endif

/* Inicializa la pila de red. Devuelve 0 si OK. */
int  red_iniciar(void);

/* Libera la pila de red. */
void red_finalizar(void);

/* Conecta con el servidor. Devuelve el socket o SOCKET_INVALIDO si falla. */
socket_t red_conectar(const char *host, int puerto);

/* Cierra una conexion. */
void red_cerrar(socket_t s);

/*    Operaciones (todas devuelven 0 si la comunicacion fue correcta,
 *    -1 si fallo el envio/recepcion = se perdio el servidor). 
 *    El resultado logico de la operacion va en el campo 'codigo' de la
 *    respuesta.  */

int red_login    (socket_t s, const char *usuario, const char *pass,
                  LoginResponse *resp);

int red_registro (socket_t s, const char *usuario, const char *pass,
                  RegistroResponse *resp);

int red_movimiento(socket_t s, const MovimientoStock *mov,
                   RespuestaServidor *resp);

int red_consulta (socket_t s, const char *id_producto,
                  ConsultaResponse *resp);

/* Historial: rellena 'items' (hasta max_items) y devuelve el numero de
 * elementos recibidos en *n_items. Retorno 0 si OK, -1 si error de red. */
int red_historial(socket_t s, const char *filtro_producto,
                  HistorialItem *items, int max_items, int *n_items);

/* Resumen: igual que historial pero sin filtro. */
int red_resumen  (socket_t s, ResumenItem *items, int max_items, int *n_items);

#endif
