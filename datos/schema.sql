PRAGMA foreign_keys = ON;

-- BORRAR TABLAS
DROP TABLE IF EXISTS ALERTAS_STOCK;
DROP TABLE IF EXISTS PRODUCTOS_INFLAMABLES;
DROP TABLE IF EXISTS PRODUCTOS_FRAGILES;
DROP TABLE IF EXISTS PRODUCTOS_PERECEDEROS;
DROP TABLE IF EXISTS HISTORIAL_MOVIMIENTOS;
DROP TABLE IF EXISTS PRODUCTOS;
DROP TABLE IF EXISTS USUARIOS;

-- =========================
-- TABLA USUARIOS
-- =========================
CREATE TABLE USUARIOS (
    id_usuario INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre_usuario TEXT NOT NULL UNIQUE,
    hash_contrasena TEXT NOT NULL,
    rol TEXT NOT NULL CHECK (rol IN ('operario','admin')),
    activo INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0,1)),
    fecha_alta TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- =========================
-- TABLA PRODUCTOS
-- =========================
CREATE TABLE PRODUCTOS (
    id_producto TEXT PRIMARY KEY NOT NULL,
    nombre TEXT NOT NULL,
    tipo TEXT NOT NULL CHECK (tipo IN ('generico','perecedero','fragil','inflamable')),
    cantidad_actual INTEGER NOT NULL DEFAULT 0 CHECK (cantidad_actual >= 0),
    stock_minimo INTEGER NOT NULL DEFAULT 5,
    precio_unitario REAL NOT NULL CHECK (precio_unitario > 0),
    estrategia_salida TEXT NOT NULL DEFAULT 'FIFO' CHECK (estrategia_salida IN ('FIFO','LIFO')),
    ubicacion_almacen TEXT
);

-- =========================
-- TABLA HISTORIAL_MOVIMIENTOS
-- =========================
CREATE TABLE HISTORIAL_MOVIMIENTOS (
    id_movimiento INTEGER PRIMARY KEY AUTOINCREMENT,
    id_producto TEXT NOT NULL,
    id_usuario INTEGER NOT NULL,
    tipo_operacion TEXT NOT NULL CHECK (tipo_operacion IN ('entrada','salida')),
    cantidad INTEGER NOT NULL CHECK (cantidad > 0),
    stock_resultante INTEGER NOT NULL,
    timestamp TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    origen TEXT CHECK (origen IN ('manual','archivo')),
    FOREIGN KEY (id_producto) REFERENCES PRODUCTOS(id_producto),
    FOREIGN KEY (id_usuario) REFERENCES USUARIOS(id_usuario)
);

-- =========================
-- TABLA PRODUCTOS_PERECEDEROS
-- =========================
CREATE TABLE PRODUCTOS_PERECEDEROS (
    id_producto TEXT PRIMARY KEY NOT NULL,
    fecha_caducidad TEXT NOT NULL,
    temperatura_max REAL,
    FOREIGN KEY (id_producto) REFERENCES PRODUCTOS(id_producto)
);

-- =========================
-- TABLA PRODUCTOS_FRAGILES
-- =========================
CREATE TABLE PRODUCTOS_FRAGILES (
    id_producto TEXT PRIMARY KEY NOT NULL,
    coste_embalaje REAL NOT NULL CHECK (coste_embalaje >= 0),
    instrucciones TEXT,
    FOREIGN KEY (id_producto) REFERENCES PRODUCTOS(id_producto)
);

-- =========================
-- TABLA PRODUCTOS_INFLAMABLES
-- =========================
CREATE TABLE PRODUCTOS_INFLAMABLES (
    id_producto TEXT PRIMARY KEY NOT NULL,
    nivel_riesgo INTEGER NOT NULL CHECK (nivel_riesgo BETWEEN 1 AND 3),
    zona_almacenamiento TEXT NOT NULL,
    FOREIGN KEY (id_producto) REFERENCES PRODUCTOS(id_producto)
);

-- =========================
-- TABLA ALERTAS_STOCK
-- =========================
CREATE TABLE ALERTAS_STOCK (
    id_alerta INTEGER PRIMARY KEY AUTOINCREMENT,
    id_producto TEXT NOT NULL,
    stock_en_alerta INTEGER NOT NULL,
    timestamp_alerta TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    resuelta INTEGER NOT NULL DEFAULT 0 CHECK (resuelta IN (0,1)),
    FOREIGN KEY (id_producto) REFERENCES PRODUCTOS(id_producto)
);

-- =========================
-- DATOS DE PRUEBA
-- =========================

INSERT INTO USUARIOS (nombre_usuario, hash_contrasena, rol)
VALUES
('alain_s', '1234', 'operario'),
('admin', 'admin', 'admin');

INSERT INTO PRODUCTOS (
    id_producto, nombre, tipo, cantidad_actual, stock_minimo, precio_unitario, estrategia_salida, ubicacion_almacen
) VALUES
('PROD-0042', 'Producto A', 'generico', 100, 5, 10.0, 'FIFO', 'A-01'),
('PROD-0043', 'Producto B', 'fragil', 80, 10, 15.0, 'FIFO', 'B-02'),
('PROD-0045', 'Producto C', 'inflamable', 20, 8, 30.0, 'LIFO', 'Z-01'),
('PROD-0050', 'Producto D', 'perecedero', 40, 6, 5.5, 'FIFO', 'C-03');

INSERT INTO PRODUCTOS_FRAGILES (id_producto, coste_embalaje, instrucciones)
VALUES ('PROD-0043', 1.5, 'Manipular con cuidado');

INSERT INTO PRODUCTOS_INFLAMABLES (id_producto, nivel_riesgo, zona_almacenamiento)
VALUES ('PROD-0045', 3, 'Z-INFLAMABLE-01');

INSERT INTO PRODUCTOS_PERECEDEROS (id_producto, fecha_caducidad, temperatura_max)
VALUES ('PROD-0050', '2026-12-31', 8.0);