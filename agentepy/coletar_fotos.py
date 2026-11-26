"""Coleta de fotos rotuladas para a base de dados.

Coloque o robo em cima de uma casa, olhe a foto e aperte:
  1 = livre   2 = lava   3 = pedra   4 = chegada
  espaco = outra foto sem salvar   c = mudar coordenada   q = sair
As fotos vao para dados/fotos_rotuladas/<classe>/ e a lista para rotulos.csv.
"""
import csv, os
from datetime import datetime
import cv2
from classificar import tirar_foto, classificar

PASTA = "dados/fotos_rotuladas"
CLASSES = {ord("1"): "livre", ord("2"): "lava", ord("3"): "pedra", ord("4"): "chegada"}

def salvar(foto, classe):
    pasta = os.path.join(PASTA, classe)
    os.makedirs(pasta, exist_ok=True)
    nome = f"{classe}_{datetime.now():%Y%m%d_%H%M%S_%f}.jpg"
    cv2.imwrite(os.path.join(pasta, nome), foto)
    lista = os.path.join(PASTA, "rotulos.csv")
    novo = not os.path.exists(lista)
    with open(lista, "a", newline="") as f:
        w = csv.writer(f)
        if novo:
            w.writerow(["arquivo", "classe_real", "classe_regras", "x", "y"])
        w.writerow([f"{classe}/{nome}", classe, classificar(foto), POS[0], POS[1]])
    contagem = len(os.listdir(pasta))
    print(f"   salvo como {classe} ({contagem} fotos de {classe})")

if __name__ == "__main__":
    print(__doc__)
    entrada = input("Coordenada da casa (ex: 2 3), ou Enter para nao informar: ").split()
    POS = (int(entrada[0]), int(entrada[1])) if len(entrada) == 2 else ("", "")
    while True:
        foto = tirar_foto()
        cv2.imshow("coleta (1 livre, 2 lava, 3 pedra, 4 chegada, q sair)", foto)
        t = cv2.waitKey(0)
        if t == ord("q"):
            break
        if t in CLASSES:
            salvar(foto, CLASSES[t])
        if t == ord("c"):
            entrada = input("Nova coordenada (ex: 2 3): ").split()
            POS = (int(entrada[0]), int(entrada[1])) if len(entrada) == 2 else ("", "")
    cv2.destroyAllWindows()
