// A TROCA DE CENA DOS AJUSTES (imagem da categoria / previa da opcao).
//
// Cada categoria e opcao usa uma ilustracao gerada. A troca acompanha
// o foco sem cortar o quadro de uma vez.
//
// Aqui a cena pedida so entra quando esta PRONTA: enquanto as imagens dela
// carregam, o quadro continua mostrando a cena anterior (a nova e desenhada
// com opacidade 0, so para pedir as texturas e contar as que faltam, em
// ajCenaFaltas). Pronta, ela entra por cima da anterior em AJ_TROCA_S.
//
// Custo: 1 camada parada (o desenho direto de sempre, sem FBO); 2 camadas do
// tamanho do quadro so durante a espera/troca. A cena nova entra com a
// opacidade de grupo, peca por peca, sobre a anterior ainda inteira: chega
// exatamente na cena nova em t=1 e nao precisa de alvo fora da tela (a C9 nao
// perdoa FBO a toa).
//
// Sem troca (corte, como antes): primeira vez que o quadro aparece, quadro que
// mudou de tamanho, e cenas de alturas diferentes (o texto embaixo acompanha a
// cena pedida e a anterior ficaria por cima dele).
#define AJ_TROCA_S 0.22f       // duracao da troca
#define AJ_TROCA_ESPERA 1.2f   // teto da espera pelas imagens da cena nova
// Seta segurada (um pedido novo menos de AJ_TROCA_RAPIDO depois do anterior):
// a troca so comeca quando o foco para por AJ_TROCA_ASSENTA. Sem isto, andar
// depressa pelas categorias emendava uma troca na outra e o quadro ficava com
// duas camadas o tempo todo.
#define AJ_TROCA_RAPIDO 0.30f
#define AJ_TROCA_ASSENTA 0.15f

typedef struct { int sec, op, chave; } AjCenaRef;
typedef struct {
  AjCenaRef mostra, alvo;   // a cena no quadro e a que vai entrar
  float t;                  // 0 = esperando a alvo ficar pronta; (0,1] = entrando
  float espera, w, h;       // h = altura da cena no quadro
  int trocando, quadro;     // quadro = o ultimo em que foi desenhado
  int pedida, rapido;       // a chave pedida no quadro anterior; seta segurada
  float desde;              // tempo desde que o pedido mudou
} AjCenaTrocaE;
static AjCenaTrocaE ajTroca[AJ_TROCA_N];
static int ajTrocaQuadro = 1;
static float ajTrocaDt;

static void ajCenaTrocaAtualizar(float dt) {
  ajTrocaQuadro++;
  ajTrocaDt = dt > 0.0f ? (dt < 0.1f ? dt : 0.1f) : 0.0f;
}

// A "mesma imagem": duas opcoes com a mesma chave desenham a mesma cena (so
// muda o detalhe da opcao em foco) e trocam direto, sem espera nem mistura.
// Cada opcao tem a sua chave, assim como cada resumo de categoria.
static int ajCenaChave(int sec, int op) {
  // 2.0.2: cada opcao tem o seu desenho gerado; trocar de opcao e uma troca
  // de cena (cruzamento de 0,22 s, sem espera: nao ha imagem para ler).
  return op < 0 ? 2000 + sec : op;
}
static float ajCenaTrocaDesenhar(int quadro, AjCenaRef c, float x, float y, float w, float corte) {
  if (quadro == AJ_TROCA_RESUMO) return aj2PreviaCategoria(c.sec, x, y, w, corte);
  if (c.op < 0) return ajVisualCena(c.sec, -1, x, y, w);
  return aj2Previa(c.op, x, y, w);
}

static float ajCenaTroca(int quadro, int sec, int op, float x, float y, float w, float corte) {
  AjCenaTrocaE *e = &ajTroca[quadro];
  AjCenaRef ped = { sec, op, ajCenaChave(sec, op) };
  float grupo = gfx_opacidade_grupo, h;
  int novoQuadro = e->quadro != ajTrocaQuadro, faltas;
  // Quadro que nao estava na tela no quadro anterior, ou que mudou de largura:
  // nao ha cena anterior para segurar.
  if (e->quadro < ajTrocaQuadro - 1 || fabsf(e->w - w) > 0.5f) {
    e->mostra = ped; e->trocando = 0; e->pedida = ped.chave; e->desde = AJ_TROCA_RAPIDO;
  }
  e->quadro = ajTrocaQuadro; e->w = w;
  if (novoQuadro) e->desde += ajTrocaDt;
  if (ped.chave != e->pedida) {
    e->rapido = e->desde < AJ_TROCA_RAPIDO;
    e->pedida = ped.chave; e->desde = 0;
  }
  if (!e->trocando) {
    if (ped.chave == e->mostra.chave) {   // a mesma imagem: desenho direto
      e->mostra = ped;
      return e->h = ajCenaTrocaDesenhar(quadro, ped, x, y, w, corte);
    }
    e->trocando = 1; e->alvo = ped; e->t = 0; e->espera = 0;
  }
  // Esperando: a alvo acompanha o foco. Entrando: a troca termina na cena em
  // que comecou e a seguinte parte dela (nada de corte no meio).
  if (e->t <= 0.0f) {
    if (ped.chave == e->mostra.chave) {   // voltou para a cena que esta no quadro
      e->trocando = 0; e->mostra = ped;
      return e->h = ajCenaTrocaDesenhar(quadro, ped, x, y, w, corte);
    }
    if (ped.chave != e->alvo.chave) e->espera = 0;
    e->alvo = ped;
    // A cena nova, invisivel: pede as texturas dela, mede a altura e diz se
    // falta alguma.
    faltas = ajCenaFaltas;
    gfx_opacidade_grupo = 0;
    h = ajCenaTrocaDesenhar(quadro, e->alvo, x, y, w, corte);
    gfx_opacidade_grupo = grupo;
    faltas = ajCenaFaltas - faltas;
    if (fabsf(h - e->h) > 1.0f) {   // alturas diferentes: corte
      e->trocando = 0; e->mostra = ped;
      return e->h = ajCenaTrocaDesenhar(quadro, ped, x, y, w, corte);
    }
    ajCenaTrocaDesenhar(quadro, e->mostra, x, y, w, corte);
    if (novoQuadro) e->espera += ajTrocaDt;
    if (faltas > 0 && e->espera < AJ_TROCA_ESPERA) return h;
    if (e->rapido && e->desde < AJ_TROCA_ASSENTA) return h;
    if (ajustes_animacoes_reduzidas()) { e->mostra = e->alvo; e->trocando = 0; return h; }
    e->t = 0.0001f;   // pronta: comeca a entrar no proximo quadro
    return h;
  }
  if (novoQuadro) e->t += ajTrocaDt / AJ_TROCA_S;
  if (e->t >= 1.0f) {
    e->mostra = e->alvo; e->trocando = 0;
    h = e->h = ajCenaTrocaDesenhar(quadro, e->mostra, x, y, w, corte);
  } else {
    float k = e->t * e->t * (3.0f - 2.0f * e->t);
    ajCenaTrocaDesenhar(quadro, e->mostra, x, y, w, corte);
    gfx_opacidade_grupo = grupo * k;
    h = ajCenaTrocaDesenhar(quadro, e->alvo, x, y, w, corte);
    gfx_opacidade_grupo = grupo;
  }
  // O texto embaixo e o da opcao em foco: a altura devolvida e a da cena dela
  // (igual a da que esta no quadro, senao teria sido corte).
  return h;
}
