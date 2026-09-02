/*
  Controle de Lâmpada — ESP32 (WiFi + Bluetooth BLE)
  ---------------------------------------------------
  Liga/desliga um relé (que por sua vez liga/desliga a lâmpada) através de:
    - HTTP (Wi-Fi):   GET /on  |  GET /off  |  GET /status
    - BLE (Bluetooth): escreve "ON" ou "OFF" na característica abaixo

  LIGAÇÃO (IMPORTANTE — SEGURANÇA):
    Nunca ligue a lâmpada (220V/110V) diretamente no ESP32.
    Use um MÓDULO RELÉ (ou relé de estado sólido) entre o ESP32 e a lâmpada:

      ESP32 GPIO 26  ---->  IN do módulo relé
      ESP32 GND      ---->  GND do módulo relé
      ESP32 5V/VIN   ---->  VCC do módulo relé (se o módulo pedir 5V)

      Lado de alta tensão do relé: COM + NA (normalmente aberto) em série
      com um dos fios da lâmpada, ligado na tomada/rede elétrica.

    Se não tiver experiência com rede elétrica, peça ajuda de alguém
    qualificado — é a parte que oferece risco real no projeto.

  BIBLIOTECAS NECESSÁRIAS (Arduino IDE > Gerenciador de Bibliotecas):
    - "ESP32 BLE Arduino" (Neil Kolban / h2zero) — geralmente já vem com o
      pacote de placas ESP32.
    - WiFi.h e WebServer.h já vêm inclusas no core do ESP32.

  PLACA: qualquer ESP32 (DevKit v1, WROOM-32, etc.)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---------------- CONFIGURAÇÕES ----------------
const char* WIFI_SSID     = "SUA_REDE_WIFI";
const char* WIFI_PASSWORD = "SUA_SENHA_WIFI";

const int  RELAY_PIN        = 26;   // GPIO ligado ao módulo relé
const bool RELAY_ACTIVE_LOW = true; // maioria dos módulos relé liga em nível LOW

const char* BLE_DEVICE_NAME = "ESP32-Lampada";
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-1234567890ab"
// ------------------------------------------------

WebServer server(80);
BLECharacteristic *pCharacteristic;
BLEServer *pBleServer;
bool lampState  = false;
bool bleClientConnected = false;

void setLamp(bool ligar) {
  lampState = ligar;
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? !ligar : ligar);

  if (pCharacteristic) {
    pCharacteristic->setValue(ligar ? "ON" : "OFF");
    if (bleClientConnected) pCharacteristic->notify();
  }
  Serial.printf("Lampada: %s\n", ligar ? "LIGADA" : "DESLIGADA");
}

// ---------------- BLE callbacks ----------------
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *srv) override {
    bleClientConnected = true;
  }
  void onDisconnect(BLEServer *srv) override {
    bleClientConnected = false;
    srv->getAdvertising()->start(); // volta a anunciar para novas conexões
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String valor = pChar->getValue().c_str();
    valor.trim();
    valor.toUpperCase();
    if (valor == "ON")  setLamp(true);
    else if (valor == "OFF") setLamp(false);
  }
};

// ---------------- HTTP handlers ----------------
void sendCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
}

void handleRoot() {
  sendCors();
  String html =
    "<!doctype html><meta charset='utf-8'>"
    "<title>ESP32 Lampada</title>"
    "<h1>Controle da Lampada</h1>"
    "<p>Estado atual: <b>" + String(lampState ? "LIGADA" : "DESLIGADA") + "</b></p>"
    "<p><a href='/on'>Ligar</a> &nbsp;|&nbsp; <a href='/off'>Desligar</a></p>";
  server.send(200, "text/html", html);
}

void handleOn() {
  sendCors();
  setLamp(true);
  server.send(200, "text/plain", "ON");
}

void handleOff() {
  sendCors();
  setLamp(false);
  server.send(200, "text/plain", "OFF");
}

void handleStatus() {
  sendCors();
  server.send(200, "text/plain", lampState ? "ON" : "OFF");
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  setLamp(false);

  // ---- Wi-Fi ----
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado! IP do ESP32: ");
  Serial.println(WiFi.localIP());
  Serial.println("-> Use esse IP no app, no modo Wi-Fi.");

  server.on("/", handleRoot);
  server.on("/on", handleOn);
  server.on("/off", handleOff);
  server.on("/status", handleStatus);
  server.begin();

  // ---- Bluetooth BLE ----
  BLEDevice::init(BLE_DEVICE_NAME);
  pBleServer = BLEDevice::createServer();
  pBleServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pBleServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new CharacteristicCallbacks());
  pCharacteristic->setValue("OFF");
  pService->start();

  BLEAdvertising *pAdvertising = pBleServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("BLE pronto. Anunciando como: " + String(BLE_DEVICE_NAME));
}

void loop() {
  server.handleClient();
}
