#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCKET close
    typedef int SOCKET;
    #define INVALID_SOCKET -1
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../../compartido/protocolo.h"

int main() {
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            printf("Error en WSAStartup\n");
            return -1;
        }
    #endif

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Error al crear el socket\n");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    // Usamos inet_addr para evitar problemas de compatibilidad en MinGW
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Error: No se pudo conectar al servidor. ¿Esta encendido?\n");
        return -1;
    }

    // Datos hardcodeados (Paso 3 de tu plan) [cite: 47]
    struct MovimientoStock prueba;
    memset(&prueba, 0, sizeof(struct MovimientoStock));
    strcpy(prueba.id_producto, "PROD-0042");
    prueba.cantidad = 50;
    prueba.tipo_op = 'E';
    prueba.timestamp = (int64_t)time(NULL);
    strcpy(prueba.id_operario, "alain_s");

    printf("Enviando movimiento: %s...\n", prueba.id_producto);
    send(sock, (const char*)&prueba, sizeof(struct MovimientoStock), 0);
    
    struct RespuestaServidor res;
    int n = recv(sock, (char*)&res, sizeof(struct RespuestaServidor), 0);

    if (n > 0) {
        printf("Servidor responde: %s (Stock: %d)\n", res.mensaje, res.stock_actual);
    }

    CLOSE_SOCKET(sock);
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}