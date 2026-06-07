#include <iostream>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <vector>
#include <thread>
#include <mutex>

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

/*  LOG DEL SERVIDOR  */
static FILE* g_logfile = nullptr;
static std::mutex g_log_mutex;

static void log_srv(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_log_mutex);

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);

    va_list args;

    printf("[%s] ", ts);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);

    if (g_logfile) {
        fprintf(g_logfile, "[%s] ", ts);
        va_start(args, fmt);
        vfprintf(g_logfile, fmt, args);
        va_end(args);
        fprintf(g_logfile, "\n");
        fflush(g_logfile);
    }
}

/* ENVIO / RECEPCION */
static bool enviar_todo(SOCKET s, const void* buf, int len) {
    const char* p = static_cast<const char*>(buf);
    int enviados = 0;
    while (enviados < len) {
        int n = (int)send(s, p + enviados, len - enviados, 0);
        if (n <= 0) return false;
        enviados += n;
    }
    return true;
}

static bool recibir_todo(SOCKET s, void* buf, int len) {
    char* p = static_cast<char*>(buf);
    int recibidos = 0;
    while (recibidos < len) {
        int n = (int)recv(s, p + recibidos, len - recibidos, 0);
        if (n <= 0) return false;
        recibidos += n;
    }
    return true;
}

/*  ATENCION A UN CLIENTE (un hilo por conexion) */
static void atender_cliente(SOCKET cli, std::string ip) {
    while (true) {
        CabeceraPeticion cab;
        if (!recibir_todo(cli, &cab, sizeof(cab))) {
            break;  /* cliente desconectado */
        }

        switch (cab.tipo) {
            case PETICION_LOGIN: {
                LoginRequest req;
                if (!recibir_todo(cli, &req, sizeof(req))) goto fin;
                LoginResponse resp;
                db_login(req, resp);
                log_srv("LOGIN | usuario: %s | resultado: %s | ip: %s",
                        req.nombre_usuario,
                        resp.codigo == RESP_OK ? "OK" : "FALLIDO", ip.c_str());
                if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
                break;
            }
            case PETICION_REGISTRO: {
                RegistroRequest req;
                if (!recibir_todo(cli, &req, sizeof(req))) goto fin;
                RegistroResponse resp;
                db_registro(req, resp);
                log_srv("REGISTRO | usuario: %s | resultado: %s | ip: %s",
                        req.nombre_usuario,
                        resp.codigo == RESP_OK ? "OK" : "FALLIDO", ip.c_str());
                if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
                break;
            }
            case PETICION_MOVIMIENTO: {
                MovimientoStock mov;
                if (!recibir_todo(cli, &mov, sizeof(mov))) goto fin;
                RespuestaServidor resp;
                db_movimiento(mov, resp);
                log_srv("%s | operario: %s | producto: %s | cantidad: %d | "
                        "stock: %d | resultado: %s | ip: %s",
                        mov.tipo_op == OP_ENTRADA ? "ENTRADA" : "SALIDA",
                        mov.id_operario, mov.id_producto, mov.cantidad,
                        resp.stock_actual,
                        resp.codigo == RESP_OK ? "OK" : "RECHAZADO", ip.c_str());
                if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
                break;
            }
            case PETICION_CONSULTA: {
                ConsultaRequest req;
                if (!recibir_todo(cli, &req, sizeof(req))) goto fin;
                ConsultaResponse resp;
                db_consulta(req, resp);
                log_srv("CONSULTA | producto: %s | stock: %d | ip: %s",
                        req.id_producto, resp.stock_actual, ip.c_str());
                if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
                break;
            }
            case PETICION_HISTORIAL: {
                HistorialRequest req;
                if (!recibir_todo(cli, &req, sizeof(req))) goto fin;
                std::vector<HistorialItem> items;
                int32_t n = (int32_t)db_historial(req, items);
                if (!enviar_todo(cli, &n, sizeof(n))) goto fin;
                for (const auto& it : items) {
                    if (!enviar_todo(cli, &it, sizeof(it))) goto fin;
                }
                log_srv("HISTORIAL | filtro: %s | registros: %d | ip: %s",
                        req.filtro_producto[0] ? req.filtro_producto : "(todos)",
                        n, ip.c_str());
                break;
            }
            case PETICION_RESUMEN: {
                std::vector<ResumenItem> items;
                int32_t n = (int32_t)db_resumen(items);
                if (!enviar_todo(cli, &n, sizeof(n))) goto fin;
                for (const auto& it : items) {
                    if (!enviar_todo(cli, &it, sizeof(it))) goto fin;
                }
                log_srv("RESUMEN | productos: %d | ip: %s", n, ip.c_str());
                break;
            }
            default:
                log_srv("PETICION_DESCONOCIDA | tipo: %d | ip: %s",
                        cab.tipo, ip.c_str());
                goto fin;
        }
    }

fin:
    log_srv("DESCONEXION | ip: %s", ip.c_str());
    CLOSE_SOCKET(cli);
}

/* ══════════════════════════════════════════════ */
int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error en WSAStartup" << std::endl;
        return -1;
    }
#endif

    g_logfile = fopen("logs/servidor.log", "a");
    if (!g_logfile) {
        system("mkdir -p logs");
        g_logfile = fopen("logs/servidor.log", "a");
    }

    if (!db_init("datos/deusto_logistics.db")) {
        std::cerr << "No se pudo abrir la base de datos. Abortando." << std::endl;
        return -1;
    }

    SOCKET server_fd;
    struct sockaddr_in address;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Error al crear socket" << std::endl;
        return -1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    memset(&address, 0, sizeof(address));
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(PUERTO_SERVIDOR);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Error en bind (puerto " << PUERTO_SERVIDOR << " ocupado?)" << std::endl;
        return -1;
    }

    if (listen(server_fd, MAX_CLIENTES) == SOCKET_ERROR) {
        std::cerr << "Error en listen" << std::endl;
        return -1;
    }

    log_srv("INICIO | DEUSTO LOGISTICS SERVIDOR [Puerto %d]", PUERTO_SERVIDOR);

    while (true) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        SOCKET cli = accept(server_fd, (struct sockaddr*)&cli_addr, &cli_len);
        if (cli == INVALID_SOCKET) continue;

        char ip_cliente[INET_ADDRSTRLEN] = "desconocida";
        inet_ntop(AF_INET, &cli_addr.sin_addr, ip_cliente, sizeof(ip_cliente));

        log_srv("CONEXION | ip: %s | puerto: %d",
                ip_cliente, ntohs(cli_addr.sin_port));

        std::thread(atender_cliente, cli, std::string(ip_cliente)).detach();
    }

    CLOSE_SOCKET(server_fd);
    db_close();
    if (g_logfile) fclose(g_logfile);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
