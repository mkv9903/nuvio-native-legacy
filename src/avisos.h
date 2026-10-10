// Notice center: updates, upstream announcements and session diagnostics.
// Notices appear while the app is open and can be dismissed per profile.
//
// O CRASH e detectado por MARCA: sessao-viva.txt e gravado no arranque e
// apagado na saida limpa. Marca presente no arranque seguinte = a sessao
// anterior morreu (crash, kill do SAM por memoria, energia). Nao ha como saber
// qual; o registro e que diz. main.c preserva o log da sessao anterior em
// /tmp/nuvio-anterior.log ANTES de truncar o atual, e e esse arquivo que
// "Enviar registro" manda (ultimos 200 KB) para o servico de recomendacoes
// (servidor/recomendacoes, rota /v1/registro). So com o botao; nunca sozinho.
// O log ja sai sem credencial (rede_url_publica em rede.h). No Tizen nao ha
// arquivo: vai so versao, plataforma e a data.
//
// O CANAL DO DONO e um arquivo JSON no repositorio, lido a cada 30 min:
//   [{"id":"2026-09-19-guia","desde":"2026-09-19","ate":"2026-10-01",
//     "plataforma":"todas|lg|tizen","ate_versao":"1.3.1",
//     "titulo":"...","titulo_en":"...","texto":"...","texto_en":"..."}]
// `ate_versao` deixa avisar so quem ainda esta numa versao com o defeito.
//
// DISCIPLINA: rede em fio proprio, estado atras de mutex, o laco de desenho so
// le. Chaves de tela em idioma_tab.h.
#ifndef NV_AVISOS_H
#define NV_AVISOS_H
#include <SDL2/SDL.h>

// Avisa (toast + item na lista) que o idioma da interface foi escolhido sozinho.
// Um item por idioma, entao so uma vez por idioma. Chamado por ajustes.c.
void avisos_idioma_definido(const char *codigo, const char *texto);
// Previous session left its crash marker. Valid after avisos_iniciar.
int  avisos_sessao_anterior_caiu(void);
// Aviso do modo seguro (ajuste desfeito, perfil seguro): entra na lista com toast.
// Um item por `id`; o id leva o numero da sessao, entao cada queda avisa uma vez.
void avisos_modo_seguro(const char *id, const char *titulo, const char *texto);
void avisos_iniciar(void);   // depois de dados_iniciar; grava a marca de sessao
void avisos_encerrar(void);  // saida limpa: apaga a marca
// Ultimo sinal de vida na marca de sessao: evento de janela ("oculto",
// "visivel", "foco-perdido"...) grava na hora; NULL e a batida periodica
// (no maximo 1 a cada 60 s). Ver marcaGravar em avisos.c.
void avisos_sinal(const char *evento, float rssMb);
void avisos_atualizar(float dt, Uint32 agora);
void avisos_desenhar(Uint32 agora);
// 1 quando consumiu o evento (toast na tela com AZUL/CH+, ou painel aberto).
int  avisos_evento(const SDL_Event *e);
int  avisos_aberto(void);
void avisos_abrir(void);
int  avisos_n_novos(void);
// O CARTAO DO CRASH: abre uma vez por crash, quando a home esta de pe e nenhum
// outro cartao esta na frente (app.c chama avisos_mostrar_se_houver).
// Enquanto aberto, avisos_evento come todo o teclado.
void avisos_mostrar_se_houver(void);
int  avisos_cartao_aberto(void);

// ACOES PEDIDAS PELO PAINEL, entregues a app.c uma vez cada (o mesmo contrato
// de spainel_pediu_abrir): o IMDb de um titulo a abrir, ou um dos codigos.
enum { AVISOS_NADA = 0, AVISOS_ABRIR_SALVOS, AVISOS_ABRIR_ATUALIZACAO };
int  avisos_pediu(void);

// ENVIO MANUAL DO REGISTRO DESTA SESSAO, pela linha "Enviar registro" dos
// Ajustes. Mesmo destino e mesmo teto (200 KB) do cartao de crash; o estado
// e o mesmo contador: 0 nada, 1 enviando, 2 enviado, 3 falhou.
void avisos_enviar_registro_atual(void);
// Envia somente o relatorio estruturado de uma sessao de diagnostico. O
// sucesso exige HTTP 2xx e um identificador de registro retornado pelo
// servidor; o log geral nao e capturado nem misturado a esta operacao.
int  avisos_enviar_diagnostico(const char *execucao_id, const char *relatorio,
                               char *registro_id, unsigned tam_registro_id);
// Envio automatico (ajuste "Enviar registros sozinho"): chamado por quadro.
void avisos_envio_auto_passo(Uint32 agora);
int  avisos_envio_estado(void);
// O ULTIMO ENVIO, para o painel de envio do registro (registro.c) e o
// inspetor de Ajustes. `motivo` diz por que o manual falhou: o servidor
// respondeu com erro (http), a TV esta sem internet (redesaude.h; nem tenta,
// e manda sozinho quando a rede volta), o prazo de 30 s acabou, a conexao
// caiu, falta login, ou esta compilacao nao tem servidor.
enum { AVISOS_ENVIO_OK = 1, AVISOS_ENVIO_SERVIDOR, AVISOS_ENVIO_OFFLINE,
       AVISOS_ENVIO_PRAZO, AVISOS_ENVIO_CONEXAO, AVISOS_ENVIO_CONTA,
       AVISOS_ENVIO_INDISPONIVEL };
#include <time.h>
typedef struct {
  int    disponivel;          // ha servidor de registros nesta compilacao
  int    estado, motivo, http, linhas, pendenteRede;
  long   bytes;
  time_t quando;              // inicio do ultimo envio manual
  char   codigo[8];           // codigo do ultimo envio manual (vazio sem recibo)
  time_t autoQuando;          // ultimo envio automatico (0 = nenhum)
  int    autoHttp;
  Uint32 autoProximoMs;       // SDL_GetTicks do proximo automatico (0 = nao marcado)
  char   ultimoCodigo[8];     // o ultimo codigo recebido, de qualquer sessao
  time_t ultimoCodigoQuando;
} AvisosEnvio;
int  avisos_envio_info(AvisosEnvio *o);
#ifdef AVISOS_TESTE_ENVIO
void avisos_teste_envio(int estado, int motivo, int http, const char *codigo, long bytes, int linhas);
void avisos_teste_envio_auto(long haSeg, int http);
void avisos_teste_queda(const char *texto);
#endif

// A LISTA COMO COMPONENTE, para a aba AVISOS do painel de Salvos: quem hospeda
// desenha na caixa que tem, guarda o proprio foco e chama _ok no OK. _ok
// devolve 1 quando a linha abre outra coisa (o hospedeiro fecha); _marcar_lidos
// ao fechar. Uma linha mede AVISOS_LINHA_H, EXCETO a linha em foco de um aviso
// do canal, que cresce para o texto inteiro: o hospedeiro posiciona por
// avisos_lista_y / avisos_lista_altura_linha, que sabem disso.
#include <stddef.h>
#define AVISOS_LINHA_H 164.0f
int   avisos_lista_n(void);
float avisos_lista_altura_linha(int linha, int focoLinha);
float avisos_lista_y(int linha, int focoLinha);
void  avisos_lista_desenhar(float x, float y, float w, float a, int focoLinha);
// 1 = performed an action and the host closes; 0 = keep the list open.
int   avisos_lista_ok(int linha);
void  avisos_marcar_lidos(void);

// Dismissal persists the notice ID per profile. A new ID can appear again.
// The last row is Dismiss all when the list is nonempty.
// avisos_lista_item returns the ID and title, or 0 for an invalid row.
void  avisos_dispensar(const char *id);
void  avisos_dispensar_todos(void);
int   avisos_lista_dispensar(int linha);
int   avisos_lista_linhas(void);
int   avisos_lista_item(int linha, char *id, size_t tamId, char *titulo, size_t tamTit);

// Mark one notice as read.
void avisos_marcar_visto(const char *id);

// Botao do MODAL DA ILHA de um aviso da central (chave "av:<id>", ver
// anunciarItem em avisos.c): `botao` 1..3, o que ilha_aviso_pediu entregou.
// Executa (abrir o titulo, salvar, enviar o registro, abrir a atualizacao) e
// marca o item como lido.
void avisos_ilha_acao(const char *chave, int botao);

#endif
