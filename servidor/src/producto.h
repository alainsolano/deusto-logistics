#pragma once

#include <string>
#include <iostream>

/* =========================================================
 * producto.h  —  Clase base abstracta Producto
 *
 * Define la interfaz comun que deben implementar todas las
 * subclases de producto: generico, fragil, inflamable y
 * perecedero.
 *
 * Operadores sobrecargados:
 *   operator<<   imprime la ficha del producto en consola
 *   operator==   compara dos productos por su id
 *   operator>    compara dos productos por su stock actual
 *   operator<    compara dos productos por su stock actual
 * ========================================================= */

class Producto
{

protected:
    std::string id;
    std::string nombre;
    std::string tipo;
    int stock_actual;
    int stock_minimo;
    double precio_unitario;
    std::string estrategia_salida;
    std::string ubicacion;

public:
    /* Constructor */
    Producto(const std::string &id,
             const std::string &nombre,
             const std::string &tipo,
             int stock_actual,
             int stock_minimo,
             double precio_unitario,
             const std::string &estrategia_salida,
             const std::string &ubicacion);

    /* Destructor virtual para que funcione el polimorfismo */
    virtual ~Producto() = default;

    /* -------------------------------------------------------
     * Metodos virtuales puros — cada subclase los implementa
     * ------------------------------------------------------- */

    /* Calcula el coste total de almacenar las unidades actuales */
    virtual double calcularCosteAlmacenamiento() const = 0;

    /* Devuelve una etiqueta con el tipo, por ejemplo "[FRAGIL]" */
    virtual std::string getTipoEtiqueta() const = 0;

    /* -------------------------------------------------------
     * Metodo concreto — igual para todas las subclases
     * Logica de la seccion 5.3 de la memoria del proyecto
     * ------------------------------------------------------- */

    /* Devuelve el estado del stock: NORMAL, BAJO, CRITICO o SIN STOCK */
    std::string calcularEstado() const;

    /* -------------------------------------------------------
     * Getters
     * ------------------------------------------------------- */

    const std::string &getId() const;
    const std::string &getNombre() const;
    const std::string &getTipo() const;
    int getStockActual() const;
    int getStockMinimo() const;
    double getPrecioUnitario() const;
    const std::string &getEstrategia() const;
    const std::string &getUbicacion() const;

    /* -------------------------------------------------------
     * Operadores sobrecargados
     * ------------------------------------------------------- */

    /* Imprime la ficha completa del producto en un ostream */
    friend std::ostream &operator<<(std::ostream &os, const Producto &p);

    /* Dos productos son iguales si tienen el mismo id */
    bool operator==(const Producto &otro) const;

    /* Compara por stock actual — util para ordenar listas */
    bool operator>(const Producto &otro) const;
    bool operator<(const Producto &otro) const;
};