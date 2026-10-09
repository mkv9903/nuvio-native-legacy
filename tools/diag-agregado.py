#!/usr/bin/env python3
"""THE BEST SCENARIO FOR EVERYONE: aggregates diagnostic reports.

Reads `diagnostico=v1|v2` reports sent by TVs (table `registro`
in D1 nuvio-recomendacoes, via wrangler, READ ONLY) and aggregates by platform
and RAM range — the same ranges as the table in src/perfiltv.c:

  - average time per artwork for each artwork source (query + download), failures,
    and in how many titles it repeated the card image (same_as_card);
  - texture/threads/hero profile: baseline, candidate, and outcome
    (applied, kept, restored, already in profile);
  - and PROPOSES the table row in perfiltv.c and the default hero source per
    platform (the fastest among those that do not repeat the card).

With few reports the proposal states this instead of inventing numbers: the rule
is the same as always in the app, "do not claim without measuring".

  python3 tools/diag-agregado.py                 # queries D1 (wrangler)
  python3 tools/diag-agregado.py --file x.json   # reads a local dump
  python3 tools/diag-agregado.py --json          # JSON output

Never prints user identity, token, or addon list: only platform, version,
RAM and numbers. Wrangler uses its own login; no credentials pass through here.
"""
import argparse
import json
import os
import subprocess
import sys
from collections import defaultdict

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVIDOR = os.path.join(RAIZ, "servidor", "recomendacoes")
# `pessoa` comes only to COUNT distinct devices (hashed here and never
# printed): three reports from the same TV are not "everyone".
CONSULTA = ("SELECT id, pessoa, plataforma, versao, criado, texto FROM registro "
            "WHERE texto LIKE 'diagnostico=v%' ORDER BY id")

# Background sources, in order of ARTEHERO_* contract (logo is not a background).
FUNDOS = ["catalog", "metahub", "tmdb", "trakt", "apple", "fanart", "anime", "tmdb_outro"]
AJUSTE_DE = {"catalog": 1, "metahub": 2, "tmdb": 3, "trakt": 4, "apple": 5,
             "fanart": 6, "anime": 7, "tmdb_outro": 3}
ROTULO_AJUSTE = {0: "Automatic", 1: "Catalog", 2: "Metahub", 3: "TMDB", 4: "Trakt",
                 5: "Apple TV", 6: "fanart.tv", 7: "Anime"}
MIN_APARELHOS = 3


def tabela_atual(plataforma, mem):
    """Mirror of ptv_tex_auto_mb/ptv_tex_teto_mb/ptv_padrao (src/perfiltv.c)."""
    tizen = plataforma.lower().startswith("tizen") or plataforma.lower() == "samsung"
    if tizen:
        auto = 96 if not mem else 64 if mem <= 1024 else 96 if mem < 4096 else 128
        return {"tex_mb": auto, "teto_mb": auto, "fios_rede": 2, "heroi": 1280}
    if not mem:
        return {"tex_mb": 96, "teto_mb": 160, "fios_rede": 4, "heroi": 1920}
    # 09/23: replica previously said 64/96 below 1.2 GB (C dropped to
    # 48/64 in be3d473) and thus proposed 96 for the range. 1.2-2 GB: 128.
    auto = 48 if mem < 1200 else 128 if mem < 3000 else 192
    teto = 64 if mem < 1200 else 160 if mem < 2000 else 300 if mem < 3000 else 512
    return {"tex_mb": auto, "teto_mb": teto, "fios_rede": 2 if mem < 1200 else 4,
            "heroi": 1280 if mem < 1200 else 1920}


def faixa(plataforma, mem):
    """The RAM range of perfiltv.c (ptv_tex_auto_mb)."""
    tizen = plataforma.lower().startswith("tizen") or plataforma.lower() == "samsung"
    if not mem:
        return ("tizen" if tizen else "lg") + " no RAM"
    if tizen:
        return "tizen <=1GB" if mem <= 1024 else "tizen <4GB" if mem < 4096 else "tizen >=4GB"
    if mem < 800:
        return "lg <800MB"
    if mem < 1200:
        return "lg <1.2GB"
    if mem < 2000:
        return "lg <2GB"
    if mem < 3000:
        return "lg <3GB"
    return "lg >=3GB"


def ler_d1():
    cmd = ["npx", "--yes", "wrangler", "d1", "execute", "nuvio-recomendacoes",
           "--remote", "--json", "--command", CONSULTA]
    p = subprocess.run(cmd, cwd=SERVIDOR, capture_output=True, text=True, timeout=180)
    if p.returncode != 0:
        # wrangler stderr might contain config path; show last line only.
        ult = (p.stderr.strip().splitlines() or ["?"])[-1]
        sys.exit("diag-agregado: wrangler failed: %s" % ult[:200])
    saida = p.stdout[p.stdout.find("["):]
    return json.loads(saida)[0]["results"]


def ler_arquivo(caminho):
    d = json.load(open(caminho, encoding="utf-8"))
    if isinstance(d, list) and d and "results" in d[0]:
        return d[0]["results"]
    return d


def campos_pipe(resto):
    """'catalog|ok=3|falhas=0' -> ('catalog', {'ok': '3', ...})"""
    partes = resto.split("|")
    kv = {}
    for p in partes[1:]:
        if "=" in p:
            k, v = p.split("=", 1)
            kv[k] = v
        elif ":" in p:
            k, v = p.split(":", 1)
            kv[k] = v
    return partes[0], kv


def num(v, padrao=0):
    try:
        return int(v)
    except (TypeError, ValueError):
        return padrao


def analisar(texto):
    r = {"fontes": {}, "sugestao": None}
    for linha in texto.splitlines():
        if "=" not in linha:
            continue
        k, v = linha.split("=", 1)
        if k == "arte_fonte":
            nome, kv = campos_pipe(v)
            r["fontes"][nome] = kv
        elif k == "sugestao_destaque":
            if v != "-":
                # alvo:x|diferente:1|lenta:y|ms_lenta=1|base:z|ms_base=2|motivo:m
                kv = {}
                for p in v.split("|"):
                    sep = ":" if ":" in p and ("=" not in p or p.index(":") < p.index("=")) else "="
                    if sep in p:
                        a, b = p.split(sep, 1)
                        kv[a] = b
                r["sugestao"] = kv
        elif k in ("perfil_antes", "perfil_candidato"):
            r[k] = [num(x) for x in v.split("|")]
        elif k == "addon":
            continue          # user addons list: excluded from aggregation
        else:
            r[k] = v
    return r


def agregar(linhas):
    grupos = defaultdict(lambda: {
        "n": 0, "versoes": set(), "ids": [],
        "fontes": defaultdict(lambda: {"ok": 0, "falhas": 0, "ms": 0, "iguais": 0,
                                       "medidas": 0, "relatorios": 0, "bytes": 0,
                                       "larg": 0, "alt": 0}),
        "aplicacao": defaultdict(int), "perfil_antes": [], "perfil_candidato": [],
        "tex_bytes": [], "tex_limite": [], "tex_pendentes": [], "tex_quentes": [],
        "artes_antes_ms": [], "artes_depois_ms": [], "pior_quadro_antes_ms": [],
        "destaque": defaultdict(int), "sugestoes": [], "modos": defaultdict(int),
        "aparelhos": set(), "mem": [], "despejos": [],
    })
    for linha in linhas:
        txt = linha.get("texto") or ""
        if not txt.startswith("diagnostico=v"):
            continue
        r = analisar(txt)
        plat = (linha.get("plataforma") or "?").strip() or "?"
        mem = num(r.get("mem_total_mb"))
        g = grupos[(plat, faixa(plat, mem))]
        g["n"] += 1
        g["aparelhos"].add(hash(linha.get("pessoa") or linha.get("id")))
        if mem:
            g["mem"].append(mem)
        # despejos_quentes_antes/_sessao only exist since 09/23 (num() returns 0).
        g["despejos"].append(num(r.get("despejos_quentes_depois")) + num(r.get("tex_quentes")) +
                             num(r.get("despejos_quentes_antes")) + num(r.get("despejos_quentes_sessao")))
        g["versoes"].add(linha.get("versao") or r.get("versao") or "?")
        g["ids"].append(linha.get("id"))
        g["modos"][r.get("modo", "?")] += 1
        g["aplicacao"][r.get("aplicacao", "?")] += 1
        for k in ("perfil_antes", "perfil_candidato"):
            if k in r:
                g[k].append(r[k])
        for k in ("tex_bytes", "tex_limite", "tex_pendentes", "tex_quentes",
                  "artes_antes_ms", "artes_depois_ms", "pior_quadro_antes_ms"):
            if k in r:
                g[k].append(num(r[k]))
        if "destaque_fonte" in r:
            g["destaque"]["%s/%s" % (r["destaque_fonte"], r.get("destaque_diferente", "?"))] += 1
        if r["sugestao"]:
            g["sugestoes"].append(r["sugestao"])
        for nome, kv in r["fontes"].items():
            f = g["fontes"][nome]
            ok, fal = num(kv.get("ok")), num(kv.get("falhas"))
            if not ok and not fal:
                continue          # not measured on this device (no key, no anime...)
            f["relatorios"] += 1
            f["ok"] += ok
            f["falhas"] += fal
            f["ms"] += num(kv.get("resolve_ms")) + num(kv.get("download_ms"))
            f["bytes"] += num(kv.get("bytes"))
            f["larg"] = max(f["larg"], num(kv.get("largura")))
            f["alt"] = max(f["alt"], num(kv.get("altura")))
            # old v2 does not have iguais/igual_ao_card: remains unknown (-1).
            if "iguais" in kv or "igual_ao_card" in kv:
                f["medidas"] += 1
                f["iguais"] += num(kv.get("iguais"), num(kv.get("igual_ao_card")))
    return grupos
    return grupos


def media(v):
    return sum(v) / len(v) if v else None


def propor(g, plat):
    """Proposal of perfiltv.c table row and hero source."""
    prop = {}
    n = g["n"]
    # Hero source: fastest per art, without majority failure.
    candidatas = []
    for nome in FUNDOS:
        f = g["fontes"].get(nome)
        if not f or f["ok"] <= 0:
            continue
        ms = f["ms"] / f["ok"]
        taxa = f["falhas"] / (f["ok"] + f["falhas"])
        candidatas.append((ms, nome, taxa, f))
    candidatas.sort()
    mesma = [c for c in candidatas if c[2] < 0.5]
    diferentes = [c for c in mesma if c[3]["medidas"] > 0 and c[3]["iguais"] == 0]
    prop["destaque_mesma_foto"] = mesma[0][1] if mesma else None
    prop["destaque_outra_arte"] = diferentes[0][1] if diferentes else None
    if not any(c[3]["medidas"] for c in mesma):
        prop["destaque_outra_arte_obs"] = ("no report with same_as_card yet "
                                           "(only new version measures): no proposal")
    # Texture profile. A cache FILLS up to the budget (C9 with 300 MB
    # reaches ~200 MB in a long session), so peak is not requirement. The
    # signal of shortfall is VISIBLE art evicted (tex_quentes /
    # despejos_quentes_depois): without any, table stays; with some, one
    # step up, within the range ceiling.
    tab = tabela_atual(plat, max(g["mem"]) if g["mem"] else 0)
    prop["tabela_atual"] = tab
    usados = [b / 1048576.0 for b in g["tex_bytes"] if b]
    if usados:
        prop["tex_pico_mb"] = round(max(usados), 1)
    escada = [48, 64, 96, 128, 160, 192, 240, 300, 400, 512]
    if any(g["despejos"]):
        acima = [e for e in escada if e > tab["tex_mb"] and e <= tab["teto_mb"]]
        prop["tex_mb"] = acima[0] if acima else tab["tex_mb"]
        prop["tex_motivo"] = "visible art evicted in %d report(s)" % sum(1 for x in g["despejos"] if x)
    else:
        prop["tex_mb"] = tab["tex_mb"]
        prop["tex_motivo"] = "no visible art evicted in %d report(s)" % n
    # Names from aplicacaoNome() in diagnostico.c.
    aplicados = g["aplicacao"].get("aplicada", 0)
    restaurados = sum(v for k, v in g["aplicacao"].items() if k.startswith("restaurada"))
    iguais = g["aplicacao"].get("ja_no_perfil", 0)
    # mantido_ruido (09/23): tested, did not degrade, but without gain beyond margin
    # of ptv_decidir; previous returned. Counted separately: not "same" nor
    # "restored due to worsening".
    ruido = g["aplicacao"].get("mantido_ruido", 0)
    if not aplicados and not restaurados and not ruido:
        prop["perfil"] = ("keep table (candidate matches profile in %d of %d)" % (iguais, n)
                          if iguais else "keep table (no candidate tested)")
    else:
        prop["perfil"] = "candidate kept in %d, restored in %d, within noise in %d of %d" % (
            aplicados, restaurados, ruido, n)
    if g["perfil_antes"]:
        ult = g["perfil_antes"][-1]
        prop["perfil_visto"] = {"tex_mb": ult[0], "fios_rede": ult[1], "heroi": ult[2]}
    prop["aparelhos"] = len(g["aparelhos"])
    prop["suficiente"] = len(g["aparelhos"]) >= MIN_APARELHOS
    return prop


def imprimir(grupos):
    total = sum(g["n"] for g in grupos.values())
    print("diagnostic reports: %d in %d group(s) (platform x RAM)" % (total, len(grupos)))
    if not total:
        return
    for (plat, fx), g in sorted(grupos.items()):
        print()
        print("== %s | %s | %d report(s) from %d device(s) | versions %s | ids %s" % (
            plat, fx, g["n"], len(g["aparelhos"]), ",".join(sorted(g["versoes"])),
            ",".join(str(i) for i in g["ids"][-6:])))
        print("  modes: %s | application: %s" % (dict(g["modos"]), dict(g["aplicacao"])))
        if g["perfil_antes"]:
            print("  profile before (tex|threads|hero): %s" % ["|".join(map(str, p)) for p in g["perfil_antes"]])
            print("  candidate profile:                 %s" % ["|".join(map(str, p)) for p in g["perfil_candidato"]])
        if g["tex_bytes"]:
            print("  textures: peak %.1f MB of %.0f MB | pending %s | hot %s" % (
                max(g["tex_bytes"]) / 1048576.0,
                max(g["tex_limite"] or [0]) / 1048576.0,
                max(g["tex_pendentes"] or [0]), max(g["tex_quentes"] or [0])))
        if g["artes_antes_ms"]:
            print("  art via cache (hot): before %.0f ms, after %.0f ms, worst frame %.0f ms" % (
                media(g["artes_antes_ms"]), media([x for x in g["artes_depois_ms"] if x]) or 0,
                media(g["pior_quadro_antes_ms"]) or 0))
        print("  hero (source/different): %s" % dict(g["destaque"]))
        print("  %-11s %9s %7s %8s %10s %10s %s" % ("source", "ms/art", "ok", "failures", "KB/art", "max", "same as card"))
        for nome in FUNDOS + ["logo"]:
            f = g["fontes"].get(nome)
            if not f or (not f["ok"] and not f["falhas"]):
                continue
            ms = "%.0f" % (f["ms"] / f["ok"]) if f["ok"] else "-"
            kb = "%.0f" % (f["bytes"] / f["ok"] / 1024.0) if f["ok"] else "-"
            igual = ("%d title(s) in %d rep." % (f["iguais"], f["medidas"])) if f["medidas"] else "not measured"
            if nome == "logo":
                igual = "-"
            print("  %-11s %9s %7d %8d %10s %10s %s" % (nome, ms, f["ok"], f["falhas"], kb,
                                                     "%dx%d" % (f["larg"], f["alt"]) if f["larg"] else "-", igual))
        for s in g["sugestoes"]:
            print("  sent suggestion: %s" % "|".join("%s:%s" % kv for kv in s.items()))
        p = propor(g, plat)
        print("  PROPOSAL%s:" % ("" if p["suficiente"] else
              " (%d device(s): indicative, do not change table before %d)" % (p["aparelhos"], MIN_APARELHOS)))
        tab = p["tabela_atual"]
        print("    perfiltv.c today in this range: texture %d MB (cap %d), %d threads, hero %d" % (
            tab["tex_mb"], tab["teto_mb"], tab["fios_rede"], tab["heroi"]))
        print("    perfiltv.c proposed: texture %d MB, %d threads, hero %d — %s%s" % (
            p["tex_mb"], tab["fios_rede"], tab["heroi"], p["tex_motivo"],
            "" if "tex_pico_mb" not in p else "; peak used %.1f MB" % p["tex_pico_mb"]))
        if p.get("perfil_visto"):
            print("    active profile on device: %d MB, %d threads, hero %d" % (
                p["perfil_visto"]["tex_mb"], p["perfil_visto"]["fios_rede"], p["perfil_visto"]["heroi"]))
        print("    profile: %s" % p["perfil"])
        ms = p["destaque_mesma_foto"]
        print("    hero, same photo as card (default): %s" % (
            "%s (Hero background = %s)" % (ms, ROTULO_AJUSTE[AJUSTE_DE[ms]]) if ms else "not measured"))
        od = p["destaque_outra_arte"]
        if od:
            print("    hero with different artwork: %s (Hero background = %s)" % (od, ROTULO_AJUSTE[AJUSTE_DE[od]]))
        else:
            print("    hero with different artwork: %s" % p.get("destaque_outra_arte_obs", "no source different from card measured"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--arquivo", "--file", dest="arquivo", help="local JSON dump (wrangler --json output or line list)")
    ap.add_argument("--json", action="store_true", help="JSON output")
    a = ap.parse_args()
    linhas = ler_arquivo(a.arquivo) if a.arquivo else ler_d1()
    grupos = agregar(linhas)
    if a.json:
        out = []
        for (plat, fx), g in sorted(grupos.items()):
            out.append({"plataforma": plat, "faixa": fx, "n": g["n"], "aparelhos": len(g["aparelhos"]),
                        "fontes": {k: dict(v) for k, v in g["fontes"].items()},
                        "aplicacao": dict(g["aplicacao"]), "proposta": propor(g, plat)})
        print(json.dumps(out, ensure_ascii=False, indent=1))
    else:
        imprimir(grupos)


if __name__ == "__main__":
    main()
