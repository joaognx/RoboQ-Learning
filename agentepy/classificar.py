import sys
import time
import cv2
import numpy as np
import requests

URL = "http://192.168.4.100/capture"   # camera (IP fixo)

# ====== LIMITES CALIBRADOS (HSV do OpenCV: H 0-180, S 0-255, V 0-255) ======
AZUL_H = (82, 105)     # matiz do tabuleiro
AZUL_S_MIN = 90        # tabuleiro tem cor forte
LARANJA_H = (5, 30)    # rachaduras da lava
LARANJA_S_MIN = 80
LARANJA_V_MIN = 90     # o laranja e mais claro que o preto em volta
MIN_AZUL = 0.50        # >50% azul -> casa livre
MIN_LARANJA = 0.03     # >3% laranja -> lava
V_PRETO = 62           # brilho mediano abaixo disso -> lava (parte preta)
# ===========================================================================

def tirar_foto(tentativas=8):
    for i in range(1, tentativas + 1):
        try:
            r = requests.get(URL, timeout=5)
            r.raise_for_status()
            return cv2.imdecode(np.frombuffer(r.content, np.uint8), cv2.IMREAD_COLOR)
        except requests.RequestException as e:
            print(f"   [camera sem resposta, tentativa {i}/{tentativas}: {type(e).__name__}]")
            time.sleep(2)
    raise RuntimeError("A camera nao respondeu. Ela reiniciou ou saiu da rede?")

def medir(img):
    h, w = img.shape[:2]
    miolo = img[h // 6: 5 * h // 6, w // 6: 5 * w // 6]   # ignora as bordas
    hsv = cv2.cvtColor(cv2.GaussianBlur(miolo, (9, 9), 0), cv2.COLOR_BGR2HSV)
    H, S, V = [hsv[..., i].astype(int) for i in range(3)]
    azul = (H >= AZUL_H[0]) & (H <= AZUL_H[1]) & (S >= AZUL_S_MIN)
    laranja = (H >= LARANJA_H[0]) & (H <= LARANJA_H[1]) & (S >= LARANJA_S_MIN) & (V >= LARANJA_V_MIN)
    return azul.mean(), laranja.mean(), float(np.median(V))

def classificar(img):
    azul, laranja, brilho = medir(img)
    if azul >= MIN_AZUL:
        return "livre"
    if laranja >= MIN_LARANJA or brilho < V_PRETO:
        return "lava"
    return "pedra"

def descrever(img):
    azul, laranja, brilho = medir(img)
    return f"{classificar(img):6s} | azul {azul:5.1%}  laranja {laranja:5.1%}  brilho {brilho:5.1f}"

if __name__ == "__main__":
    if len(sys.argv) > 1:                      # python3 classificar.py foto1.png foto2.png ...
        for arq in sys.argv[1:]:
            print(f"{arq:35s} -> {descrever(cv2.imread(arq))}")
    else:                                      # ao vivo: qualquer tecla = nova foto, q = sair
        while True:
            foto = tirar_foto()
            print(descrever(foto))
            cv2.imshow("casa embaixo do robo", foto)
            if cv2.waitKey(0) == ord("q"):
                break
        cv2.destroyAllWindows()
