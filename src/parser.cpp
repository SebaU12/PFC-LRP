// =============================================================================
// parser.cpp — Lector de archivos .dat (formato Prodhon / benchmark estándar LRP)
// =============================================================================
//
// El archivo .dat no tiene etiquetas — solo números en orden fijo.
// La lectura es secuencial: cada bloque de líneas corresponde a un concepto.
//
// Orden de los datos en el archivo:
//   Línea 1:        número de clientes (n)
//   Línea 2:        número de depósitos (m)
//   Líneas 3..m+2:  coordenadas x,y de cada depósito
//   Líneas m+3..m+n+2: coordenadas x,y de cada cliente
//   Línea siguiente: capacidad Q del vehículo
//   m líneas:       capacidad W_i de cada depósito
//   n líneas:       demanda d_j de cada cliente
//   m líneas:       costo de apertura O_i de cada depósito
//   1 línea:        costo fijo F del vehículo
//   1 línea:        flag 0/1 (0 = distancias enteras ×100, 1 = flotante real)
//
// =============================================================================
#include "parser.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

// -----------------------------------------------------------------------------
// Lee el archivo completo y lo divide en "fichas" (tokens) por línea.
// Ignora líneas vacías y normaliza saltos de línea Windows (\r\n → \n).
// Devuelve: vector de líneas, donde cada línea es un vector de palabras/números.
// -----------------------------------------------------------------------------
static std::vector<std::vector<std::string>>
tokenizar_archivo(const std::string &ruta) {
    std::ifstream f(ruta);
    if (!f.is_open())
        throw std::runtime_error("No se pudo abrir: " + ruta);

    std::vector<std::vector<std::string>> lineas;
    std::string linea;

    while (std::getline(f, linea)) {
        // Los archivos generados en Windows tienen \r al final — lo eliminamos
        if (!linea.empty() && linea.back() == '\r')
            linea.pop_back();

        // Separar la línea en tokens (números separados por espacios)
        std::istringstream ss(linea);
        std::vector<std::string> tokens;
        std::string tok;
        while (ss >> tok)
            tokens.push_back(tok);

        // Solo guardamos líneas que tengan algo (ignoramos líneas en blanco)
        if (!tokens.empty())
            lineas.push_back(tokens);
    }
    return lineas;
}

// -----------------------------------------------------------------------------
// Convierte el contenido del archivo .dat en una InstanciaLRP lista para usar.
//
// Usamos un índice `idx` que avanza línea a línea — así es fácil seguir
// en qué parte del archivo estamos en cada momento.
// -----------------------------------------------------------------------------
InstanciaLRP leer_instancia(const std::string &ruta) {
    auto lineas = tokenizar_archivo(ruta);

    // El nombre de la instancia es solo el nombre del archivo (sin la ruta completa)
    std::string nombre = ruta;
    auto pos = ruta.find_last_of("/\\");
    if (pos != std::string::npos)
        nombre = ruta.substr(pos + 1);

    InstanciaLRP inst;
    inst.nombre = nombre;

    int idx = 0; // puntero a la línea actual del archivo

    // ── Dimensiones ──────────────────────────────────────────────────────────
    inst.num_clientes  = std::stoi(lineas[idx++][0]);
    inst.num_depositos = std::stoi(lineas[idx++][0]);

    // ── Coordenadas de los depósitos ─────────────────────────────────────────
    // Los IDs de los depósitos arrancan justo después de los clientes:
    // si hay 10 clientes → depósito 1 tiene ID 11, depósito 2 tiene ID 12, etc.
    inst.depositos.resize(inst.num_depositos);
    for (int i = 0; i < inst.num_depositos; ++i, ++idx) {
        inst.depositos[i].id = inst.num_clientes + i + 1;
        inst.depositos[i].x  = std::stod(lineas[idx][0]);
        inst.depositos[i].y  = std::stod(lineas[idx][1]);
    }

    // ── Coordenadas de los clientes ───────────────────────────────────────────
    // Los IDs de los clientes empiezan en 1
    inst.clientes.resize(inst.num_clientes);
    for (int i = 0; i < inst.num_clientes; ++i, ++idx) {
        inst.clientes[i].id = i + 1;
        inst.clientes[i].x  = std::stod(lineas[idx][0]);
        inst.clientes[i].y  = std::stod(lineas[idx][1]);
    }

    // ── Capacidad del vehículo (Q) ────────────────────────────────────────────
    // Un solo número — todos los camiones tienen la misma capacidad
    inst.Q = std::stod(lineas[idx++][0]);

    // ── Capacidad de cada depósito (W_i) ─────────────────────────────────────
    for (int i = 0; i < inst.num_depositos; ++i, ++idx)
        inst.depositos[i].capacidad = std::stod(lineas[idx][0]);

    // ── Demanda de cada cliente (d_j) ─────────────────────────────────────────
    for (int i = 0; i < inst.num_clientes; ++i, ++idx)
        inst.clientes[i].demanda = std::stod(lineas[idx][0]);

    // ── Costo de apertura de cada depósito (O_i) ──────────────────────────────
    for (int i = 0; i < inst.num_depositos; ++i, ++idx)
        inst.depositos[i].costo_apertura = std::stod(lineas[idx][0]);

    // ── Costo fijo por vehículo (F) ───────────────────────────────────────────
    inst.F = std::stod(lineas[idx++][0]);

    // ── Flag de tipo de distancia ─────────────────────────────────────────────
    // 0 → distancias enteras (dist * 100, truncado) — algunos benchmarks lo exigen
    //     para poder comparar resultados con la literatura exactamente
    // 1 → distancias reales (flotante euclidiano)
    inst.es_entero = (std::stoi(lineas[idx][0]) == 0);

    return inst;
}

// -----------------------------------------------------------------------------
// Imprime un resumen de la instancia en consola.
// Útil para verificar que el archivo se leyó bien antes de correr el algoritmo.
// -----------------------------------------------------------------------------
void mostrar_resumen(const InstanciaLRP &inst) {
    const std::string sep65(65, '=');
    const std::string sep35(35, '-');
    const std::string sep50(50, '-');

    std::cout << sep65 << "\n";
    std::cout << "RESUMEN DE LA INSTANCIA: " << inst.nombre << "\n";
    std::cout << sep65 << "\n";

    std::cout << "\n1. PARAMETROS GLOBALES\n" << sep35 << "\n";
    std::cout << std::left
              << std::setw(25) << "Total Clientes:"        << inst.num_clientes << "\n"
              << std::setw(25) << "Total Depositos:"       << inst.num_depositos << "\n"
              << std::setw(25) << "Capacidad Vehiculo (Q):"<< inst.Q << "\n"
              << std::setw(25) << "Costo Vehiculo (F):"    << inst.F << "\n"
              << std::setw(25) << "Distancias (*100 int):" << (inst.es_entero ? "Si" : "No") << "\n";

    std::cout << "\n2. INSTALACIONES POTENCIALES (DEPOSITOS)\n"
              << std::string(65, '-') << "\n";
    std::cout << std::left
              << std::setw(10) << "ID Nodo" << " | "
              << std::setw(10) << "Coord X" << " | "
              << std::setw(10) << "Coord Y" << " | "
              << std::setw(15) << "Capacidad (W)"
              << " | Costo Apertura (O)\n"
              << std::string(65, '-') << "\n";
    for (const auto &d : inst.depositos)
        std::cout << std::setw(10) << d.id      << " | "
                  << std::setw(10) << d.x        << " | "
                  << std::setw(10) << d.y        << " | "
                  << std::setw(15) << d.capacidad << " | "
                  << d.costo_apertura << "\n";

    std::cout << "\n3. CLIENTES (Muestra de los primeros 5 y ultimos 2)\n"
              << sep50 << "\n";
    std::cout << std::left
              << std::setw(10) << "ID Nodo" << " | "
              << std::setw(10) << "Coord X" << " | "
              << std::setw(10) << "Coord Y"
              << " | Demanda (d)\n"
              << sep50 << "\n";

    int total = inst.num_clientes;
    auto print_cliente = [&](const Cliente &c) {
        std::cout << std::setw(10) << c.id << " | "
                  << std::setw(10) << c.x  << " | "
                  << std::setw(10) << c.y  << " | "
                  << c.demanda << "\n";
    };
    for (int i = 0; i < std::min(5, total); ++i)
        print_cliente(inst.clientes[i]);
    if (total > 7)
        std::cout << std::setw(10) << "..." << " | "
                  << std::setw(10) << "..." << " | "
                  << std::setw(10) << "..." << " | ...\n";
    for (int i = std::max(5, total - 2); i < total; ++i)
        print_cliente(inst.clientes[i]);

    std::cout << sep65 << "\n\n";
}
