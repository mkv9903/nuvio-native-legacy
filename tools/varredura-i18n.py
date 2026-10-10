#!/usr/bin/env python3
"""Finds interface text that DOES NOT pass through the translation table.

WHY IT EXISTS. Three rounds of issue #12 were resolved manually, and with each
release the reporter sent a photo of yet another screen in Portuguese. Manual
scanning never finishes: misses split literals, misses accents written as \\uXXXX,
and cannot tell whether the key exists in the table.

TWO FLAW FAMILIES, fixed in different ways:

  1. DRAWN LITERAL WITHOUT KEY. text.c calls i18n() on everything it draws, so
     a loose literal TRANSLATES — if the key is in the table. Without the key it
     appears in Portuguese. Fix: add the line in idioma_tab.h.

  2. SENTENCE ASSEMBLED WITH snprintf. The final string ("Resume S1E1") never
     matches any key. Fix: wrap the FORMAT — snprintf(b, n, i18n("..."), x)
     — which is what already-fixed screens do.

Usage:
    python3 tools/varredura-i18n.py          # lists what is missing
    python3 tools/varredura-i18n.py --tabela # only lines ready to paste
"""
import re, sys, pathlib, unicodedata

RAIZ = pathlib.Path(__file__).resolve().parent.parent

# text.c functions that call i18n() on parameter `s`. txt_tracking is on the
# list on purpose: it measures AND draws, and was not passing through i18n.
DESENHO = ("txt_linha", "txt_linha_corta", "txt_linha_familia",
           "txt_linha_corta_familia", "txt_bloco", "txt_bloco_dir",
           "txt_tracking",
           # App log shortcuts (registro_ilha.inc, registro_envio.inc,
           # desempenho.c, telemetria.c): all pass text through txt_linha /
           # txt_bloco / txt_tracking, which translate.
           "rgT", "rgTC", "rgTT", "rgBloco", "rgKicker", "rgK2", "rgCaps",
           "rgStat", "rgCartNota", "rgBotao", "rgKbdTexto", "rgCabecalho",
           "rgSeguindo", "rgBlocoAltura", "dsT", "tlT", "tlCaps", "tlBotao")

# A C literal can come split ("abc" "def") and with escapes. Joins and decodes.
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"(?:\s*"((?:[^"\\]|\\.)*)")*')

def literais_colados(texto, i):
    """Reads sequence of adjacent literals starting at `texto[i]` == '"'."""
    partes, n = [], len(texto)
    while i < n:
        while i < n and texto[i] in " \t\n\r\\":
            i += 1
        if i >= n or texto[i] != '"':
            break
        i += 1
        atual = []
        while i < n and texto[i] != '"':
            if texto[i] == "\\":
                atual.append(texto[i]); i += 1
                if i < n: atual.append(texto[i]); i += 1
            else:
                atual.append(texto[i]); i += 1
        i += 1
        partes.append("".join(atual))
    return "".join(partes), i

def decodificar(s):
    # \xNN, \uNNNN and simple escapes. bytes -> utf-8, which is how table stores them.
    out, i, n = bytearray(), 0, len(s)
    simples = {"n": 10, "t": 9, "r": 13, "0": 0, '"': 34, "\\": 92, "'": 39}
    while i < n:
        c = s[i]
        if c != "\\":
            out += c.encode("utf-8"); i += 1; continue
        i += 1
        if i >= n: break
        e = s[i]; i += 1
        if e == "x":
            h = ""
            while i < n and len(h) < 2 and s[i] in "0123456789abcdefABCDEF":
                h += s[i]; i += 1
            if h: out.append(int(h, 16))
        elif e == "u":
            h = s[i:i+4]; i += 4
            try: out += chr(int(h, 16)).encode("utf-8")
            except ValueError: pass
        elif e in simples:
            out.append(simples[e])
        else:
            out += e.encode("utf-8")
    return out.decode("utf-8", "replace")

def chaves_da_tabela():
    txt = (RAIZ / "src/idioma_tab.h").read_text(encoding="utf-8")
    chaves = set()
    for linha in txt.splitlines():
        m = re.match(r'\s*\{\s*"((?:[^"\\]|\\.)*)"\s*,', linha)
        if m: chaves.add(decodificar(m.group(1)))
    return chaves

# Words and tokens that only appear in Portuguese. Does not serve to decide what
# to TRANSLATE (every drawn literal is interface); serves to pick, among the
# many snprintf in the code, which are UI strings and which are logs.
PT = re.compile(
    r"[áàâãéêíóôõúçÁÀÂÃÉÊÍÓÔÕÚÇ]|"
    r"\b(de|da|do|das|dos|em|para|com|sem|que|nao|não|uma|um|os|as|ao|à|"
    r"você|voce|seu|sua|todos|nenhum|nenhuma|mais|menos|ver|abrir|fechar|"
    r"episódio|episodio|temporada|fonte|fontes|legenda|legendas|assistir|"
    r"assistindo|reproduzir|adicionar|remover|buscar|carregando|erro|"
    r"agora|depois|antes|entre|sobre|desde|ainda|apenas|somente|"
    # Words WITHOUT accents that the app uses and have no stopwords in
    # phrase: "A seguir" and "Personalizar" went unnoticed in first version
    # of this list, and are two screens from photo in issue #12.
    r"seguir|personalizar|marcar|desmarcar|restaurar|padrao|padrão|opcoes|"
    r"opções|biblioteca|conta|perfil|tela|catalogo|catálogo|fileira|fileiras|"
    r"poster|pôster|destaque|amigos|historico|histórico|informacao|"
    r"informação|aguarde|selecionar|navegar|fechar|"
    # Words from the THREE strings leaked in v1.0.35 ("Conhecido por %s",
    # "%d curtidas", "0 a %d por dia"): none had stopwords from list
    # above, so gate 2 didn't see them and they reached TV in Portuguese.
    r"por|dia|dias|curtidas|conhecido|título|titulo|títulos|titulos|"
    r"coleção|colecao|gênero|genero|nudez|leve|moderado|severo)\b",
    re.IGNORECASE)

def eh_frase(s):
    """Looks like interface text? Discards raw format, id, url, JSON key."""
    if len(s) < 3: return False
    if s.startswith(("http", "/", "tt", "{", "[", "#")): return False
    # PATH AND URL assembled with %s. startswith alone does not catch "%s/catalogo.txt"
    # nor "luna://com.webos.media/%s", and after format rules allowed strings with
    # "%" in, those four became fixed noise.
    if "://" in s: return False
    if re.search(r"/[A-Za-z0-9._-]+\.[A-Za-z]{2,4}$", s): return False
    # RAW FORMAT vs STRING CONTAINING FORMAT, and the difference cost three
    # Portuguese strings in v1.0.35. The old rule discarded every ASCII literal
    # with "%" inside — and "Conhecido por  %s", "%d curtidas" and
    # "0 a %d por dia" are exactly that: unaccented, letters and a specifier.
    # Now strips specifiers and checks if words REMAIN; if none remain,
    # then it is a raw format ("%s/%s", "%d de %d").
    if "%" in s:
        resto = re.sub(r"%[-+0-9.*# ]*[a-zA-Z]", " ", s)
        if not re.search(r"[a-zA-ZáàâãéêíóôõúçÁ-Ú]{3}", resto): return False
    if not re.search(r"[a-zA-ZáàâãéêíóôõúçÁ-Ú]{3}", s): return False
    return True

# THIRD DEFECT FAMILY, found in player subtitle/audio menu
# (issues #41/#42): FUNCTION RETURNING LABEL ALREADY IN PORTUGUESE, called without
# enclosing i18n(). ling_nome() (linguas.c) is the measured case — returns
# "Português", "Inglês" etc from its own table, not from idioma_tab.h — but
# the list covers any such function that appears.
#
# WHY THE SCAN ABOVE DOES NOT CATCH THIS: that one looks at string literals.
# `ling_nome(f->idioma)` is not a literal, it's a call — text it returns
# exists only at runtime. A direct call to a drawing function
# (`txt_linha_corta(TXT_X, ling_nome(c), ...)`) still translates, because
# text.c applies i18n() ON TOP of what function returns, literal or not.
# The hole is the REVERSE: `ling_nome(...)` passed inside a snprintf/vsnprintf
# that ASSEMBLES a compound string ("%s  ·  %s") without passing that piece
# through i18n() first — the final string never matches any table key, and never
# will, so it remains in Portuguese forever. See video.c/video_tizen.c
# (audio track label) and addons.c (OpenSubtitles subtitle label):
# all three had exactly this gap.
FUNCOES_ROTULO_PT = ("ling_nome",)

RE_FUNC_ROTULO = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(FUNCOES_ROTULO_PT) + r")\s*\(")
# i18n( immediately before call: i18n(ling_nome(...)) is correct.
RE_I18N_ANTES = re.compile(r"i18n\s*\(\s*$")

# VERIFIED ONE BY ONE, each with reason — same discipline as IGNORAR above.
# Without this list section (3) cannot become a test (achar_rotulo_sem_i18n()
# alone cannot distinguish safe from forgotten).
IGNORAR_FUNCAO = {
    # The DEFINITION itself of ling_nome() in linguas.c contains "ling_nome(" in
    # header (`const char *ling_nome(const char *c) {`) — not a
    # CALL, and regex doesn't distinguish. Exists only once in this file.
    "linguas.c",
    # ajustes.c:167 keeps RAW ling_nome() on purpose: rotulosDeIdioma() runs
    # once when screen opens, not per frame. What truly translates is
    # desenhaLinha->txt_linha_corta, each frame, with CURRENT language —
    # exactly like other labels in this screen (V_QUALIDADE etc.), which
    # also stay in Portuguese in array and only become English at drawing. i18n()
    # here would lock list in the language of when screen opened.
    "ajustes.c:167",
}

def achar_rotulo_sem_i18n(txt, linha_de, ctx_fn, re_desenho, re_nao_e_tela, nome_arq):
    """Every call to a FUNCOES_ROTULO_PT that is neither i18n(...), nor a DIRECT
    argument to a drawing function, nor log/comparison — the three safe forms.
    The rest is a string that will be ASSEMBLED (snprintf) without translation,
    the gap in this section."""
    achados = []
    if nome_arq in IGNORAR_FUNCAO:
        return achados
    for m in RE_FUNC_ROTULO.finditer(txt):
        j = m.start()
        while j > 0 and txt[j-1] in " \t\n":
            j -= 1
        pedaco = txt[max(0, j-6):j]
        if RE_I18N_ANTES.search(pedaco):
            continue
        ctx = ctx_fn(txt, m.start())
        if re_desenho.search(ctx) or re_nao_e_tela.search(ctx):
            continue
        ln = linha_de(m.start())
        if f"{nome_arq}:{ln}" in IGNORAR_FUNCAO:
            continue
        achados.append((m.group(1), ln))
    return achados

# Calls whose text DOES NOT go to the screen: log, comparison, file, network.
# This list separates "[rede] OpenSSL locked for %d regions" (log, stays in
# Portuguese on purpose) from "Nenhuma fonte" (interface, must translate).
NAO_E_TELA = ("printf", "fprintf", "puts", "fputs", "perror", "marco",
              "strcmp", "strncmp", "strcasecmp", "strstr", "strchr", "strrchr",
              "getenv", "setenv", "fopen", "unlink", "remove", "rename",
              "mkdir", "system", "dlopen", "dlsym", "js_", "jsw_", "rede_",
              "curl_", "SDL_Log",
              # saved source log reason (fontevolta.h): never UI
              "fontevolta_", "addons_buscar", "cat_indice_por",
              "idioma_registrar", "assert", "_Static_assert",
              "EM_ASM", "MAIN_THREAD",
              # ICON NAME IS NOT TEXT. gfx_icone receives rasterized SVG basename
              # ("guia-fontes", "menu_home"), and Portuguese heuristic matched
              # "fontes" — tool was asking translation for a filename. Reporting
              # non-defects teaches ignoring the test, which is how this defect class survives.
              "gfx_icone", "gfx_icones_dir", "extras_caminho_marca_nome",
              "tex_obter", "tex_arquivo",
              # DEMO DATA from notification center (NUVIO_AVISOS_DEMO): mocks
              # what network returns, not a UI label.
              "demoAviso",
              # JSON READER in recomenda.c: objeto(r, "agora") is server response
              # key, not label.
              "objeto(")

RE_DESENHO = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(DESENHO) + r")\s*\(")

# i18n( right before literal. The author ALREADY STATED that it is UI — nothing
# left to guess afterwards.
RE_I18N = re.compile(r"(?<![A-Za-z0-9_])i18n\s*\(\s*$")

RE_NAO_E_TELA = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(re.escape(f) for f in NAO_E_TELA) + r")")

# HTTP HEADER IS NOT SCREEN TEXT, and its shape is unmistakable: "Name: value",
# with name in ASCII and hyphen. Needed because Portuguese heuristic matched
# parts of User-Agent — MAG250 in stalker.c sends "... ver: 2 rev: 250",
# and `ver` is in interface word list. A header flagged as UI text would make
# the suite report non-existent translation, and false positives teach ignoring tests.
RE_CABECALHO_HTTP = re.compile(r"^[A-Za-z][A-Za-z0-9-]{1,40}: ")

def contexto(txt, i):
    """The code snippet preceding literal up to start of statement."""
    j = i
    while j > 0 and txt[j-1] not in ";{}":
        j -= 1
    return txt[j:i]

# NOT INTERFACE, verified one by one. Each entry here is a decision, not an
# "unknown": without this list the tool cannot be a test, and without becoming
# a test it doesn't prevent regressions.
IGNORAR = {
    # RC 1.8 triage, each checked in the source:
    # - sync.c NOME[] are the labels of the "[sync] etapas" LOG line; "ver" is a
    #   field name in the jellyfin conta file; "forcé" is a pattern legref.c
    #   letreiro() matches against track names (comparison data, like "português").
    "biblioteca", "ver", "forcé",
    # - pluginjs.c error texts only go to the "[plugins] <nome>: ..." log line
    #   (plugins.c prints res.erro; no screen draws it).
    "cabecalhos maiores que o teto", "callback do fetch", "codigo do scraper",
    "orcamento de rede dos plugins esgotado", "resposta maior que o teto",
    "sem contexto (orcamento)", "sem memoria", "sem runtime (orcamento)",
    "prazo de %d ms",
    # - salvospainel.c: the format sits in a CtxExtra initializer and ctxmenu.c
    #   (ctx confirmation, i18n(x0->pergunta)) translates it; the key is in idioma_tab.h.
    "Remover %s dos amigos?",
    # App log (10/03): log area labels ([fonte], [legendas])
    # that registro.c looks for to group lines, icons passed to
    # drawing shortcuts, and proper names/files that are not translated.
    "fonte", "legendas", "aj_info", "aj_rotate-cw", "nuvio.log", "libcurl",
    "Mac", "webOS", "Samsung .wgt", "Samsung",
    # Glass UI Settings (10/03): "Mbps" unit is universal (same in
    # all languages); mockup row key from Settings capture
    # (ajustes_teste_quadro) and embedded art subfolder (ajArte) are not
    # UI text.
    "Mbps", "com.linvo.cinemeta_movie_top", "poster/",
    # Usage guide (10/03): mockup frame IDs in ajustesTesteGuia and
    # guide.json entry ID they point to. Only capture uses them; they
    # never go to screen.
    "guia-fontes", "guia-fileiras", "guia-legendas", "guia-vindo-do-whatsnew",
    "h-fileiras",
    # Word that Sources sheet LOOKS FOR in file name/description to
    # know if there is Portuguese audio (streams.c, idiomaDa). Comparison data,
    # never drawn text.
    "português",
    # Trailer diagnostic gate IDs are log codes, never UI labels.
    "dynamic-poster-hidden", "poster-wait",
    # Application identifiers; never presented as UI text.
    'br.com.claro-now',
    'br.com.claro.now.smarttvclient',
    'com.amazon.amazonvideo.livingroom',
    'com.apple.appletv',
    'com.apple.atve.androidtv.appletv',
    'com.disney.disneyplus',
    'com.disney.disneyplus-prod',
    'com.globo.globotv',
    'com.google.android.youtube.tv',
    'com.netflix.ninja',
    'com.wbd.stream',

    # Saved source watchdog reasons (fontevolta.c, fontevolta_decidir):
    # only go to log "[voltafonte] recuo para a busca: <motivo>".
    "erro do player", "clipe curto", "conferencia falhou",
    # Poster/meta provider hosts carrying config in path
    # (redeurl.c, rede_url_log): comparison data, never UI. "com" matched
    # Portuguese word list.
    "elfhosted.com", "ratingposterdb.com", "top-poster", "top-posters.com",
    "toposters.com",
    # Keyboard email shortcuts (teclado.c, #216): type address snippet,
    # identical in all languages.
    ".com", "@gmail.com", "@hotmail.com", "@outlook.com",
    # 1.7.2 card (novidades172.c): brand name, same in all languages, and
    # sample address typed in email login preview.
    "NUVIO LEGACY", "voce@gmail.com",
    # Part of printf "[tmdb] idioma dos metadados" (ajustes.c,
    # ajustes_tmdb_idioma_relatar): ternary sits on line without printf,
    # so NAO_E_TELA doesn't see it. Log only.
    "da interface",
    # Words in PUBLIC catalog name when it's a ranking; comparison
    # data in home.c (sinalRanking, Dynamic layout), never drawn text.
    # "top", "popular" and "trending" already don't match because ASCII.
    "em alta", "mais vist", "tendência",
    "-perfil",                      # filename suffix (ajustes.c)
    "biblioteca.h", "catalogo.h", "fileiras.h", "perfil.h", "legenda.h",
    "perfil.txt",                   # #include and filename
    "com.webos.media.client.nuvio", # LS2 client ID
    "abrir", "buscar", "erro",      # internal operation names
    "fontes", "legenda", "mais", "nao", "poster",  # internal keys, not labels
    "fileiras", "ordem-da-conta",  # context parts in homeestado.c log
    # reason for GPU step down in log "[gpu-nivel] nivel 1 -> 2 (...)" (gpunivel.c aplicar)
    "GPU presa mesmo com efeitos leves: efeitos minimos",
    # Stop points in condemned loop (descoberta.c, CONDENADA): log only.
    "antes de pedir os catalogos",
    "depois do continuar assistindo", "depois dos catalogos",
    "depois dos manifestos", "esperando os catalogos", "lendo os manifestos",
    # Reasons and origins of GPU level (gpunivel.c): only go to nuvio.log.
    "definido por gpun_definir_nivel", "nenhum", "padrao", "sem alvo interno",
    "teto de tempo de medida",
    "lento pela CPU ou pelo resto, nao pela GPU: menos pixel nao ajuda",
    # .tpk: only go to nuvio.log / compile error / filename.
    "sem recorte em 2 s, mostra assim", "sem tocando, mostra assim",
    "sem recorte",
    "eglMakeCurrent no fio do app falhou: 0x%x",
    "crédit", "crédito",            # word searched in MKV chapter
    "episodio", "episódio",         # word searched in TMDB video name (extras.c)
    "película",                     # support word for Spanish headlines search (noticias.c, PAR)
    "seriál",                       # ditto in Czech and Slovak (noticias.c, PAR)
    # NATIVE language name in selector (ajustes.c, V_IDIOMA): without i18n
    # on purpose, users who switched to unreadable language need to find theirs.
    "Türkçe",
    # "Hello" written in each language (novidades160.c, OLA): language scene
    # displays each language in itself, without translation on purpose.
    "Olá", "Xin chào",
    # #158: data, not label. Modifier letters table (U+1D2C..)
    # from channel name normalization (epg.c) and word searched in Xtream
    # category name to find country grid (guia.c).
    "a?b?de?ghijklmn?o?prtuw", "românia",
    # Proper name and acronym: identical in both languages.
    "IMDb", "Trakt", "YouTube", "PIN", "AI-powered", "NUVIO",
    # Accents -> base letter table for title normalization (trailerapple.c):
    # data, not label.
    "ÀÁÂÃÄÅàáâãäåÈÉÊËèéêëÌÍÎÏìíîïÒÓÔÕÖØòóôõöøÙÚÛÜùúûüÝýÿÑñÇç",
    # ISO 639-2 for Portuguese (linguas.c). Became candidate when "por" entered
    # Portuguese word list; language code, not label.
    "por",
    # SAME CODE IN UPPERCASE (fontepref.c). Value of TERMOS table,
    # translating "portugues"/"brazilian"/"nacional" to language code to
    # COMPARE two sources. Never reaches screen: entire signature
    # ("BR+DUB+POR") only appears in diagnostic printf.
    "POR",
    # Not UI: preferences file header and two log lines
    # that Portuguese heuristic cannot distinguish from phrases.
    "# Fileiras da Home, escolha DESTE aparelho. Nunca e enviada para\n"
    "# a conta nem para o Trakt.\n",
    "%d addons · %d progressos · %d vistos · %d na lista · %d coleções%s",
    # #215: two variants of sync summary above (account server down),
    # local copy filename (contacache.c) and notification key in island (app.c) —
    # internal key, not label.
    "servidor da conta fora do ar (HTTP %d) · usando a cópia de %s",
    "servidor da conta fora do ar (HTTP %d) · sem cópia salva",
    "conta-%s-p%d.json", "conta-fora",
    "conta-addons-pend-",  # offline queue file prefix, never UI text
    "hdr do pipeline: %s (fonte DV=%d)",
    # Three video.c/video_tizen.c milestones. Buffer is assembled in one statement
    # and delivered to marco() in NEXT, so marco — which is already in
    # NAO_E_TELA — is outside context read by scan. Log messages.
    "pipeline erro: %.60s",
    # Header marker of mkvass font sidecar (file on disk) and
    # assrender status line, which only goes to log. "fontes" matched
    # Portuguese word list; neither reaches screen.
    "NVASS-FONTES-1\n",
    "%s; eventos=%d fontes=%d fontselect=%d cobertura=%lld-%lldms render=%lluus quadro=%zuB",
    "mkv: %d faixas lidas, %d legendas com idioma",
    "seek para %ds",
    # Four diagnostic reasons in [col] (descoberta.c). ARRAY DATA,
    # not printf argument: scan looks at what precedes literal
    # and there is only an initializer brace, so NAO_E_TELA cannot
    # recognize them. Kept in Portuguese on purpose — for issue reports.
    "nenhuma colecao usa este addon",
    "colecao tem o addon, mas com outro tipo",
    "colecao tem addon e tipo, mas outro id de catalogo",
    "CASOU — nao engoliu porque o grupo esta oculto",
    # motivoTV() reasons (faixas.c, #92): why ASS track stayed with
    # TV instead of app overlay. Function `return "..."` and only go
    # to printf "[legenda] faixa N -> TV: motivo" — log for issue reports.
    # #92, v1.4.7: if VIDEO was already open when mkvass Range failed
    # (momentoVideo, mkvass.c), and reason for orphan video in pre-search line (player.c).
    # Both only go to diagnostic printf.
    "antes do video",
    "com video aberto",
    "teto vencido, o resto segue em segundo plano",
    "faixa inexistente",
    "sem URL da fonte",
    "mkvass ja desistiu desta faixa nesta sessao",
    "fonte nao e MKV (nao ha sonda)",
    "sonda do cabecalho ainda nao voltou",
    "sonda voltou sem par para esta faixa (ver [mkv] legendas da TV x arquivo)",
    "codec nao e ASS/SSA: a TV desenha bem",
    "sem ordinal no arquivo",
    # motivoNoGo() reasons (faixas.c): same case, for line "[legenda]
    # mkvass no-go N (motivo)". What goes to SCREEN are warnings with i18n().
    "nao e MKV",
    "servidor sem Range",
    "faixa nao e ASS",
    "sem indice da faixa",
    "sem CueRelativePosition",
    # Worker JSON key (noticia.c, /v1/noticia) and HTML class fragment
    # discarded by scraper (leitura.c, "saiba-mais"): data, not label.
    '"titulo"', "saiba-mais",
    # novidades20.c: texts from CAP[] table (and summary list B[]) with "%"
    # in middle of sentence ("200%"). Not snprintf formats: they are keys, and
    # drawing passes each through i18n() (novidades20.c, lines of `i18n(e->t)`).
    # All have entries in idioma_tab.h. "poster/%s.jpg" is a filepath.
    "Volume até 200%", "Tamanho padrão 80%", "Continuar assistindo · 58%",
    "Cache de busca e volume até 200%", "poster/%s.jpg",
}

def sem_corpo_em_js(txt):
    """Erases EM_JS/EM_ASM body, replacing with spaces and keeping \n.

    The body of an EM_JS is JAVASCRIPT written inside a .c file, and this
    scanner reads C. Single quotes in JS ("don't", 'text') are not character
    literals, and the string literals inside are bridge code —
    never UI text, because drawing is on the C side.

    Without this, every comment or new string inside EM_JS of video_tizen.c
    became a "drawn literal without key in table". Happened on 09/17 with
    nine findings at once, all false, and real signals get lost in
    such noise.

    Replaces with spaces instead of removing so offsets — and therefore
    line numbers across rest of file — remain valid.
    """
    saida = list(txt)
    for m in re.finditer(r"(?<![A-Za-z0-9_])EM_(?:JS|ASM|ASM_INT|ASM_DOUBLE)\s*\(", txt):
        i = m.end() - 1          # at "(" opener
        nivel, j = 0, i
        while j < len(txt):
            if txt[j] == "(": nivel += 1
            elif txt[j] == ")":
                nivel -= 1
                if nivel == 0: break
            j += 1
        for k in range(m.start(), min(j + 1, len(txt))):
            if saida[k] != "\n":
                saida[k] = " "
    return "".join(saida)


def varrer():
    chaves = chaves_da_tabela()
    faltando_tabela, faltando_i18n = {}, {}
    faltando_funcao = {}
    src = RAIZ / "src"
    arquivos = sorted(src.glob("*.c")) + [
        src / nome for nome in ("ajustes_ux_tela.inc", "ajustes_ux_interacao.inc",
                                "ajustes_ux_desenho.inc", "ajustes_ux_ilha.inc",
                                "registro_ilha.inc", "registro_envio.inc") if (src / nome).exists()
    ]
    for arq in sorted(arquivos):
        if arq.name == "idioma.c":
            continue
        txt = arq.read_text(encoding="utf-8")
        txt = sem_corpo_em_js(txt)
        # line number by offset
        quebras = [0]
        for k, c in enumerate(txt):
            if c == "\n":
                quebras.append(k)
        def linha_de(off):
            lo, hi = 0, len(quebras) - 1
            while lo < hi:
                mid = (lo + hi + 1) // 2
                if quebras[mid] <= off: lo = mid
                else: hi = mid - 1
            return lo + 1

        for nome, ln in achar_rotulo_sem_i18n(txt, linha_de, contexto, RE_DESENHO,
                                              RE_NAO_E_TELA, arq.name):
            faltando_funcao.setdefault(nome, []).append(f"{arq.name}:{ln}")

        # Outside comments and outside #include: iterate and grab each block of
        # adjacent literals once.
        i, n = 0, len(txt)
        while i < n:
            c = txt[i]
            if c == "/" and i + 1 < n and txt[i+1] == "/":
                i = txt.find("\n", i);  i = n if i < 0 else i;  continue
            if c == "/" and i + 1 < n and txt[i+1] == "*":
                i = txt.find("*/", i);  i = n if i < 0 else i + 2;  continue
            if c == "'":
                i += 3 if txt[i+1:i+2] != "\\" else 4;  continue
            if c != '"':
                i += 1;  continue
            ini = i
            bruto, i = literais_colados(txt, i)
            s = decodificar(bruto)
            if not eh_frase(s):
                continue
            if ("gl_FragColor" in s or "uniform " in s or "varying " in s
                    or "void main()" in s):
                continue                      # shader code (gfx.c)
            ctx = contexto(txt, ini)
            # TWO LAYERS, and the difference between them already caused a regression
            # in this tool.
            #
            # Literal delivered DIRECTLY to a drawing function is UI by
            # definition: no heuristics needed, either it is in the table or in
            # IGNORAR. By requiring "looks like Portuguese" here too,
            # "↑ Voltar aos filtros · Voltar: menu" (biblioteca.c) — which
            # the first version caught — went unnoticed again.
            #
            # Outside drawing (snprintf into a buffer that will be drawn) context
            # alone cannot distinguish screen from log, and then Portuguese
            # heuristic applies.
            #
            # THIRD GATE, which exists due to a measured case: a literal
            # inside i18n() that DOES NOT go directly to a drawing function
            # (typically `snprintf(buf, size, "%s", i18n("..."))`) was only
            # checked if Portuguese heuristic liked it. It didn't like
            # "Setas: mover  ·  OK: entrar" — lacking obvious Portuguese markers —
            # and the key appeared on screen in Portuguese across English UI.
            # Writing i18n( IS the declaration that it is interface text;
            # guessing language afterwards only causes errors.
            direto = (RE_DESENHO.search(ctx) is not None
                      or RE_I18N.search(ctx) is not None)
            if not direto and not PT.search(s):
                continue
            # BY FULL NAME, not substring: "printf" matches inside
            # "snprintf", which is how "A seguir" (posplay.c) — one of the
            # screens in issue #12 photo — slipped through in this very tool.
            # All text assembled with snprintf was being silently discarded.
            if RE_NAO_E_TELA.search(ctx):
                continue
            if RE_CABECALHO_HTTP.match(s):
                continue
            onde = f"{arq.name}:{linha_de(ini)}"
            # ASSEMBLY: format must be wrapped at origin, because the final
            # string never matches a key. Without "%", literal reaches screen
            # as a whole and key alone suffices.
            if "%" in s and "i18n" not in ctx.split("(")[0][-40:] and "i18n" not in ctx:
                if s not in IGNORAR:
                    faltando_i18n.setdefault(s, []).append(onde)
            elif s not in chaves and s not in IGNORAR:
                faltando_tabela.setdefault(s, []).append(onde)
    return faltando_tabela, faltando_i18n, faltando_funcao

if __name__ == "__main__":
    tab, fmt, func = varrer()
    if "--tabela" in sys.argv or "--table" in sys.argv:
        for s in sorted(tab): print('  { "%s", "" },' % s.replace('"', '\\"'))
        sys.exit(0)
    print("=== (1) literal drawn WITHOUT key in table: %d ===" % len(tab))
    for s in sorted(tab):
        print("  %-72s %s" % (repr(s)[:72], ", ".join(tab[s][:3])))
    print()
    print("=== (2) snprintf in Portuguese WITHOUT i18n in format: %d ===" % len(fmt))
    for s in sorted(fmt):
        print("  %-72s %s" % (repr(s)[:72], ", ".join(fmt[s][:3])))
    print()
    print("=== (3) function returning label in Portuguese, called without i18n(): %d ===" % len(func))
    for s in sorted(func):
        print("  %-72s %s" % (s, ", ".join(func[s][:6])))
    sys.exit(1 if (tab or fmt or func) else 0)
