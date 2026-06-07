#include "producto.h"
#include <iomanip>
#include <stdexcept>

/* =========================================================
 * producto.cpp  —  Implementacion de la clase base Producto
 * ========================================================= */

/* ---------------------------------------------------------
 * Constructor
 * --------------------------------------------------------- */
Producto::Producto(const std::string &id,
                   const std::string &nombre,
                   const std::string &tipo,
                   int stock_actual,
                   int stock_minimo,
                   double precio_unitario,
                   const std::string &estrategia_salida,
                   const std::string &ubicacion)
{

    /* Validaciones basicas antes de asignar */
    if (precio_unitario <= 0)
    {
        throw std::invalid_argument("El precio debe ser mayor que 0");
    }

    if (stock_actual < 0)
    {
        throw std::invalid_argument("El stock no puede ser negativo");
    }

    if (stock_minimo < 0)
    {
        throw std::invalid_argument("El stock minimo no puede ser negativo");
    }

    /* Asignacion de atributos */
    this->id = id;
    this->nombre = nombre;
    this->tipo = tipo;
    this->stock_actual = stock_actual;
    this->stock_minimo = stock_minimo;
    this->precio_unitario = precio_unitario;
    this->estrategia_salida = estrategia_salida;
    this->ubicacion = ubicacion;
}

/* ---------------------------------------------------------
 * calcularEstado
 *
 * Clasifica el producto en cuatro estados segun la relacion
 * entre el stock actual y el stock minimo configurado.
 * Logica definida en la seccion 5.3 de la memoria.
 * --------------------------------------------------------- */
std::string Producto::calcularEstado() const
{

    if (stock_actual <= 0)
    {
        return "SIN STOCK";
    }

    if (stock_actual < stock_minimo)
    {
        return "CRITICO";
    }

    if (stock_actual < stock_minimo * 2)
    {
        return "BAJO";
    }

    return "NORMAL";
}

/* ---------------------------------------------------------
 * Getters
 * --------------------------------------------------------- */

const std::string &Producto::getId() const { return id; }
const std::string &Producto::getNombre() const { return nombre; }
const std::string &Producto::getTipo() const { return tipo; }
int Producto::getStockActual() const { return stock_actual; }
int Producto::getStockMinimo() const { return stock_minimo; }
double Producto::getPrecioUnitario() const { return precio_unitario; }
const std::string &Producto::getEstrategia() const { return estrategia_salida; }
const std::string &Producto::getUbicacion() const { return ubicacion; }

/* ---------------------------------------------------------
 * operator<<
 *
 * Imprime la ficha completa del producto.
 *
 * Ejemplo de salida:
 *   [FRAGIL] PROD-0043 | Producto B
 *     Stock     : 80 uds  (min: 10)  Estado: NORMAL
 *     Precio    : 15.00 EUR
 *     Coste alm.: 1320.00 EUR
 *     Estrategia: FIFO  Ubicacion: B-02
 * --------------------------------------------------------- */
std::ostream &operator<<(std::ostream &os, const Producto &p)
{

    os << std::fixed << std::setprecision(2);

    os << p.getTipoEtiqueta() << " " << p.id << " | " << p.nombre << "\n";

    os << "  Stock     : " << p.stock_actual << " uds"
       << "  (min: " << p.stock_minimo << ")"
       << "  Estado: " << p.calcularEstado() << "\n";

    os << "  Precio    : " << p.precio_unitario << " EUR\n";

    os << "  Coste alm.: " << p.calcularCosteAlmacenamiento() << " EUR\n";

    os << "  Estrategia: " << p.estrategia_salida
       << "  Ubicacion: " << p.ubicacion << "\n";

    return os;
}

/* ---------------------------------------------------------
 * operator==
 *
 * Dos productos son iguales si tienen el mismo id_producto.
 * --------------------------------------------------------- */
bool Producto::operator==(const Producto &otro) const
{
    return this->id == otro.id;
}

/* ---------------------------------------------------------
 * operator>
 *
 * Un producto es "mayor" que otro si tiene mas stock actual.
 * Util para ordenar listas de productos por disponibilidad.
 * --------------------------------------------------------- */
bool Producto::operator>(const Producto &otro) const
{
    return this->stock_actual > otro.stock_actual;
}

/* ---------------------------------------------------------
 * operator<
 * --------------------------------------------------------- */
bool Producto::operator<(const Producto &otro) const
{
    return this->stock_actual < otro.stock_actual;
}