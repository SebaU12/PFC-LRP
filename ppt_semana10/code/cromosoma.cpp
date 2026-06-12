// cromosoma.h
struct CromosomaLRP {
  // DS[i]=0: deposito i cerrado
  // DS[i]=k: abierto, empieza en CS[k-1]
  vector<int> DS; // longitud m
  vector<int> CS; // longitud n (1-indexed)
  double fitness;
  bool abierto(int dep_idx) const;
  pair<int,int>
    rango_cs(int, const InstanciaLRP&) const;
  vector<int>
    clientes_de(int, const InstanciaLRP&) const;
};
CromosomaLRP estado_a_cromosoma(
  const EstadoLRP&);
EstadoLRP    cromosoma_a_estado(
  const CromosomaLRP&, const Rutas&,
  const Matriz&, const InstanciaLRP&);
