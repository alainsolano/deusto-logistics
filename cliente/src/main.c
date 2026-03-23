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
#include "menu.h"
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

    char id_operario[16] = "alain_s";
    menu_principal(sock, id_operario);

    CLOSE_SOCKET(sock);
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}