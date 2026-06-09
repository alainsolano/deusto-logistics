#include <iostream>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <vector>
#include <thread>
#include <mutex>
#include <set>
#include <string>

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

/*  SESIONES ACTIVAS  (un usuario solo puede tener una sesion a la vez)  */
static std::set<std::string> g_sesiones;
static std::mutex g_sesiones_mutex;

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
    std::string usuario_actual;  /* usuario logueado en ESTA conexion ("" = ninguno) */
    std::string rol_actual;      /* rol del usuario logueado ("admin" | "operario") */

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

                /* Control de sesion unica: rechazar si el usuario ya tiene
                 * otra sesion activa en una conexion distinta. */
                if (resp.codigo == RESP_OK) {
                    std::string nuevo = req.nombre_usuario;
                    std::lock_guard<std::mutex> lock(g_sesiones_mutex);
                    if (g_sesiones.count(nuevo) && nuevo != usuario_actual) {
                        resp.codigo = RESP_ERROR;
                        snprintf(resp.mensaje, sizeof(resp.mensaje),
                                 "Usuario ya tiene una sesion activa");
                    } else {
                        if (!usuario_actual.empty() && usuario_actual != nuevo)
                            g_sesiones.erase(usuario_actual);
                        g_sesiones.insert(nuevo);
                        usuario_actual = nuevo;
                        rol_actual = resp.rol;
                    }
                }

                log_srv("LOGIN | usuario: %s | resultado: %s | ip: %s",
                        req.nombre_usuario,
                        resp.codigo == RESP_OK ? "OK" : "FALLIDO", ip.c_str());
                if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
                break;
            }
            case PETICION_LOGOUT: {
                /* Sin cuerpo ni respuesta: solo libera la sesion. */
                std::lock_guard<std::mutex> lock(g_sesiones_mutex);
                if (!usuario_actual.empty()) {
                    g_sesiones.erase(usuario_actual);
                    log_srv("LOGOUT | usuario: %s | ip: %s",
                            usuario_actual.c_str(), ip.c_str());
                    usuario_actual.clear();
                    rol_actual.clear();
                }
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

case PETICION_ALTA_PRODUCTO: {
    AltaProductoRequest req;
    if (!recibir_todo(cli, &req, sizeof(req))) goto fin;

    AltaProductoResponse resp;
    if (rol_actual != "admin") {
        resp.codigo = RESP_ERROR;
        snprintf(resp.mensaje, sizeof(resp.mensaje),
                 "Permiso denegado: requiere rol admin");
    } else {
        db_alta_producto(req, resp);
    }

    log_srv("ALTA_PRODUCTO | producto: %s | tipo: %s | usuario: %s | resultado: %s | ip: %s",
            req.id_producto, req.tipo,
            usuario_actual.empty() ? "-" : usuario_actual.c_str(),
            resp.codigo == RESP_OK ? "OK" : "ERROR",
            ip.c_str());

    if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
    break;
}

case PETICION_BAJA_PRODUCTO: {
    BajaProductoRequest req;
    if (!recibir_todo(cli, &req, sizeof(req))) goto fin;
    OpProductoResponse resp;
    if (rol_actual != "admin") {
        resp.codigo = RESP_ERROR;
        snprintf(resp.mensaje, sizeof(resp.mensaje),
                 "Permiso denegado: requiere rol admin");
    } else {
        db_baja_producto(req, resp);
    }
    log_srv("BAJA_PRODUCTO | producto: %s | usuario: %s | resultado: %s | ip: %s",
            req.id_producto, usuario_actual.empty() ? "-" : usuario_actual.c_str(),
            resp.codigo == RESP_OK ? "OK" : "ERROR", ip.c_str());
    if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
    break;
}

case PETICION_EDITAR_PRODUCTO: {
    EditarProductoRequest req;
    if (!recibir_todo(cli, &req, sizeof(req))) goto fin;
    OpProductoResponse resp;
    if (rol_actual != "admin") {
        resp.codigo = RESP_ERROR;
        snprintf(resp.mensaje, sizeof(resp.mensaje),
                 "Permiso denegado: requiere rol admin");
    } else {
        db_editar_producto(req, resp);
    }
    log_srv("EDITAR_PRODUCTO | producto: %s | usuario: %s | resultado: %s | ip: %s",
            req.id_producto, usuario_actual.empty() ? "-" : usuario_actual.c_str(),
            resp.codigo == RESP_OK ? "OK" : "ERROR", ip.c_str());
    if (!enviar_todo(cli, &resp, sizeof(resp))) goto fin;
    break;
}

case PETICION_ALERTAS: {
    std::vector<AlertaItem> items;
    int32_t n = (int32_t)db_alertas(items);
    if (!enviar_todo(cli, &n, sizeof(n))) goto fin;
    for (const auto& it : items) {
        if (!enviar_todo(cli, &it, sizeof(it))) goto fin;
    }
    log_srv("ALERTAS | activas: %d | ip: %s", n, ip.c_str());
    break;
}

case PETICION_PROVEEDORES: {
    std::vector<ProveedorItem> items;
    int32_t n = (int32_t)db_proveedores(items);
    if (!enviar_todo(cli, &n, sizeof(n))) goto fin;
    for (const auto& it : items) {
        if (!enviar_todo(cli, &it, sizeof(it))) goto fin;
    }
    log_srv("PROVEEDORES | total: %d | ip: %s", n, ip.c_str());
    break;
}

case PETICION_PEDIDOS: {
    std::vector<PedidoItem> items;
    int32_t n = (int32_t)db_pedidos(items);
    if (!enviar_todo(cli, &n, sizeof(n))) goto fin;
    for (const auto& it : items) {
        if (!enviar_todo(cli, &it, sizeof(it))) goto fin;
    }
    log_srv("PEDIDOS | pendientes: %d | ip: %s", n, ip.c_str());
    break;
}

            default:
                log_srv("PETICION_DESCONOCIDA | tipo: %d | ip: %s",
                        cab.tipo, ip.c_str());
                goto fin;
        }
    }

fin:
    {
        std::lock_guard<std::mutex> lock(g_sesiones_mutex);
        if (!usuario_actual.empty()) g_sesiones.erase(usuario_actual);
    }
    log_srv("DESCONEXION | ip: %s | usuario: %s", ip.c_str(),
            usuario_actual.empty() ? "-" : usuario_actual.c_str());
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
    db_mostrar_costes_almacenamiento();

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
