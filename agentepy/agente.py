"""Agente de Q-learning do robo no tabuleiro.

  python3 agente.py --simular   testa o aprendizado num tabuleiro de mentira (sem robo)
  python3 agente.py             treina com o robo de verdade
  python3 agente.py --caminho   mostra o caminho aprendido ate agora
"""
import csv, json, os, random, sys, time
from datetime import datetime

# ====== TABULEIRO (x = coluna, cresce para a DIREITA; y = linha, cresce para CIMA) ======
LARGURA, ALTURA = 5, 7       # x vai de 0 a 4, y vai de 0 a 6
INICIO  = (0, 0)             # canto de baixo a esquerda
CHEGADA = (4, 6)             # canto de cima a direita (papel roxo)

# ====== OBSTACULOS DE VERDADE ======
# O agente NAO usa isso para decidir nada, serve so para rotular as fotos
MAPA_REAL = {
    (1, 1): "lava",  (3, 1): "lava",  (2, 5): "lava",
    (0, 2): "pedra", (3, 0): "pedra", (3, 2): "pedra", (3, 3): "pedra", (2, 4): "pedra",
}

def casa_real(pos):
    if pos == CHEGADA:
        return "chegada"
    return MAPA_REAL.get(pos, "livre")

# ====== RECOMPENSAS E PARAMETROS ======
R_LIVRE, R_OBSTACULO, R_CHEGADA = -1, -10, 50
ALFA, GAMA = 0.5, 0.95                           # taxa de aprendizado e desconto
EPS_INICIAL, EPS_MIN, EPS_DECAI = 0.3, 0.05, 0.9 # exploracao aleatoria, cai a cada episodio
MAX_PASSOS = 60                                  # desiste do episodio depois disso
PAUSA_ENTRE_PASSOS = 1.0    # segundos parado entre um movimento e o proximo (robo real)

ACOES = {"cima": (0, 1), "baixo": (0, -1), "esquerda": (-1, 0), "direita": (1, 0)}
OPOSTA = {"cima": "baixo", "baixo": "cima", "esquerda": "direita", "direita": "esquerda"}


# ---------- Tabela Q ----------
def chave(pos): return f"{pos[0]},{pos[1]}"

def q(Q, pos, a): return Q.get(chave(pos), {}).get(a, 0.0)

def acoes_validas(pos):   # nao deixa sair do tabuleiro
    return [a for a, (dx, dy) in ACOES.items()
            if 0 <= pos[0] + dx < LARGURA and 0 <= pos[1] + dy < ALTURA]

def escolher(Q, pos, eps):
    validas = acoes_validas(pos)
    if random.random() < eps:
        return random.choice(validas)                       # explora
    melhor = max(q(Q, pos, a) for a in validas)
    return random.choice([a for a in validas if q(Q, pos, a) == melhor])  # aproveita

def atualizar(Q, pos, a, r, prox, fim):
    futuro = 0 if fim else max(q(Q, prox, b) for b in acoes_validas(prox))
    antigo = q(Q, pos, a)
    Q.setdefault(chave(pos), {})[a] = antigo + ALFA * (r + GAMA * futuro - antigo)

def carregar(arq):
    if os.path.exists(arq):
        d = json.load(open(arq))
        return d["Q"], d["episodio"]
    return {}, 0

def salvar(arq, Q, ep):
    json.dump({"Q": Q, "episodio": ep}, open(arq, "w"), indent=1)


# ---------- Ambientes ----------
class AmbienteReal:
    def __init__(self):
        from robo import Robo
        self.robo = Robo()

    def reiniciar(self):
        input(f"\nColoque o robo na casa {INICIO}, virado para CIMA, e aperte Enter...")
        self.robo.zerar()

    def passo(self, acao, destino):
        return self.robo.mover_com_foto(acao)   # (tipo da casa, foto)

    def voltar(self, acao):            # sai do obstaculo voltando para a casa anterior
        time.sleep(PAUSA_ENTRE_PASSOS)
        self.robo.mover(OPOSTA[acao])

    def pausa(self):
        time.sleep(PAUSA_ENTRE_PASSOS)


class AmbienteSimulado:             # para testar a logica sem gastar bateria
    OBSTACULOS = MAPA_REAL   # simula o tabuleiro montado de verdade
    def reiniciar(self): pass
    def passo(self, acao, destino): return self.OBSTACULOS.get(destino, "livre"), None
    def voltar(self, acao): pass
    def pausa(self): pass


# ---------- Registro dos dados (vira a base publica no Kaggle) ----------
ARQ_LOG = "dados/transicoes.csv"
PASTA_FOTOS = "dados/fotos_treino"
CAMPOS = ["data_hora", "episodio", "passo", "x", "y", "acao", "x_destino", "y_destino",
          "casa_classificada", "casa_real", "recompensa", "x_proximo", "y_proximo", "fim", "epsilon", "foto"]

def registrar(linha, foto):
    os.makedirs(PASTA_FOTOS, exist_ok=True)
    if foto is not None:
        import cv2
        nome = f"ep{linha['episodio']:03d}_passo{linha['passo']:02d}.jpg"
        cv2.imwrite(os.path.join(PASTA_FOTOS, nome), foto)
        linha["foto"] = nome
    novo = not os.path.exists(ARQ_LOG)
    with open(ARQ_LOG, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CAMPOS)
        if novo:
            w.writeheader()
        w.writerow(linha)


# ---------- Treino ----------
def episodio(amb, Q, eps, arq, ep, falar=True, gravar=False):
    amb.reiniciar()
    pos, total = INICIO, 0
    for n in range(1, MAX_PASSOS + 1):
        a = escolher(Q, pos, eps)
        dx, dy = ACOES[a]
        destino = (pos[0] + dx, pos[1] + dy)
        casa, foto = amb.passo(a, destino)

        if destino == CHEGADA or casa == "chegada":
            r, prox, fim = R_CHEGADA, destino, True
        elif casa in ("lava", "pedra"):
            r, prox, fim = R_OBSTACULO, pos, False     # penaliza e volta para onde estava
            amb.voltar(a)
        else:
            r, prox, fim = R_LIVRE, destino, False

        atualizar(Q, pos, a, r, prox, fim)
        salvar(arq, Q, ep)
        if gravar:
            registrar({"data_hora": datetime.now().isoformat(timespec="seconds"),
                       "episodio": ep, "passo": n, "x": pos[0], "y": pos[1], "acao": a,
                       "x_destino": destino[0], "y_destino": destino[1], "casa_classificada": casa,
                       "casa_real": casa_real(destino) if MAPA_REAL else "",
                       "recompensa": r, "x_proximo": prox[0], "y_proximo": prox[1],
                       "fim": int(fim), "epsilon": round(eps, 4), "foto": ""}, foto)                             # nada se perde se cair a bateria
        total += r
        if falar:
            print(f"  passo {n:2d}: {pos} -> {a:8s} -> {casa:7s} (recompensa {r:+d})")
        pos = prox
        if fim:
            return True, n, total
        amb.pausa()
    return False, MAX_PASSOS, total

def caminho(Q):
    pos, rota = INICIO, [INICIO]
    for _ in range(LARGURA * ALTURA):
        if pos == CHEGADA:
            break
        a = max(acoes_validas(pos), key=lambda b: q(Q, pos, b))
        dx, dy = ACOES[a]
        pos = (pos[0] + dx, pos[1] + dy)
        if pos in rota:
            return rota + [pos], False                 # entrou em loop: ainda nao aprendeu
        rota.append(pos)
    return rota, pos == CHEGADA


if __name__ == "__main__":
    simular = "--simular" in sys.argv
    arq = "tabela_q_simulada.json" if simular else "tabela_q.json"
    Q, ep = carregar(arq)

    if "--caminho" in sys.argv:
        rota, ok = caminho(Q)
        print(("Caminho aprendido: " if ok else "Ainda nao chega la: ") + " -> ".join(map(str, rota)))
        sys.exit()

    amb = AmbienteSimulado() if simular else AmbienteReal()
    episodios = 200 if simular else 1000
    try:
        for _ in range(episodios):
            ep += 1
            eps = max(EPS_MIN, EPS_INICIAL * EPS_DECAI ** ep)
            chegou, n, total = episodio(amb, Q, eps, arq, ep, falar=not simular, gravar=not simular)
            print(f"Episodio {ep:3d}: {'CHEGOU' if chegou else 'desistiu'} em {n:2d} passos, "
                  f"recompensa {total:+d}, exploracao {eps:.2f}")
            if not simular:
                rota, ok = caminho(Q)
                print("  caminho atual:", " -> ".join(map(str, rota)), "(completo)" if ok else "")
    except KeyboardInterrupt:
        print("\nTreino pausado. A tabela Q foi salva; rode de novo para continuar.")
    rota, ok = caminho(Q)
    print(("\nCaminho aprendido: " if ok else "\nAinda nao chega la: ") + " -> ".join(map(str, rota)))
