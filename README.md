# Deusto Logistics

Sistema de control de stock de almacén con arquitectura **cliente-servidor**.

- **Servidor** (C++): único dueño de la base de datos SQLite. Atiende a varios
  clientes a la vez por TCP/IP.
- **Cliente** (C): terminal interactiva. **Depende del servidor**: si el servidor
  no está arrancado, el cliente no funciona.

Toda operación (login, registro, entradas/salidas, consultas, historial y resumen)
viaja por sockets; el cliente nunca accede a la base de datos directamente.

## Requisitos

Entorno **Linux / WSL Ubuntu** (sockets POSIX). Instala el toolchain y SQLite:

```bash
sudo apt update
sudo apt install build-essential libsqlite3-dev sqlite3
```

## 1. Compilar

Desde la raíz del proyecto:

```bash
make all        # compila cliente y servidor
# make cliente  # solo el cliente
# make servidor # solo el servidor
# make clean    # borra binarios y objetos
```

Genera los binarios `servidor/servidor` y `cliente/cliente`.

## 2. Preparar la base de datos (solo la primera vez)

El repositorio ya incluye `datos/deusto_logistics.db` con datos de prueba.
Si quieres regenerarla desde cero:

```bash
sqlite3 datos/deusto_logistics.db < datos/schema.sql
```

## 3. Arrancar

**Primero el servidor** (déjalo corriendo en una terminal):

```bash
./servidor/servidor
```

Queda escuchando en el puerto `8080` y escribe en `logs/servidor.log`.

**Después el cliente**, en otra terminal:

```bash
./cliente/cliente
```

Si el servidor no está arrancado, el cliente avisará y se cerrará.

## Usuarios de prueba

| Usuario   | Contraseña | Rol      |
|-----------|------------|----------|
| `admin`   | `admin`    | admin    |
| `user`    | `user`     | operario |

También puedes crear operarios nuevos desde la opción **"Registrar Nuevo
Operario"** del menú inicial (se guardan en la base de datos del servidor).

## Configuración del cliente

`datos/config.txt` (usar saltos de línea LF):

```
RUTA_LOG=logs/log.txt
HOST_SERVIDOR=127.0.0.1
PUERTO_SERVIDOR=8080
```

Para conectar a un servidor en otra máquina, cambia `HOST_SERVIDOR` por su IP.

## Estructura

```
cliente/src/    Cliente C (main, menu, red, config, usuario, logger)
servidor/src/   Servidor C++ (main, db_manager)
compartido/     protocolo.h — estructuras binarias del protocolo TCP
datos/          base de datos, esquema y configuración
logs/           logs del cliente y del servidor
```
