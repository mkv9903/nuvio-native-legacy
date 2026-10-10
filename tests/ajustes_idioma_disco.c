// #129: IDIOMA DE LEGENDA E AUDIO ESCOLHIDO NA TV SOBREVIVE AO ARRANQUE.
//
// O relato: "escolhi ingles para legenda e audio, saio dos ajustes e nao
// fica salvo; o filme seguinte nao liga a legenda em ingles". O ajustes.txt
// recebia "legendaIdioma 3" certinho — quem perdia era a LEITURA.
// ajustes_dir le o arquivo passando cada valor por limita(), que para as duas
// linhas de idioma consulta nValores() -> nLingua. E nLingua so era
// preenchido por rotulosDeIdioma(), chamado DEPOIS do laco de leitura: no
// arranque a lista tinha "1 valor", todo indice >= 1 era recusado como fora
// da faixa, e a legenda voltava a "Da conta" e o audio ao "Original".
//
// Este teste grava o arquivo como gravar() gravaria, le com ajustes_dir num
// processo que nunca abriu a tela de Ajustes (nLingua zerado, igual ao
// arranque) e confere o indice E o que chega em linguas.c, que e o que o
// player consulta (ling_legenda/ling_audio).
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

// Volta o modulo ao estado de um processo que acabou de abrir: e o que separa
// um caso do seguinte (o arquivo e a unica coisa que atravessa um arranque).
static void reiniciarIdioma(void) {
  valor[AJ_IDIOMA] = 0;
  idiomaEfetivo = IDIOMA_EN;
  idiomaPosArranque = 0;
  idiomaUltimaFonte = -1;
  idiomaFonteGravada = IDA_PADRAO;
  sistemaLoc[0] = contaTmdbLing[0] = contaLegLing[0] = 0;
}
static void escrever(const char *caminho, const char *texto) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(texto, f);
  fclose(f);
}
// O numero de uma linha "chave N" do arquivo, ou -1 se nao existe.
static int linhaDoArquivo(const char *caminho, const char *chave) {
  char linha[96], k[64];
  int v, achou = -1;
  FILE *f = fopen(caminho, "r");
  assert(f);
  while (fgets(linha, sizeof linha, f))
    if (sscanf(linha, "%63s %d", k, &v) == 2 && !strcmp(k, chave)) achou = v;
  fclose(f);
  return achou;
}

int main(void) {
  char dir[] = "/tmp/nuvio-aj-idioma-XXXXXX";
  char caminho[700];
  FILE *f;
  int en = -1, es = -1, i;
  assert(mkdtemp(dir));
  for (i = 0; i < ling_opcao_n(); i++) {
    if (!strcmp(ling_opcao_codigo(i), "en")) en = i;
    if (!strcmp(ling_opcao_codigo(i), "es")) es = i;
  }
  assert(en > 1 && es > 1);
  assert(nLingua == 0);   // o estado do arranque: a tela nunca abriu

  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w");
  assert(f);
  // An older file may still contain the retired upload preference. Adjacent
  // retained choices must load by key rather than shift with enum positions.
  fprintf(f, "legendaIdioma %d\naudioIdioma %d\nenvioAuto 0\n"
             "texturasMB 2\ntamanhoMaxLocal 3\ntamanhoMinLocal 1\n"
             "iconeAppLocal 7\n", en, es);
  fclose(f);

  ajustes_dir(dir);
  assert(valor[AJ_LEG_LINGUA] == en);
  assert(valor[AJ_AUD_LINGUA] == es);
  assert(valor[AJ_TEX_MB] == 2 && valor[AJ_TAM_MAX] == 3 && valor[AJ_TAM_MIN] == 1);
  assert(ajustes_icone_app() == 7 && iconeapp_atual() == 7);
  assert(!strcmp(ling_legenda(), "en"));
  assert(!strcmp(ling_audio(), "es"));

  // A conta com outro idioma nao desfaz a escolha desta TV (linguas.c: a
  // local ganha). E o blob da linha "[ajustes] idiomas da conta" do relato.
  ajustes_aplicar_blob("{\"features\":{\"player_settings\":{"
    "\"subtitle_preferred_language\":{\"type\":\"string\",\"value\":\"pt-br\"},"
    "\"preferred_audio_language\":{\"type\":\"string\",\"value\":\"\"}}}}");
  assert(valor[AJ_LEG_LINGUA] == en);
  assert(valor[AJ_AUD_LINGUA] == es);
  assert(!strcmp(ling_legenda(), "en"));
  assert(!strcmp(ling_audio(), "es"));

  // ABRIR A TELA DE AJUSTES nao zera a escolha (#129, a causa de campo):
  // conferirPadroes comparava o idioma contra o n=2 da tabela e voltava tudo
  // a "Da conta" — "padrao fora da lista em 4 (\"Idioma do áudio\")".
  ajustes_iniciar();
  assert(valor[AJ_LEG_LINGUA] == en);
  assert(valor[AJ_AUD_LINGUA] == es);
  assert(iconeapp_atual() == 7);
  ajustes_iniciar();
  assert(valor[AJ_LEG_LINGUA] == en);

  // O que ficou em disco continua sendo a escolha, para o proximo arranque.
  { char linha[96]; int achouLeg = 0;
    f = fopen(caminho, "r");
    assert(f);
    while (fgets(linha, sizeof linha, f)) {
      int v;
      if (sscanf(linha, "legendaIdioma %d", &v) == 1) { assert(v == en); achouLeg = 1; }
    }
    fclose(f);
    assert(achouLeg); }

  // ---- IDIOMA AUTOMATICO DA INTERFACE -------------------------------------
  // Cada caso parte de um arquivo escrito a mao, como o de uma versao anterior
  // ou o de uma TV nova, e le com ajustes_dir num estado limpo.
  unlink(caminho);

  // (1) TV nova: nenhum arquivo -> automatico, e sem conta nem TV, ingles.
  reiniciarIdioma();
  ajustes_dir(dir);
  assert(valor[AJ_IDIOMA] == 0 && ajustes_idioma() == IDIOMA_EN);

  // (2) ARQUIVO DE ANTES DA MARCA: "idioma 0" (ou 1) sem idiomaAutoLocal e
  // escolha manual, nao importa o que a conta diga depois.
  escrever(caminho, "idioma 0\n");
  reiniciarIdioma();
  ajustes_dir(dir);
  assert(valor[AJ_IDIOMA] == 1 && ajustes_idioma() == IDIOMA_PT);
  ajustes_aplicar_blob("{\"features\":{\"tmdb_settings\":{"
    "\"tmdb_language\":{\"type\":\"string\",\"value\":\"ro-RO\"}}}}");
  assert(ajustes_idioma() == IDIOMA_PT);          // a conta nao desfaz a escolha
  assert(valor[AJ_IDIOMA] == 1);
  escrever(caminho, "idioma 1\n");
  reiniciarIdioma();
  ajustes_dir(dir);
  assert(valor[AJ_IDIOMA] == 0 && ajustes_idioma() == IDIOMA_EN);
  // A lista do "Idioma dos metadados" cresceu no fim: os 14 indices antigos
  // continuam apontando para os mesmos codigos.
  valor[AJ_IDIOMA] = 1 + IDIOMA_EN;
  valor[AJ_TMDB_IDIOMA] = 1;  assert(!strcmp(ajustes_tmdb_idioma(), "pt-BR"));
  valor[AJ_TMDB_IDIOMA] = 7;  assert(!strcmp(ajustes_tmdb_idioma(), "pt-PT"));
  valor[AJ_TMDB_IDIOMA] = 13; assert(!strcmp(ajustes_tmdb_idioma(), "ru-RU"));
  valor[AJ_TMDB_IDIOMA] = 31; assert(!strcmp(ajustes_tmdb_idioma(), "zh-TW"));
  valor[AJ_TMDB_IDIOMA] = 0;

  unlink(caminho);
  { char tmp[700]; snprintf(tmp, sizeof tmp, "%s/ajustes.tmp", dir); unlink(tmp); }
  rmdir(dir);
  puts("ajustes_idioma_disco: ok");
  return 0;
}
