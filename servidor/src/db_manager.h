#pragma once

#include "../../compartido/protocolo.h"
#include <vector>

/* Abre / cierra la base de datos. */
bool db_init(const char* ruta);
void db_close();

void db_login     (const LoginRequest& req,    LoginResponse& resp);
void db_registro  (const RegistroRequest& req, RegistroResponse& resp);
void db_movimiento(const MovimientoStock& mov, RespuestaServidor& resp);
void db_consulta  (const ConsultaRequest& req, ConsultaResponse& resp);

int  db_historial (const HistorialRequest& req, std::vector<HistorialItem>& out);
int  db_resumen   (std::vector<ResumenItem>& out);
void db_mostrar_costes_almacenamiento();
void db_alta_producto(const AltaProductoRequest& req,
                      AltaProductoResponse& resp);

void db_baja_producto  (const BajaProductoRequest& req,   OpProductoResponse& resp);
void db_editar_producto(const EditarProductoRequest& req, OpProductoResponse& resp);

int  db_alertas    (std::vector<AlertaItem>& out);
int  db_proveedores(std::vector<ProveedorItem>& out);
int  db_pedidos    (std::vector<PedidoItem>& out);
