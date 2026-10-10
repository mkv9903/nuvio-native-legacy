// Dismissed notices: IDs persist per profile. Dismissing update:1.8.1
// does not suppress update:1.8.2 or later upstream announcements.
//
// POR PERFIL, EM DISCO: avisos-dispensados.txt na pasta de dados, uma linha
// "<perfil> <chave>" (o formato do enquete.txt). Teto de AVD_MAX linhas; a mais
// velha sai primeiro — uma chave de evento antigo nao volta a ser emitida, entao
// perder a mais velha nao traz nada de volta na pratica.
//
// SESSAO: avisodisp_sessao_* e o conjunto em RAM do que ja foi dito/adiado
// nesta sessao (tambem por perfil). E o limite de ruido: um aviso que ja passou
// pela ilha nesta sessao nao volta a passar, a menos que seja um evento novo
// (outra chave).
//
// Thread-safe (o fio do canal de avisos.c consulta). Sem SDL, testavel sozinho
// (tests/avisodisp.c).
#ifndef NV_AVISODISP_H
#define NV_AVISODISP_H

#define AVD_ARQ   "avisos-dispensados.txt"
#define AVD_MAX   200
#define AVD_CHAVE 80

// 1 = a chave foi dispensada no perfil ativo.
int  avisodisp_tem(const char *chave);
// Dispensa no perfil ativo e grava. Repetir nao duplica.
void avisodisp_por(const char *chave);
// Nesta sessao (RAM): ja dito/adiado no perfil ativo.
int  avisodisp_sessao_tem(const char *chave);
void avisodisp_sessao_por(const char *chave);
// Para os testes: esquece a RAM (o arquivo e relido na proxima consulta).
void avisodisp_esquecer(void);

#endif
