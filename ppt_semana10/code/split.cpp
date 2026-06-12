// split.h
struct SplitResultado {
  vector<Ruta> rutas;
  double       costo;
  bool         factible;
};
SplitResultado split_deposito(
  const vector<int>& clientes,
  int   dep_idx,  // 0-based en mat
  const Matriz&    mat,
  const InstanciaLRP& inst);

vector<SplitResultado> evaluar_cromosoma(
  CromosomaLRP&       crom,
  const Matriz&       mat,
  const InstanciaLRP& inst,
  double alpha = 1000.0);
