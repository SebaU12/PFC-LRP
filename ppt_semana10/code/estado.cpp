// parser.h
struct InstanciaLRP {
  int    num_clientes, num_depositos;
  vector<Cliente>  clientes;   // 1..nc
  vector<Deposito> depositos;  // nc+1..nc+nd
  double Q, F;  bool es_entero;
};
// estado.h
using Ruta  = vector<int>;
using Rutas = map<int, vector<Ruta>>;
struct EstadoLRP {
  Rutas        rutas;         // dep_id -> rutas
  vector<int>  depositos_abiertos;
  vector<int>  no_asignados;
  const Matriz       *matriz; // puntero ligero
  const InstanciaLRP *datos;  // puntero ligero
  EstadoLRP copy() const;
  double    objective() const;
};
