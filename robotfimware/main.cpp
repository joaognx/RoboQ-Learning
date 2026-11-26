#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>

const char* SSID  = "Robo-ESP32";
const char* SENHA = "robo12345";

const int ENA = 25, IN1 = 26, IN2 = 27;
const int ENB = 33, IN3 = 18, IN4 = 19;
const int MPU = 0x69;

// valores ajustaveis pelo painel
int tempoFrente = 640;   // ms para andar uma casa
int velA = 180, velB = 200;
int velGiro = 180;       // velocidade maxima no giro
int velMin = 150;        // menor velocidade que ainda faz o robo girar
int tolerancia = 2;      // erro aceito no giro (graus)
int kp10 = 30;           // correcao para andar reto (x10)
int sinal = 1;           // sentido do giroscopio (1 ou -1)

float biasZ = 0;
float rumo = 0;   // direcao atual medida (graus)
float alvo = 0;   // direcao desejada (multiplo de 90)
unsigned long tAnt = 0;

WebServer server(80);
Preferences prefs;

// ===== Motores =====
void motorA(int vel) {
  digitalWrite(IN1, vel > 0);
  digitalWrite(IN2, vel < 0);
  analogWrite(ENA, abs(vel));
}

void motorB(int vel) {
  digitalWrite(IN3, vel > 0);
  digitalWrite(IN4, vel < 0);
  analogWrite(ENB, abs(vel));
}

void parar() { motorA(0); motorB(0); }

// ===== Giroscopio =====
void escreverMPU(byte reg, byte valor) {
  Wire.beginTransmission(MPU);
  Wire.write(reg);
  Wire.write(valor);
  Wire.endTransmission();
}

int16_t lerRawZ() {
  Wire.beginTransmission(MPU);
  Wire.write(0x47);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 2);
  int16_t alto = Wire.read();
  int16_t baixo = Wire.read();
  return (int16_t)((alto << 8) | baixo);
}

float lerGiroZ() { return lerRawZ() / 131.0 - biasZ; }

void calibrarGiro() {
  float soma = 0;
  biasZ = 0;
  for (int i = 0; i < 500; i++) {
    soma += lerRawZ() / 131.0;
    delay(2);
  }
  biasZ = soma / 500;
}

void iniciarMedida() { tAnt = micros(); }

void medir() {
  unsigned long t = micros();
  rumo += sinal * lerGiroZ() * (t - tAnt) / 1e6;
  tAnt = t;
}

void esperarMedindo(int ms) {
  unsigned long inicio = millis();
  while (millis() - inicio < (unsigned long)ms) { medir(); delay(2); }
}

// ===== Movimentos =====
void andar(int sentido) {  // +1 frente, -1 tras
  iniciarMedida();
  unsigned long inicio = millis();
  while (millis() - inicio < (unsigned long)tempoFrente) {
    medir();
    float corr = (kp10 / 10.0) * (rumo - alvo);
    motorA((int)constrain(sentido * velA + corr, -255, 255));
    motorB((int)constrain(sentido * velB - corr, -255, 255));
    delay(5);
  }
  parar();
  esperarMedindo(200);
}

void girar(int dir) {  // +1 esquerda, -1 direita
  alvo += dir * 90;
  iniciarMedida();
  unsigned long inicio = millis();
  while (millis() - inicio < 5000) {
    medir();
    float erro = alvo - rumo;
    if (fabs(erro) <= tolerancia) {
      parar();
      esperarMedindo(250);  // espera o robo assentar e confere
      if (fabs(alvo - rumo) <= tolerancia) break;
      continue;
    }
    int vel = constrain((int)(fabs(erro) * 4), velMin, velGiro);
    int s = erro > 0 ? 1 : -1;
    motorA(-s * vel);
    motorB(s * vel);
    delay(2);
  }
  parar();
}

// ===== Servidor =====
String estado() {
  return " | rumo " + String(rumo, 1) + " (alvo " + String((int)alvo) + ")";
}

void comando() {
  String c = server.arg("c");
  if      (c == "w") andar(1);
  else if (c == "s") andar(-1);
  else if (c == "a") girar(1);
  else if (c == "d") girar(-1);
  else { server.send(400, "text/plain", "comando invalido"); return; }
  server.send(200, "text/plain", "ok " + c + estado());
}

void ajustar(const char* nome, int &var, int minimo, int maximo) {
  if (server.hasArg(nome)) {
    var = constrain(server.arg(nome).toInt(), minimo, maximo);
    prefs.putInt(nome, var);
  }
}

void config() {
  ajustar("tf", tempoFrente, 0, 5000);
  ajustar("va", velA, 0, 255);
  ajustar("vb", velB, 0, 255);
  ajustar("vg", velGiro, 0, 255);
  ajustar("vm", velMin, 0, 255);
  ajustar("tl", tolerancia, 1, 20);
  ajustar("kp", kp10, -200, 200);
  ajustar("sg", sinal, -1, 1);
  if (sinal == 0) sinal = 1;
  String r = "tf=" + String(tempoFrente) + " vg=" + String(velGiro) + " vm=" + String(velMin) +
             " tl=" + String(tolerancia) + " kp=" + String(kp10) + " va=" + String(velA) +
             " vb=" + String(velB) + " sg=" + String(sinal);
  server.send(200, "text/plain", r);
}

void recalibrar() {
  calibrarGiro();
  server.send(200, "text/plain", "giroscopio calibrado");
}

void zerar() {
  rumo = 0;
  alvo = 0;
  server.send(200, "text/plain", "direcao zerada" + estado());
}

const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:sans-serif;text-align:center}
.m button{font-size:24px;width:70px;height:60px;margin:4px}
.p{margin:10px}.p button{width:48px;height:42px;font-size:20px}
.p span{display:inline-block;width:70px;font-size:20px}
.b button{margin:6px;padding:10px}
</style></head><body>
<h2>Robô</h2>
<div class="m"><button onclick="c('w')">↑</button></div>
<div class="m"><button onclick="c('a')">↺</button><button onclick="c('s')">↓</button><button onclick="c('d')">↻</button></div>
<p id="st">pronto</p>
<div class="b"><button onclick="g('/zerar')">Zerar direção</button><button onclick="g('/calibrar')">Recalibrar giroscópio</button></div>
<div id="ps"></div>
<script>
const N={tf:'Tempo andar',vg:'Vel giro máx',vm:'Vel giro mín',tl:'Tolerância giro (graus)',kp:'Correção reto',va:'Vel A',vb:'Vel B',sg:'Sentido giroscópio'};
const S={tf:20,vg:5,vm:5,tl:1,kp:5,va:5,vb:5,sg:2};
let v={};
const st=document.getElementById('st'), ps=document.getElementById('ps');
function c(x){st.textContent='movendo...';fetch('/cmd?c='+x).then(r=>r.text()).then(t=>st.textContent=t);}
function g(u){st.textContent='...';fetch(u).then(r=>r.text()).then(t=>st.textContent=t);}
function load(t){t.split(' ').forEach(p=>{const[k,x]=p.split('=');v[k]=+x});draw();}
function draw(){ps.innerHTML=Object.keys(N).map(k=>
 `<div class=p>${N[k]}<br><button onclick="m('${k}',-${S[k]})">−</button><span>${v[k]}</span><button onclick="m('${k}',${S[k]})">+</button></div>`).join('');}
function m(k,d){fetch('/config?'+k+'='+(v[k]+d)).then(r=>r.text()).then(load);}
fetch('/config').then(r=>r.text()).then(load);
</script></body></html>
)rawliteral";

void pagina() { server.send(200, "text/html", PAGINA); }

void setup() {
  Serial.begin(115200);
  int pinos[] = {ENA, IN1, IN2, ENB, IN3, IN4};
  for (int p : pinos) pinMode(p, OUTPUT);
  parar();

  Wire.begin(21, 22);
  Wire.beginTransmission(MPU);
  if (Wire.endTransmission() == 0) Serial.println("MPU6050 encontrado");
  else Serial.println("ERRO: MPU6050 nao encontrado");
  escreverMPU(0x6B, 0);
  escreverMPU(0x1B, 0);
  escreverMPU(0x1A, 3);
  delay(100);
  calibrarGiro();

  prefs.begin("robo", false);
  tempoFrente = prefs.getInt("tf", tempoFrente);
  velA        = prefs.getInt("va", velA);
  velB        = prefs.getInt("vb", velB);
  velGiro     = prefs.getInt("vg", velGiro);
  velMin      = prefs.getInt("vm", velMin);
  tolerancia  = prefs.getInt("tl", tolerancia);
  kp10        = prefs.getInt("kp", kp10);
  sinal       = prefs.getInt("sg", sinal);

  WiFi.softAP(SSID, SENHA);
  Serial.println(WiFi.softAPIP());

  server.on("/", pagina);
  server.on("/cmd", comando);
  server.on("/config", config);
  server.on("/calibrar", recalibrar);
  server.on("/zerar", zerar);
  server.begin();
}

void loop() {
  server.handleClient();
}
