// QUEM POE E TIRA OS CARTOES DA ILHA (ilha.h): a atividade ao vivo.
//
// ATIVIDADE AO VIVO. O player, ao fechar, entrega a sessao (ilhacart_player_saiu).
// Ela vira cartao pelo MESMO criterio da faixa "Retomar agora" (entre 1% e o
// Percentual assistido, home_retorno_vale) e fica enquanto a faixa ficaria: a
// proxima saida do player a troca ou a tira (retomou e terminou, ou largou
// outro titulo no meio). Alem disso ela SAI SOZINHA depois de 30 min sem
// nenhuma tecla (VIVO_OCIOSO_MS): a TV esquecida ligada na home nao fica a
// noite inteira oferecendo um episodio de horas atras, e quem volta ainda tem
// a faixa da home. "Fechar" no modal tira na hora. Nada e gravado: fechar o
// app encerra.
//
// FIO PRINCIPAL.
#ifndef NV_ILHACART_H
#define NV_ILHACART_H
#include <SDL2/SDL.h>
#include <stddef.h>
#include "ilha.h"

void ilhacart_player_saiu(int indice, double posSeg, double durSeg, int t, int e);
// A atividade ao vivo pertence a conta/perfil que encerrou o player. So
// compara identidade em RAM; chamar antes de eventos/quadro, mesmo no login.
void ilhacart_validar_identidade(void);
void ilhacart_esquecer_vivo(void);
// Valida tambem uma copia devolvida pelo modal antes de retomar/abrir titulo.
int ilhacart_vivo_vale(const IlhaCartao *cartao);
// Toda tecla ou clique (app_evento): o relogio da ociosidade recomeca.
void ilhacart_tecla(Uint32 agora);
// Sobe a cada saida do player que POS o cartao: compare antes e depois de
// player_encerrar para saber se ESTA saida virou atividade ao vivo.
unsigned ilhacart_vivo_seq(void);
// O titulo que o cartao ao vivo segura ("" sem cartao). Ver cwretido.h.
const char *ilhacart_vivo_imdb(void);
// Por quadro: valida a identidade e o tempo de ociosidade.
void ilhacart_atualizar(Uint32 agora);
// O modal pediu para tirar o cartao (Fechar / Dispensar).
void ilhacart_dispensar(int qual);

#endif
