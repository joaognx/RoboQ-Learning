#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

// A camera entra na rede Wi-Fi criada pelo proprio robo
const char* SSID  = "Robo-ESP32";
const char* SENHA = "robo12345";

WebServer server(80);

// ====== AJUSTES DE IMAGEM (valores iniciais) ======
int forcaFlash = 20;   // 0 = apagado, 255 = maximo
int exposicao  = 300;  // 0 a 1200 (maior = mais claro)
int ganho      = 0;    // 0 a 30   (maior = mais claro, mais ruido)
int balanco    = 3;    // 0 auto, 1 sol, 2 nublado, 3 escritorio, 4 casa
// ==================================================

// Pinos da ESP32-CAM modelo AI Thinker
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

bool iniciarCamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk  = XCLK_GPIO_NUM;
  c.pin_pclk  = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href  = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn  = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size   = FRAMESIZE_VGA;     // 640x480 (pode baixar para FRAMESIZE_QVGA)
  c.jpeg_quality = 12;                // 10 = melhor, 63 = pior
  c.fb_count     = 2;
  c.fb_location  = CAMERA_FB_IN_PSRAM;
  c.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("Erro ao iniciar a camera: 0x%x\n", err);
    return false;
  }
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);        // vira de cabeca para cima
  s->set_hmirror(s, 1);      // desfaz o espelhado lateral
  s->set_saturation(s, 2);   // realca as cores
  s->set_brightness(s, 1);   // clareia um pouco

  // Exposicao, ganho e balanco de branco FIXOS: como a luz agora e sempre
  // a do flash, a camera nao deve "compensar" sozinha (senao o preto vira cinza)
  s->set_exposure_ctrl(s, 0);  s->set_aec_value(s, exposicao);
  s->set_gain_ctrl(s, 0);      s->set_agc_gain(s, ganho);
  s->set_whitebal(s, 1);       s->set_awb_gain(s, 1);
  s->set_wb_mode(s, balanco);
  return true;
}

// http://IP/config?exp=300&ganho=0&wb=3&flash=20  -> muda os ajustes sem regravar
void configurar() {
  sensor_t* s = esp_camera_sensor_get();
  if (server.hasArg("flash")) forcaFlash = constrain(server.arg("flash").toInt(), 0, 255);
  if (server.hasArg("exp"))   { exposicao = constrain(server.arg("exp").toInt(), 0, 1200); s->set_aec_value(s, exposicao); }
  if (server.hasArg("ganho")) { ganho = constrain(server.arg("ganho").toInt(), 0, 30);    s->set_agc_gain(s, ganho); }
  if (server.hasArg("wb"))    { balanco = constrain(server.arg("wb").toInt(), 0, 4);      s->set_wb_mode(s, balanco); }
  String r = "flash=" + String(forcaFlash) + " exp=" + String(exposicao) +
             " ganho=" + String(ganho) + " wb=" + String(balanco);
  Serial.println(r);
  server.send(200, "text/plain", r);
}

// http://IP/capture -> devolve UMA foto JPEG atual
void tirarFoto() {
  ledcWrite(4, forcaFlash);  // acende o flash
  delay(300);                // tempo para a camera se ajustar a luz

  camera_fb_t* fb = esp_camera_fb_get();   // descarta uma foto antiga
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();                // pega a foto atual

  ledcWrite(4, 0);           // apaga o flash
  if (!fb) {
    server.send(500, "text/plain", "Falha ao capturar");
    return;
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nIniciando ESP32-CAM...");

  ledcAttachChannel(4, 5000, 8, 2);  // flash no GPIO 4, canal 2
  ledcWrite(4, 0);

  if (!iniciarCamera()) {
    Serial.println("Verifique o encaixe do cabo da camera e reinicie.");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                  // sem modo economia: responde sempre na hora
  WiFi.setAutoReconnect(true);
  // IP fixo: a camera sempre fica em 192.168.4.100 (o robo e o 192.168.4.1)
  WiFi.config(IPAddress(192, 168, 4, 100), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.begin(SSID, SENHA);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado!");
  Serial.print("Abra no navegador: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/capture");

  server.on("/capture", tirarFoto);
  server.on("/config", configurar);
  server.begin();
  WiFi.setTxPower(WIFI_POWER_19_5dBm);   // sinal no maximo (a camera fica embaixo do robo)
}

unsigned long ultimaChecagem = 0;

void loop() {
  server.handleClient();

  // se caiu do Wi-Fi do robo, tenta entrar de novo a cada 3 s
  if (millis() - ultimaChecagem > 3000) {
    ultimaChecagem = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi caiu, reconectando...");
      WiFi.reconnect();
    }
  }
}
