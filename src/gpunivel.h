// GPU rendering levels shared by webOS and the Linux preview.
// 0: full effects; 1: light effects; 2: minimal effects;
// 3: minimal effects drawn at 1280x720, only when explicitly requested.
//
// Automatic mode measures frame wait time against CPU time on a populated
// Home screen and steps down to level 2 at most. The learned level is saved
// by GPU, driver, model and surface resolution in gpu-nivel.txt.
// NUVIO_GPU_NIVEL can force a level in the Linux preview.
// Full-screen target discard uses ES3 or EXT_discard_framebuffer when present.
#ifndef NV_GPUNIVEL_H
#define NV_GPUNIVEL_H

// Depois do contexto GL, ANTES de gfx_iniciar e de tex_iniciar: le GL_*,
// decide o nivel inicial e marca a GPU fraca no perfil (perfiltv.h).
// `w`,`h` = drawable da janela.
void gpun_iniciar(int w, int h);


int  gpun_nivel(void);
// Troca o nivel na hora, sem medir nem gravar (captura de teste).
void gpun_definir_nivel(int n);
// Ajuste "Efeitos visuais" do .tpk: 0 automatico, 1 completos, 2 leves.
void gpun_preferencia(int p);

// Ajuste "Resolucao da interface = 720p": fixa o nivel 3 (desenho interno em
// 1280x720) e desliga a medida e o "Efeitos visuais" ate o fim da sessao.
void gpun_forcar_720(void);

// 4K that did not hold (resolucao.h): from the next frame on, the interface is
// drawn into an internal 1920x1080 target and scaled to the 4K surface in one
// pass; the adaptive level measures again at 1080p. No-op without a surface
// above 1080p or with 720p forced. Session-only; main.c remembers it on disk.
void gpun_alvo_1080(void);
int  gpun_alvo_1080_ativo(void);

// Laco de quadro: _inicio ANTES do glClear da tela (liga o alvo interno no
// nivel 2), _fim depois do ultimo desenho e antes do swap (amplia e descarta).
void gpun_quadro_inicio(void);
void gpun_quadro_fim(void);

// Uma medida por quadro, com as fases do quadro ANTERIOR (main.c). `naHome`
// = a home e a tela de frente, sem detalhe nem player; `cheia` = com artes na
// tela (a home vazia roda a 60 e nao diz nada).
void gpun_medir(double dtms, double espera, double cpu, int naHome, int cheia);

// Descarta a COR do alvo ligado agora, que vai ser sobrescrito inteiro.
// `padrao` = 1 se o ligado e a janela (FBO 0). No-op sem ES3/extensao.
void gpun_descartar_cor(int padrao);
#endif
