// =============================================================================
// algoritmo.h — Interfaz común para todos los algoritmos del proyecto
// =============================================================================
//
// Este archivo define el "contrato" que deben cumplir ALNS y MA|PM.
// La idea es que el código de comparación (validate.cpp, benchmark.cpp)
// pueda usar cualquier algoritmo sin saber cómo funciona por dentro.
//
// Funciona con polimorfismo: ambos algoritmos heredan de IAlgoritmo
// e implementan ejecutar(). Desde afuera solo llamas:
//
//   auto resultado = algoritmo.ejecutar(sol_inicial);
//
// y obtienes el mismo tipo de resultado sin importar qué algoritmo uses.
//
// =============================================================================
#pragma once
#include "estado.h"
#include <string>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// Lo que devuelve cualquier algoritmo al terminar.
// Tiene todo lo necesario para analizar qué tan bien funcionó.
// -----------------------------------------------------------------------------
struct ResultadoAlgoritmo {
    EstadoLRP mejor_estado;   // La mejor solución encontrada durante toda la corrida

    double costo_inicial;     // Z de la solución de entrada (antes de optimizar)
    double costo_final;       // Z de la mejor solución encontrada (siempre ≤ costo_inicial)
    int    iteraciones;       // Cuántas iteraciones ejecutó el algoritmo

    // Registro de cada vez que se encontró una solución mejor que la anterior.
    // Útil para graficar la curva de convergencia del algoritmo.
    // Formato: (número de iteración, costo en ese momento)
    std::vector<std::pair<int, double>> historial_mejoras;
};

// -----------------------------------------------------------------------------
// Interfaz que todo algoritmo debe implementar.
// Si en el futuro agregas un tercer algoritmo, hereda de aquí.
// -----------------------------------------------------------------------------
class IAlgoritmo {
public:
    virtual ~IAlgoritmo() = default;

    // Ejecuta el algoritmo partiendo de sol_inicial y devuelve el resultado.
    // No modifica sol_inicial — trabaja sobre copias internas.
    virtual ResultadoAlgoritmo ejecutar(const EstadoLRP &sol_inicial) = 0;

    // Nombre del algoritmo para reportes ("ALNS", "MA|PM", etc.)
    virtual std::string nombre() const = 0;
};
