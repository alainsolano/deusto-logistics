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
