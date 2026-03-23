#include <iostream>
#include <cstring>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #define CLOSE_SOCKET close
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int SOCKET;
#endif

#include "../../compartido/protocolo.h"
#include "db_manager.h"

int main() {
    // Inicialización para Windows
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Error en WSAStartup" << std::endl;
            return -1;
        }
    #endif

    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // 1. Crear socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Error al crear socket" << std::endl;
        return -1;
    }

    // 2. Configurar opciones
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // 3. Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Error en bind" << std::endl;
        return -1;
    }

    // 4. Listen
    if (listen(server_fd, 5) == SOCKET_ERROR) {
        std::cerr << "Error en listen" << std::endl;
        return -1;
    }

    std::cout << "DEUSTO LOGISTICS - SERVIDOR [Puerto 8080]" << std::endl;

	db_init("datos/almacen.db");

    while(true) {
    new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket == INVALID_SOCKET) continue;

    std::cout << "\n[+] Cliente conectado." << std::endl;

    // Bucle interno: atiende todos los movimientos del mismo cliente
        while(true) {
            MovimientoStock mov;
            int bytes = recv(new_socket, (char*)&mov, sizeof(MovimientoStock), 0);

            // Si bytes <= 0 el cliente se desconectó
            if (bytes <= 0) {
                std::cout << "[-] Cliente desconectado." << std::endl;
                break;
            }

            if (bytes == sizeof(MovimientoStock)) {
                std::cout << "\n[LOG] Movimiento: " << mov.id_producto 
                        << " | Cantidad: " << mov.cantidad 
                        << " | Op: " << mov.tipo_op << std::endl;

                db_registrar_movimiento(mov, 100);
                RespuestaServidor res = {0, 100, "Operacion recibida correctamente"};
                send(new_socket, (const char*)&res, sizeof(RespuestaServidor), 0);
            }
        }

    CLOSE_SOCKET(new_socket);
    }

    CLOSE_SOCKET(server_fd);
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}
