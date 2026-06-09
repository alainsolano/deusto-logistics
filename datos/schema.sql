PRAGMA foreign_keys = ON;

-- BORRAR TABLAs
DROP TABLE IF EXISTS PEDIDOS_REPOSICION;
DROP TABLE IF EXISTS ALERTAS_STOCK;
DROP TABLE IF EXISTS PRODUCTOS_INFLAMABLES;
DROP TABLE IF EXISTS PRODUCTOS_FRAGILES;
DROP TABLE IF EXISTS PRODUCTOS_PERECEDEROS;
DROP TABLE IF EXISTS HISTORIAL_MOVIMIENTOS;
DROP TABLE IF EXISTS PRODUCTOS;
DROP TABLE IF EXISTS PROVEEDORES;
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
-- TABLA PROVEEDORES
-- =========================
CREATE TABLE PROVEEDORES (
    id_proveedor INTEGER PRIMARY KEY AUTOINCREMENT,
    nombre TEXT NOT NULL,
    contacto TEXT,
    telefono TEXT,
    activo INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0,1))
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
    ubicacion_almacen TEXT,
    activo INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0,1)),
    id_proveedor INTEGER REFERENCES PROVEEDORES(id_proveedor)
);

-- ========================
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
-- TABLA PEDIDOS_REPOSICION
--   Pedidos generados (automatica o manualmente) para reponer stock
-- =========================
CREATE TABLE PEDIDOS_REPOSICION (
    id_pedido INTEGER PRIMARY KEY AUTOINCREMENT,
    id_producto TEXT NOT NULL,
    id_proveedor INTEGER,
    cantidad INTEGER NOT NULL CHECK (cantidad > 0),
    fecha TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    estado TEXT NOT NULL DEFAULT 'pendiente' CHECK (estado IN ('pendiente','recibido','cancelado')),
    FOREIGN KEY (id_producto) REFERENCES PRODUCTOS(id_producto),
    FOREIGN KEY (id_proveedor) REFERENCES PROVEEDORES(id_proveedor)
);

-- =========================
-- DATOS DE PRUEBA
-- =========================

-- Usuarios (id_usuario: 1=alain_s, 2=admin, 3=maria_o, 4=user)
INSERT INTO USUARIOS (nombre_usuario, hash_contrasena, rol)
VALUES
('alain_s', '1234', 'operario'),
('admin', 'admin', 'admin'),
('maria_o', 'maria', 'operario'),
('user', 'user', 'operario');

-- Proveedores (id_proveedor: 1, 2, 3)
INSERT INTO PROVEEDORES (nombre, contacto, telefono)
VALUES
('Suministros Bilbao S.L.', 'ventas@sumbilbao.es', '944111222'),
('Iberica de Almacenaje', 'pedidos@iberalmacen.es', '915333444'),
('FrioNorte Distribucion', 'comercial@frionorte.es', '946555666');

-- Productos: mezcla de tipos y estados de stock
--   NORMAL: stock >= 2*minimo | BAJO: minimo<=stock<2*minimo | CRITICO: 0<stock<minimo | SIN STOCK: 0
INSERT INTO PRODUCTOS (
    id_producto, nombre, tipo, cantidad_actual, stock_minimo, precio_unitario, estrategia_salida, ubicacion_almacen, activo, id_proveedor
) VALUES
-- Genericos
('PROD-0042', 'Tornillos M6 (caja 100)', 'generico',   120, 20, 3.50, 'FIFO', 'A-01', 1, 1),  -- NORMAL
('PROD-0044', 'Cable HDMI 2m',           'generico',     0,  5, 6.90, 'FIFO', 'A-02', 1, 1),  -- SIN STOCK
('PROD-0053', 'Guantes nitrilo (caja)',  'generico',    18, 40, 8.20, 'FIFO', 'A-03', 1, 1),  -- CRITICO
('PROD-0099', 'Producto descatalogado',  'generico',     5,  5, 2.00, 'FIFO', 'A-09', 0, 1),  -- DADO DE BAJA
-- Fragiles
('PROD-0043', 'Vajilla ceramica',        'fragil',        8, 10, 15.00,'FIFO', 'B-02', 1, 2),  -- CRITICO
('PROD-0047', 'Bombillas LED',           'fragil',      200, 30, 4.10, 'FIFO', 'B-03', 1, 1),  -- NORMAL
-- Inflamables
('PROD-0045', 'Disolvente industrial',   'inflamable',   15,  8, 30.00,'LIFO', 'Z-01', 1, 2),  -- BAJO
('PROD-0046', 'Pintura en spray',        'inflamable',    6, 10, 12.50,'LIFO', 'Z-02', 1, 2),  -- CRITICO
-- Perecederos
('PROD-0050', 'Yogur natural',           'perecedero',   40,  6, 0.55, 'FIFO', 'C-03', 1, 3),  -- NORMAL, caducidad lejana
('PROD-0051', 'Leche fresca',            'perecedero',   25, 10, 0.95, 'FIFO', 'C-04', 1, 3),  -- caduca pronto
('PROD-0052', 'Queso curado',            'perecedero',   12,  5, 9.80, 'FIFO', 'C-05', 1, 3);  -- CADUCADO

INSERT INTO PRODUCTOS_FRAGILES (id_producto, coste_embalaje, instrucciones)
VALUES
('PROD-0043', 1.50, 'Manipular con cuidado. No apilar mas de 3 cajas'),
('PROD-0047', 0.80, 'Embalaje individual con espuma');

INSERT INTO PRODUCTOS_INFLAMABLES (id_producto, nivel_riesgo, zona_almacenamiento)
VALUES
('PROD-0045', 3, 'Z-INFLAMABLE-01'),
('PROD-0046', 2, 'Z-INFLAMABLE-02');

-- Caducidades: lejana (ok), proxima (<30 dias), pasada (caducado)
INSERT INTO PRODUCTOS_PERECEDEROS (id_producto, fecha_caducidad, temperatura_max)
VALUES
('PROD-0050', '2026-12-31', 8.0),
('PROD-0051', '2026-06-20', 4.0),
('PROD-0052', '2026-05-15', 6.0);

-- Historial de movimientos (timestamps explicitos para probar filtros por fecha/operario)
INSERT INTO HISTORIAL_MOVIMIENTOS (id_producto, id_usuario, tipo_operacion, cantidad, stock_resultante, timestamp, origen)
VALUES
('PROD-0042', 1, 'entrada', 100, 100, '2026-05-02 09:15:00', 'manual'),
('PROD-0042', 3, 'salida',   30,  70, '2026-05-10 11:40:00', 'manual'),
('PROD-0042', 1, 'entrada',  50, 120, '2026-05-21 08:05:00', 'manual'),
('PROD-0043', 2, 'entrada',  20,  20, '2026-05-03 10:00:00', 'manual'),
('PROD-0043', 3, 'salida',   12,   8, '2026-06-01 16:20:00', 'manual'),
('PROD-0050', 1, 'entrada',  60,  60, '2026-05-15 12:00:00', 'manual'),
('PROD-0050', 3, 'salida',   20,  40, '2026-06-04 13:30:00', 'manual'),
('PROD-0046', 2, 'salida',    4,   6, '2026-06-05 17:45:00', 'manual'),
('PROD-0099', 1, 'entrada',   5,   5, '2026-04-20 09:00:00', 'manual');

-- Pedidos de reposicion en distintos estados (ciclo completo).
-- Ademas, el servidor genera nuevos 'pendiente' automaticamente al bajar del minimo.
INSERT INTO PEDIDOS_REPOSICION (id_producto, id_proveedor, cantidad, fecha, estado)
VALUES
('PROD-0044', 1, 10, '2026-06-06 09:00:00', 'pendiente'),
('PROD-0046', 2, 14, '2026-06-05 18:00:00', 'pendiente'),
('PROD-0043', 2, 20, '2026-05-28 10:30:00', 'recibido'),
('PROD-0053', 1, 60, '2026-05-30 12:00:00', 'cancelado');