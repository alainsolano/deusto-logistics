#pragma once

#include "producto.h"

/* =========================================================
 * productoInflamable.h  —  Producto de tipo inflamable
 *
 * Atributos adicionales:
 *   nivel_riesgo         nivel de peligrosidad: 1, 2 o 3
 *   zona_almacenamiento  zona especifica donde se almacena
 *
 * El coste de almacenamiento tiene un recargo segun el nivel
 * de riesgo: nivel 1 -> +10%, nivel 2 -> +20%, nivel 3 -> +30%
 * ========================================================= */

class ProductoInflamable : public Producto
{

private:
    int nivel_riesgo;
    std::string zona_almacenamiento;

public:
    /* Constructor */
    ProductoInflamable(const std::string &id,
                       const std::string &nombre,
                       int stock_actual,
                       int stock_minimo,
                       double precio_unitario,
                       const std::string &estrategia_salida,
                       const std::string &ubicacion,
                       int nivel_riesgo,
                       const std::string &zona_almacenamiento);

    /* Coste = stock_actual * precio_unitario * (1 + nivel_riesgo * 0.10) */
    double calcularCosteAlmacenamiento() const override;

    /* Etiqueta del tipo para mostrar en consola */
    std::string getTipoEtiqueta() const override;

    /* Getters propios */
    int getNivelRiesgo() const;
    const std::string &getZonaAlmacenamiento() const;
};