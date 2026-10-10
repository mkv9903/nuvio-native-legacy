// Notice center for updates, local session interruptions and safe-mode recovery.
// Startup records a session marker; clean shutdown removes it. Crash reports and
// the previous session log remain local. No announcements or upload worker exists.
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
// One-shot update action delivered to app.c.
enum { AVISOS_NADA = 0, AVISOS_ABRIR_ATUALIZACAO };
int  avisos_pediu(void);

// A LISTA COMO COMPONENTE, para a aba AVISOS do painel de Salvos: quem hospeda
// desenha na caixa que tem, guarda o proprio foco e chama _ok no OK. _ok
// devolve 1 quando a linha abre outra coisa (o hospedeiro fecha); _marcar_lidos
// ao fechar. Uma linha mede AVISOS_LINHA_H, EXCETO a linha em foco de um aviso
// local, que cresce para o texto inteiro: o hospedeiro posiciona por
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
// Opens an update or dismisses a local notice, and
// marca o item como lido.
void avisos_ilha_acao(const char *chave, int botao);

#ifdef AVISOS_TESTE
void avisos_teste_queda(const char *texto);
#endif

#endif
