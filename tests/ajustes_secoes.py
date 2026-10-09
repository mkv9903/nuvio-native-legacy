"""Cobertura do catalogo UX executavel (ver tests/ajustes_secoes.sh)."""
import argparse
import re
import sys
from pathlib import Path


parser = argparse.ArgumentParser()
parser.add_argument("fonte", help="src/ajustes.c, de onde vem o enum OpcaoId")
parser.add_argument(
    "--catalogo",
    default=str(Path(__file__).resolve().parents[1] / "src/ajustes_ux_tela.inc"),
    help="include do catalogo; padrao src/ajustes_ux_tela.inc",
)
args = parser.parse_args()
fonte = Path(args.fonte).read_text(encoding="utf-8")
catalogo = Path(args.catalogo).read_text(encoding="utf-8")

enum = re.search(r"typedef enum \{([^}]*?)\}\s*OpcaoId;", fonte, re.S)
if not enum:
    sys.exit("FALHA: enum OpcaoId nao encontrado em " + args.fonte)
corpo = re.sub(r"//[^\n]*", "", enum.group(1))
opcoes = [n.strip() for n in corpo.split(",") if n.strip() and n.strip() != "AJ_N"]
# Retired from the screen on purpose; the enum slot stays (positional valor[]/CHAVE[]).
RETIRADAS = {"AJ_RELOGIO_POS"}  # 1.8: clock island is always top-right
opcoes = [op for op in opcoes if op not in RETIRADAS]

tabela = re.search(r"static const Item TELA\[\] = \{(.*?)\n\};", catalogo, re.S)
if not tabela:
    sys.exit("FALHA: static const Item TELA[] nao encontrado em " + args.catalogo)
corpo_tela = re.sub(r"//[^\n]*", "", tabela.group(1))
itens = re.findall(r'\b(SEC|GRP|ROT|OPC)\(\s*("(?:[^"\\]|\\.)*"|AJ_[A-Z_0-9]+)', corpo_tela)

falhas = []
if not itens:
    falhas.append("nenhum item encontrado em TELA[]")
elif itens[0][0] != "SEC":
    falhas.append("TELA[] nao comeca por uma categoria (SEC)")
if any(t == "GRP" for t, _ in itens):
    falhas.append("TELA[] deve usar apenas blocos fixos ROT, sem GRP recolhivel")

na_tela = [v for t, v in itens if t == "OPC"]
for op in opcoes:
    n = na_tela.count(op)
    if n == 0:
        falhas.append("%s nao aparece em TELA[]: a opcao some da TV" % op)
    elif n > 1:
        falhas.append("%s aparece %d vezes em TELA[]" % (op, n))
for op in na_tela:
    if op not in opcoes:
        falhas.append("TELA[] cita %s, que nao existe no enum" % op)

# Categorias e blocos ROT precisam conter ao menos uma opcao.
abertos = {}
for t, v in itens + [("SEC", "<fim>")]:
    fecha = ("SEC", "ROT") if t == "SEC" else ("ROT",) if t == "ROT" else ()
    for tipo in fecha:
        if tipo in abertos:
            nome, n = abertos.pop(tipo)
            if n == 0:
                falhas.append("%s %s vazio(a) em TELA[]" % (tipo, nome))
    if t in ("SEC", "ROT"):
        abertos[t] = [v, 0]
    elif t == "OPC":
        for tipo in abertos:
            abertos[tipo][1] += 1

n_sec = sum(1 for t, _ in itens if t == "SEC")
if n_sec != 12:
    falhas.append("esperadas 12 categorias, encontradas %d" % n_sec)

ajuda = re.search(r"static const char \*SECAO_AJUDA\[\] = \{(.*?)\n\};", catalogo, re.S)
if not ajuda:
    falhas.append("SECAO_AJUDA[] nao encontrado no include")
else:
    n_ajuda = len(re.findall(r'^\s*"', ajuda.group(1), re.M))
    if n_ajuda != n_sec:
        falhas.append("%d categorias e %d frases em SECAO_AJUDA" % (n_sec, n_ajuda))

# A classificação avançada é explícita; todo caso não listado permanece básico.
funcao = re.search(r"static int uxAvancada\(int op\)\s*\{(.*?)\n\}", catalogo, re.S)
if not funcao:
    falhas.append("uxAvancada(int op) nao encontrado no include")
else:
    avancadas = re.findall(r"\bcase\s+(AJ_[A-Z_0-9]+)\s*:", funcao.group(1))
    for op in avancadas:
        if op not in opcoes:
            falhas.append("uxAvancada cita %s, que nao existe no enum" % op)
    if len(avancadas) != len(set(avancadas)):
        falhas.append("uxAvancada repete um ID avançado")
    if "AJ_SEEKR_AJUSTE" not in avancadas:
        falhas.append("AJ_SEEKR_AJUSTE deve permanecer avancada")
    if "AJ_SAIDA_PLAYER" in avancadas:
        falhas.append("AJ_SAIDA_PLAYER deve permanecer basica")

if falhas:
    for f in falhas:
        print("FALHA: " + f)
    sys.exit(1)

print("PASS: %d opcoes de Ajustes em %d categorias, cada uma exatamente uma vez (%d avancadas)."
      % (len(opcoes), n_sec, len(avancadas)))
