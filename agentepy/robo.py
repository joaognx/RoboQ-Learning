import time
import requests
from classificar import tirar_foto, classificar, descrever

ROBO = "http://192.168.4.1"   # o robo cria a rede e sempre tem esse IP

# direcoes do tabuleiro visto de cima, em sentido anti-horario (cada uma = +90 graus)
DIRECOES = ["cima", "esquerda", "baixo", "direita"]

PAUSA_GIRO = 0.5   # segundos parado entre girar e andar


class Robo:
    def __init__(self):
        self.virado = 0   # indice em DIRECOES para onde a frente do robo aponta

    def _cmd(self, c):
        r = requests.get(f"{ROBO}/cmd", params={"c": c}, timeout=15)
        r.raise_for_status()
        return r.text

    def zerar(self):
        """Chame com o robo virado para 'cima' no inicio de cada episodio."""
        requests.get(f"{ROBO}/zerar", timeout=5)
        self.virado = 0

    def mover(self, direcao):
        """Anda UMA casa na direcao pedida e devolve o tipo da casa onde parou."""
        return self.mover_com_foto(direcao)[0]

    def mover_com_foto(self, direcao):
        """Igual ao mover, mas devolve (tipo da casa, foto)."""
        alvo = DIRECOES.index(direcao)
        dif = (alvo - self.virado) % 4
        if dif == 0:            # ja esta virado para la
            self._cmd("w")
        elif dif == 2:          # direcao oposta: anda de re, sem girar
            self._cmd("s")
        elif dif == 1:          # 90 graus a esquerda
            self._cmd("a"); self.virado = alvo; time.sleep(PAUSA_GIRO); self._cmd("w")
        else:                   # 90 graus a direita
            self._cmd("d"); self.virado = alvo; time.sleep(PAUSA_GIRO); self._cmd("w")
        foto = tirar_foto()
        return classificar(foto), foto


if __name__ == "__main__":
    robo = Robo()
    teclas = {"w": "cima", "s": "baixo", "a": "esquerda", "d": "direita"}
    input("Coloque o robo virado para CIMA e aperte Enter...")
    robo.zerar()
    print("Comandos: w=cima  s=baixo  a=esquerda  d=direita  f=so foto  q=sair")
    while True:
        t = input("> ").strip().lower()
        if t == "q":
            break
        if t == "f":
            print("  ", descrever(tirar_foto()))
        elif t in teclas:
            print(f"   {teclas[t]} -> caiu em: {robo.mover(teclas[t])}")
