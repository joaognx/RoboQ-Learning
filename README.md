# RoboQ-Learning

Robô físico de baixo custo que aprende, por **Q-learning tabular**, a atravessar um tabuleiro 5 × 7 com obstáculos desconhecidos. Ele percebe as casas apenas por uma **ESP32-CAM voltada para o chão** e um classificador de cores em HSV.

Projeto final da disciplina de Inteligência Artificial, Departamento de Computação, Universidade Federal do Piauí (UFPI).

> **Artigo:** *Physical Robot Grid Navigation via Tabular Q-Learning with Camera-Based Obstacle Perception*. João Gabriel Nunes Oliveira Lima.
> **Base de dados (Kaggle):** https://www.kaggle.com/datasets/joaognlx/physical-robot-grid-navigation-with-q-learning
> **Notebook de análise (Kaggle):** https://www.kaggle.com/code/joaognlx/robot-q-learning

## Resultados principais

- O robô real aprendeu o **caminho ótimo de 12 passos** depois de **488 movimentos** de experiência. A mediana da simulação sem ruído (100 sementes) é de 494 movimentos.
- A câmera acertou **98,0%** das decisões livre × obstáculo, com **0% de falsos negativos** e **2,1% de falsos positivos**.
- Os erros de percepção pesam de forma **assimétrica**: falsos negativos se corrigem na visita seguinte, enquanto falsos positivos podem congelar um desvio subótimo de 14 passos. Com 2,1% de FP, só 59% das simulações chegam ao ótimo.
- Uma regressão logística treinada com 11 atributos HSV eleva a acurácia balanceada de três classes de **75,5% para 99,8%** (validação Leave-One-Cell-Out).

## Como funciona

```
 ┌────────────────────┐   HTTP /cmd?c=w|s|a|d    ┌──────────────────────┐
 │  PC (Python)       │ ───────────────────────▶ │  ESP32 do robô       │
 │  agente.py         │                          │  L298N + 2 motores   │
 │  Q-learning        │                          │  MPU6050 (giros 90°) │
 │                    │   HTTP /capture (JPEG)   ├──────────────────────┤
 │  classificar.py    │ ◀─────────────────────── │  ESP32-CAM + flash   │
 │  regras HSV        │                          │  voltada para o chão │
 └────────────────────┘                          └──────────────────────┘
       Rede Wi-Fi "Robo-ESP32", criada pelo próprio robô (192.168.4.1)
```

A cada passo, o agente escolhe uma ação (ε-greedy) e manda o robô andar uma casa. Em seguida, pede uma foto à câmera e classifica a casa como **livre**, **lava** ou **pedra**:

| Situação | Recompensa | O que acontece |
|---|---|---|
| Casa livre | −1 | o robô fica na casa nova |
| Obstáculo percebido | −10 | o robô volta para a casa anterior |
| Chegada em (4, 6) | +50 | fim do episódio |

Parâmetros: α = 0,5, γ = 0,95, ε = máx(0,05; 0,30 × 0,90ᵏ) por episódio k e no máximo 60 passos por episódio. A tabela Q é salva **a cada passo**, então o treino continua de onde parou depois de uma troca de bateria.

## Estrutura do repositório

```
RoboQ-Learning/
├── agentepy/                 # Lado do PC (Python)
│   ├── agente.py             # Q-learning: treino real, simulação e caminho aprendido
│   ├── robo.py               # Comunicação HTTP com o robô (andar, girar, foto)
│   ├── classificar.py        # Captura da foto e classificação por regras HSV
│   └── coletar_fotos.py      # Coleta manual de fotos rotuladas
├── robotfimware/
│   └── main.cpp              # Firmware do robô (ESP32 + L298N + MPU6050)
└── espcam/
    ├── main.cpp              # Firmware da câmera (ESP32-CAM)
    └── platformio.ini
```

## Hardware

- ESP32 (controle dos motores e ponto de acesso Wi-Fi)
- Driver de motor L298N e 2 motores DC sem encoder, mais roda boba
- Giroscópio MPU6050 no endereço I²C `0x69`, usado nos giros de 90° e na correção de rumo
- ESP32-CAM (OV2640) voltada para o chão, com o flash ligado durante a foto
- Tabuleiro azul 5 × 7 com bordas pretas e obstáculos de EVA cobertos com papel: lava (preto com rachaduras laranja) e pedra (cinza-claro com traços escuros)

Pinos do L298N no ESP32 do robô: `ENA=25, IN1=26, IN2=27, ENB=33, IN3=18, IN4=19`.

## Instalação

### 1. Firmwares (PlatformIO)

**Câmera:** dentro de `espcam/`, rode

```bash
pio run -t upload
```

A câmera se conecta à rede do robô com IP fixo `192.168.4.100`.

**Robô:** grave `robotfimware/main.cpp` num ESP32 (por exemplo, `board = esp32dev`, `framework = arduino`). O robô cria a rede `Robo-ESP32` (senha `robo12345`) e responde em `192.168.4.1`.

### 2. Python

```bash
pip install opencv-python numpy requests
```

Conecte o PC à rede `Robo-ESP32` antes de rodar os scripts.

## Uso

Todos os comandos abaixo são executados dentro de `agentepy/`.

```bash
# Testar a lógica do Q-learning num tabuleiro simulado (sem robô)
python3 agente.py --simular

# Treinar com o robô real (pede para posicionar o robô em (0,0), virado para cima)
python3 agente.py

# Mostrar o caminho aprendido até agora
python3 agente.py --caminho
```

Ferramentas auxiliares:

```bash
python3 robo.py               # controle manual: w/a/s/d move, f tira foto, q sai
python3 classificar.py        # classificação ao vivo das fotos da câmera
python3 classificar.py a.jpg  # classifica fotos salvas
python3 coletar_fotos.py      # coleta fotos rotuladas (1 livre, 2 lava, 3 pedra, 4 chegada)
```

Durante o treino real, cada transição é registrada em `dados/transicoes.csv` e a foto correspondente vai para `dados/fotos_treino/`. Esses arquivos formam a base publicada no Kaggle.

### Ajustes sem regravar o firmware

| Endereço | Para que serve |
|---|---|
| `http://192.168.4.1/` | página de controle e calibração do robô |
| `http://192.168.4.1/config?tf=...&va=...&vb=...` | tempo de avanço, velocidades e ganho da correção de rumo (fica salvo na memória) |
| `http://192.168.4.1/calibrar` | recalibra o giroscópio |
| `http://192.168.4.100/capture` | tira uma foto |
| `http://192.168.4.100/config?exp=...&ganho=...&wb=...&flash=...` | exposição, ganho, balanço de branco e força do flash |

## Limitações conhecidas

- Não há encoders nas rodas, então a posição é estimada por *dead reckoning*: deslizamento pode desalinhar o robô da casa esperada.
- O reflexo do flash no papel brilhante faz a lava ser confundida com pedra pelas regras HSV.
- Episódios podem ser interrompidos por queda de bateria ou de Wi-Fi. A tabela Q salva a cada passo evita perder o aprendizado.

## Licença e uso de IA

Os dados e o notebook estão publicados no Kaggle sob **CC BY 4.0**.

O Claude (Anthropic) foi usado como ferramenta de apoio para depurar os firmwares e o agente Python, analisar os registros e organizar a documentação. Todo o código foi escrito, executado e testado no robô físico pelo autor.
