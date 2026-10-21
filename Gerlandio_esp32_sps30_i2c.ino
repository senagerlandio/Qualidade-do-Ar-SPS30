/*##############################################################
# Projeto de Mestrado UENF
# Placa de sensoriamento de dados para dispersão de poluentes
# Discente: Francisco Gerlandio 
#
# Orientador: Prof. Nivaldo Coorientador: Prof. Anselmo
# Data: 01/10/2026
# Atualização: 21/10/2026
# Funcionando: OLED, BME280, SPS30, RTC, MQTT, NTP
# Andamento: GPS, CO, CO2 e NO2 Talvez: NH3 e tem o sensor de CH4
# Andamento: Organizando o código
# Teste para saber SPS30 está lendo o tamanho da particula em nm ou um
# Pela tabela está em nm os valores
# Fazendo teste em float para mostrar no serial Resultado: Tamanho em Nano
# Adicionado o secrets.h
# Condição RTC, NTP
# 21/10/2026 chegou o sensor de CO2 - Instalado e funcionando. Colocado a compensação da medição de CO2 em relação a pressão do BME280
##############################################################*/

// ============================================================================
// 1. BIBLIOTECAS
// ============================================================================

#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <WiFi.h>
#include <PubSubClient.h> //MQTT
#include <ArduinoJson.h> //MQTT

#include <RTClib.h>
#include <time.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <SensirionI2cSps30.h>

#include <TinyGPSPlus.h> // Gerlandio GPS NEO-6M 
#include <SensirionI2cScd4x.h> // Gerlandio SCD41 CO2

#include "secrets.h"


// ============================================================================
// 2. CONFIGURAÇÕES DE HARDWARE
// ============================================================================

// ============================================================================
// Pinos I2C padrao do ESP32 DevKitC para 5V - Barramento I2C principal: SPS30
// ============================================================================
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;

// ============================================================================
// Pinos I2C padrao do ESP32 DevKitC Gerlandio Segundo I2C OLED para 3.3V // Segundo barramento I2C: OLED, BME280 e RTC
// ============================================================================
constexpr uint8_t SDA2_PIN = 26;
constexpr uint8_t SCL2_PIN = 14;

// ============================================================================
// OLED I2C SSD1306 128x64.
// ============================================================================
constexpr uint8_t OLED_I2C_ADDR = 0x3C;
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;

// ============================================================================
// Sensor BME280
// ============================================================================
#define BME280_ADDR 0x76

// ============================================================================
// Cartao microSD via SPI (VSPI padrao do ESP32). Gerlandio adaptado para a placa
// ============================================================================
constexpr uint8_t SD_CS_PIN = 32;
constexpr uint8_t SD_SCK_PIN = 25;
constexpr uint8_t SD_MISO_PIN = 27;
constexpr uint8_t SD_MOSI_PIN = 23;

const char* ARQUIVO_LOG = "/sps30_log.csv";

/*// ============================================================================
// GPS NEO-6M - UART2
// ============================================================================

// Ligacoes:
// GPS TX -> ESP32 GPIO16 RX
// GPS RX -> ESP32 GPIO17 TX

constexpr uint8_t GPS_RX_PIN = 16;
constexpr uint8_t GPS_TX_PIN = 17;
constexpr uint32_t GPS_BAUD = 9600;


// ============================================================================
// MiCS-6814 - CO e NO2
// ============================================================================

// RED: canal principalmente sensivel ao CO
constexpr uint8_t MICS_CO_PIN = 34;

// OX: canal principalmente sensivel ao NO2
constexpr uint8_t MICS_NO2_PIN = 35;*/ // Gerlandio

// ============================================================================
// SCD41 - CO2
// ============================================================================

// O SCD41 sera ligado no segundo barramento I2C:
// SDA = GPIO26
// SCL = GPIO14

constexpr uint8_t SCD41_I2C_ADDR = 0x62;


// ============================================================================
// 3. CONFIGURAÇÕES DE REDE WI-FI, MQTT E NTP
// ============================================================================

// Configuração Wifi (Compartilhado do Celular). Gerlandio

// secrets.h

// MQTT: preencha com os mesmos dados configurados na Nova_estacao. Gerlandio
// O topico ambiental pode ser alterado sem mexer na logica de publicacao.

//// secrets.h

const uint32_t MQTT_RETRY_MS = 5000UL;

// NTP - Time Gerlandio
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600; // Brasil (UTC-3)
const int daylightOffset_sec = 0;

// ============================================================================
// 4. VARIÁVEIS GLOBAIS
// ============================================================================

// Medição
unsigned long ultimaMedicao = 0; // Gerlandio
const unsigned long INTERVALO_MEDICAO = 10000; //Gerlandio - Tempo de amostragem

// Barramentos e sensores

TwoWire wireOled = TwoWire(1); // Gerlandio novo
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &wireOled, -1);
Adafruit_BME280 bme; // Gerlandio
SensirionI2cSps30 sps30;
RTC_DS3231 rtc; // Teste Gerlandio // RTC DS3231 - Gerlandio

/*// ============================================================================
// Objetos dos novos sensores
// ============================================================================

// UART2 do ESP32 para o GPS
HardwareSerial gpsSerial(2);

// Interpretador das mensagens NMEA do GPS
TinyGPSPlus gps;*/

// Sensor de CO2
SensirionI2cScd4x scd41;// Gerlandio


// Estado dos dispositivos
bool bmeDisponivel = false; // Gerlandio
bool sdDisponivel = false;
bool rtcDisponivel = false; // Gerlandio RTC

/*// Estado dos novos sensores
bool gpsDisponivel = false;*/
bool scd41Disponivel = false; // Gerlandio 21/10/2026

// Dados do BME280
float temperatura_bme; // Gerlandio
float umidade_bme; // Gerlandio
float pressao_bme; // Gerlandio

/*// ============================================================================
// Dados do GPS NEO-6M
// ============================================================================

double latitude = NAN;
double longitude = NAN;

float altitudeGps = NAN;
float velocidadeGps = NAN;

uint32_t satelitesGps = 0;

// ============================================================================
// Dados do MiCS-6814
// ============================================================================

// Leitura bruta do ADC
uint16_t co_adc = 0;
uint16_t no2_adc = 0;

// Tensão medida nos canais
uint32_t co_mV = 0;
uint32_t no2_mV = 0;*/// Gerlandio

// ============================================================================
// Dados do SCD41
// ============================================================================

uint16_t co2_ppm = 0;

float temperatura_scd41 = NAN;
float umidade_scd41 = NAN;



// Comunicação MQTT
// MQTT Gerlandio
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Wifi e MQTT
unsigned long ultimaTentativaWiFi = 0; // Gerlandio vai ser usada para se reconectar a internet
uint32_t ultimaTentativaMqtt = 0;

// Protótipo da função de data e hora 
DateTime obterDataHora(); //Gerlandio

// ============================================================================
// 5. FUNÇÕES MQTT
// ============================================================================

String mqttClientId() {
  uint64_t chipId = ESP.getEfuseMac();
  char clientId[48]; // Gerlandio 32 modificado para 48
  snprintf(clientId, sizeof(clientId), "SPS30_BME280-%08lX%08lX", //Gerlandio
           static_cast<unsigned long>(chipId >> 32),
           static_cast<unsigned long>(chipId));
  return String(clientId);
}

bool conectaMqtt() {
  if (WiFi.status() != WL_CONNECTED || strlen(MQTT_SERVER) == 0) {
    return false;
  }
  if (mqttClient.connected()) {
    return true;
  }

  Serial.print("[MQTT] Conectando em ");
  Serial.println(MQTT_SERVER);
  bool conectado = strlen(MQTT_USERNAME) > 0
      ? mqttClient.connect(mqttClientId().c_str(), MQTT_USERNAME, MQTT_PASSWORD)
      : mqttClient.connect(mqttClientId().c_str());
  if (!conectado) {
    Serial.printf("[MQTT] Falha, estado=%d\n", mqttClient.state());
  } else {
    Serial.println("[MQTT] Conectado.");
  }
  return conectado;
}

void mantemMqtt() {
  // Se o Wi-Fi caiu, tenta reconectar - Gerlandio
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimaTentativaMqtt >= MQTT_RETRY_MS) {
      ultimaTentativaMqtt = millis();
      Serial.println("[WIFI] Reconectando...");
      WiFi.disconnect(); // Gerlandio teste
      WiFi.reconnect();
    }
    return;
  }
  // Mantem a comunicacao do MQTT ativa - Gerlandio
  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }
  // Tenta reconectar ao broker a cada 5 segundos - Gerlandio
  if (millis() - ultimaTentativaMqtt >= MQTT_RETRY_MS) {
    ultimaTentativaMqtt = millis();
    conectaMqtt();
  }
}

// Publicação Modificado - Gerlandio
void publicaMedicaoAmbiental(
  uint16_t pm1_0,
  uint16_t pm2_5,
  uint16_t pm4_0,
  uint16_t pm10_0,
  uint16_t nc0_5,
  uint16_t nc1_0,
  uint16_t nc2_5,
  uint16_t nc4_0,
  uint16_t nc10_0,
  uint16_t tamanhoParticula
) {

  if (!mqttClient.connected()) {

    Serial.println(
      "[MQTT] Medicao nao publicada: broker desconectado."
    );

    return;
  }

  // Obtem data e hora do RTC
  DateTime agora = obterDataHora(); //DateTime agora = rtc.now(); // Gerlandio

  char dataHora[25];

  snprintf(
    dataHora,
    sizeof(dataHora),
    "%04d-%02d-%02d %02d:%02d:%02d",
    agora.year(),
    agora.month(),
    agora.day(),
    agora.hour(),
    agora.minute(),
    agora.second()
  );

  StaticJsonDocument<1024> doc; // Gerlandio 512 para 1024

  //doc["estacao"] = "ESP32_UFRJ_UENF_01"; //Gerlandio
  //doc["firmware"] = "1.0";
  //doc["projeto"] = "Mestrado_UENF";

  // Identificacao da estacao
  doc["sensor"] = "SPS30_BME280";
  doc["data_hora"] = dataHora;
  doc["timestamp_ms"] = millis();

  // Concentracao de massa das particulas
  doc["pm1_0_ug_m3"] = pm1_0;
  doc["pm2_5_ug_m3"] = pm2_5;
  doc["pm4_0_ug_m3"] = pm4_0;
  doc["pm10_0_ug_m3"] = pm10_0;

  // Concentracao numerica
  doc["nc0_5"] = nc0_5;
  doc["nc1_0"] = nc1_0;
  doc["nc2_5"] = nc2_5;
  doc["nc4_0"] = nc4_0;
  doc["nc10_0"] = nc10_0;

  // Tamanho tipico da particula
  doc["tamanho_particula_nm"] = tamanhoParticula; // Gerlandio particula é nm

  // Dados do BME280
  if (bmeDisponivel &&
      !isnan(temperatura_bme) &&
      !isnan(umidade_bme) &&
      !isnan(pressao_bme)) {

    doc["temperatura_c"] = temperatura_bme;
    doc["umidade_pct"] = umidade_bme;
    doc["pressao_hpa"] = pressao_bme;

  } else {

    doc["temperatura_c"] = nullptr;
    doc["umidade_pct"] = nullptr;
    doc["pressao_hpa"] = nullptr;
  }

  char payload[1024]; // Gerlandio de 512 para 1024

  // ==========================================================================
  // CO2 - SCD41
  // ==========================================================================

  if (scd41Disponivel && co2_ppm > 0) {

    doc["co2_ppm"] = co2_ppm;

    doc["temperatura_scd41"] =
      temperatura_scd41;

    doc["umidade_scd41"] =
      umidade_scd41;

  } else {

    doc["co2_ppm"] = nullptr;

    doc["temperatura_scd41"] =
      nullptr;

    doc["umidade_scd41"] =
      nullptr;
  }


  /*// ==========================================================================
  // CO e NO2 - MiCS-6814
  // ==========================================================================

  doc["co_adc"] = co_adc;
  doc["co_mV"] = co_mV;

  doc["no2_adc"] = no2_adc;
  doc["no2_mV"] = no2_mV;

  // Ainda nao existe conversao calibrada para ppm
  doc["co_ppm"] = nullptr;
  doc["no2_ppm"] = nullptr;


  // ==========================================================================
  // GPS NEO-6M
  // ==========================================================================

  if (gpsDisponivel) {

    doc["latitude"] = latitude;
    doc["longitude"] = longitude;

    doc["satelites_gps"] =
      satelitesGps;

    if (!isnan(altitudeGps)) {

      doc["altitude_gps_m"] =
        altitudeGps;

    } else {

      doc["altitude_gps_m"] =
        nullptr;
    }

    if (!isnan(velocidadeGps)) {

      doc["velocidade_gps_kmh"] =
        velocidadeGps;

    } else {

      doc["velocidade_gps_kmh"] =
        nullptr;
    }

  } else {

    doc["latitude"] = nullptr;
    doc["longitude"] = nullptr;

    doc["altitude_gps_m"] =
      nullptr;

    doc["velocidade_gps_kmh"] =
      nullptr;

    doc["satelites_gps"] =
      nullptr;
  }*///Gerlandio

  size_t tamanhoPayload = serializeJson(
    doc,
    payload,
    sizeof(payload)
  );

  bool publicado = mqttClient.publish(
    MQTT_TOPIC_AMBIENTAL,
    reinterpret_cast<const uint8_t*>(payload),
    tamanhoPayload,
    false
  );

  if (publicado) {

    Serial.printf(
      "[MQTT] Medicao publicada em: %s\n",
      MQTT_TOPIC_AMBIENTAL
    );

    Serial.print("[MQTT] JSON: ");
    Serial.println(payload);

  } else {

    Serial.println(
      "[MQTT] Falha ao publicar a medicao."
    );
  }
}


// ============================================================================
// 6. FUNÇÕES DO BME280
// ============================================================================

// BME 280 - Gerlandio



void lerBME280() {

  if (!bmeDisponivel) {
    return;
  }

  temperatura_bme = bme.readTemperature();
  umidade_bme = bme.readHumidity();
  pressao_bme = bme.readPressure() / 100.0F;

  if (isnan(temperatura_bme) ||
      isnan(umidade_bme) ||
      isnan(pressao_bme)) {

    Serial.println("Erro na leitura do BME280");
  }
}

/*// ============================================================================
// 7. FUNÇÕES DO GPS NEO-6M
// ============================================================================

void iniciarGPS() {

  gpsSerial.begin(
    GPS_BAUD,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );

  Serial.println("GPS NEO-6M iniciado na UART2.");
}


void atualizaGPS() {

  // Esta funcao deve ser executada continuamente no loop,
  // e nao somente a cada 10 segundos.

  while (gpsSerial.available() > 0) {

    char caractereGps = gpsSerial.read();

    gps.encode(caractereGps);
  }

  if (gps.location.isValid() &&
      gps.location.isUpdated()) {

    latitude = gps.location.lat();
    longitude = gps.location.lng();

    gpsDisponivel = true;
  }

  if (gps.altitude.isValid() &&
      gps.altitude.isUpdated()) {

    altitudeGps = gps.altitude.meters();
  }

  if (gps.speed.isValid() &&
      gps.speed.isUpdated()) {

    velocidadeGps = gps.speed.kmph();
  }

  if (gps.satellites.isValid() &&
      gps.satellites.isUpdated()) {

    satelitesGps = gps.satellites.value();
  }
}


void mostrarGPSserial() {

  Serial.println("----- GPS NEO-6M -----");

  if (gpsDisponivel) {

    Serial.printf(
      "Latitude    : %.6f\n",
      latitude
    );

    Serial.printf(
      "Longitude   : %.6f\n",
      longitude
    );

    if (!isnan(altitudeGps)) {

      Serial.printf(
        "Altitude    : %.2f m\n",
        altitudeGps
      );
    }

    if (!isnan(velocidadeGps)) {

      Serial.printf(
        "Velocidade  : %.2f km/h\n",
        velocidadeGps
      );
    }

    Serial.printf(
      "Satelites   : %lu\n",
      static_cast<unsigned long>(satelitesGps)
    );

  } else {

    Serial.println(
      "Posicao GPS ainda nao disponivel."
    );

    Serial.printf(
      "Caracteres recebidos: %lu\n",
      static_cast<unsigned long>(
        gps.charsProcessed()
      )
    );
  }

  Serial.println();
}*/

// ============================================================================
// 8. FUNÇÕES DO SCD41 - CO2
// ============================================================================

/*void iniciarSCD41() {

  // SCD41 ligado ao segundo barramento I2C
  scd41.begin(
    wireOled,
    SCD41_I2C_ADDR
  );

  // Para uma medicao anterior, caso o ESP32 tenha reiniciado
  scd41.stopPeriodicMeasurement();

  delay(500);

  int16_t erro = scd41.startPeriodicMeasurement();

  if (erro != 0) {

    scd41Disponivel = false;

    Serial.print(
      "Erro ao iniciar SCD41. Codigo: "
    );

    Serial.println(erro);

  } else {

    scd41Disponivel = true;

    Serial.println(
      "SCD41 iniciado. Medicao periodica ativa."
    );
  }
}*/ // Gerlandio

void iniciarSCD41() {

  Serial.println("Iniciando SCD41...");

  scd41.begin(
    wireOled,
    SCD41_I2C_ADDR
  );

  delay(30);

  // Garante que o sensor não ficou em medição após um reinício do ESP32
  int16_t erro = scd41.stopPeriodicMeasurement();

  if (erro != 0) {
    Serial.printf(
      "[SCD41] Aviso ao parar medicao anterior. Codigo: %d\n",
      erro
    );
  }

  delay(500);

  erro = scd41.reinit();

  if (erro != 0) {
    Serial.printf(
      "[SCD41] Erro de reinicializacao. Codigo: %d\n",
      erro
    );

    scd41Disponivel = false;
    return;
  }

  delay(30);

  erro = scd41.startPeriodicMeasurement();

  if (erro != 0) {
    Serial.printf(
      "[SCD41] Erro ao iniciar medicao. Codigo: %d\n",
      erro
    );

    scd41Disponivel = false;
    return;
  }

  scd41Disponivel = true;

  Serial.println(
    "[SCD41] Sensor iniciado. Medicao periodica ativa."
  );

  Serial.println(
    "[SCD41] Primeira medicao pode levar alguns segundos."
  );
}


/*void lerSCD41() {

  if (!scd41Disponivel) {
    return;
  }

  uint16_t co2Lido = 0;

  float temperaturaLida = NAN;
  float umidadeLida = NAN;

  int16_t erro = scd41.readMeasurement(
    co2Lido,
    temperaturaLida,
    umidadeLida
  );

  if (erro != 0) {

    Serial.print(
      "Erro na leitura do SCD41. Codigo: "
    );

    Serial.println(erro);

    return;
  }

  // CO2 igual a zero significa que nao existe
  // uma medicao valida disponivel.

  if (co2Lido == 0) {

    Serial.println(
      "SCD41 ainda sem nova medicao valida."
    );

    return;
  }

  co2_ppm = co2Lido;
  temperatura_scd41 = temperaturaLida;
  umidade_scd41 = umidadeLida;
}*/ //Gerlandio
void lerSCD41() {

  if (!scd41Disponivel) {
    return;
  }

  // Compensasão da Pressão na medida do CO2
  // Compensação de pressão do SCD41 utilizando
  // a pressão atmosférica medida pelo BME280.
  if (bmeDisponivel &&
    !isnan(pressao_bme) &&
    pressao_bme > 700 &&
    pressao_bme < 1200) {

    uint32_t pressaoPa =
        (uint32_t)(pressao_bme * 100.0);

    int16_t erroPressao =
        scd41.setAmbientPressure(
            pressaoPa
        );

    if (erroPressao != 0) {

        Serial.printf(
            "[SCD41] Erro na compensacao de pressao. Codigo: %d\n",
            erroPressao
        );

        } else{ // Gerlandio 21/10/2026 temporário para teste de compensação de pressão
          Serial.printf(
            "[SCD41] Pressao aplicada: %lu Pa\n",
            (unsigned long)pressaoPa
            );

    }
}
  //////////////////////////////////////

  bool dadosProntos = false;

  int16_t erro = scd41.getDataReadyStatus(
    dadosProntos
  );

  if (erro != 0) {
    Serial.printf(
      "[SCD41] Erro ao verificar dados. Codigo: %d\n",
      erro
    );

    return;
  }

  if (!dadosProntos) {
    Serial.println(
      "[SCD41] Ainda nao existe nova medicao."
    );

    return;
  }

  uint16_t co2Lido = 0;
  float temperaturaLida = NAN;
  float umidadeLida = NAN;

  erro = scd41.readMeasurement(
    co2Lido,
    temperaturaLida,
    umidadeLida
  );

  if (erro != 0) {
    Serial.printf(
      "[SCD41] Erro na leitura. Codigo: %d\n",
      erro
    );

    return;
  }

  if (co2Lido == 0) {
    Serial.println(
      "[SCD41] Medicao recebida, mas CO2 ainda invalido."
    );

    return;
  }

  co2_ppm = co2Lido;
  temperatura_scd41 = temperaturaLida;
  umidade_scd41 = umidadeLida;
}

/*void mostrarSCD41serial() {

  Serial.println("----- SCD41 -----");

  if (scd41Disponivel && co2_ppm > 0) {

    Serial.printf(
      "CO2         : %u ppm\n",
      co2_ppm
    );

    Serial.printf(
      "Temperatura : %.2f C\n",
      temperatura_scd41
    );

    Serial.printf(
      "Umidade     : %.2f %%\n",
      umidade_scd41
    );

  } else {

    Serial.println(
      "Medicao de CO2 ainda nao disponivel."
    );
  }

  Serial.println();
}*/ //Gerlandio

void mostrarSCD41serial() {

  Serial.println("----- SCD41 - CO2 -----");

  if (
    scd41Disponivel &&
    co2_ppm > 0 &&
    !isnan(temperatura_scd41) &&
    !isnan(umidade_scd41)
  ) {

    Serial.printf(
      "CO2          : %u ppm\n",
      co2_ppm
    );

    Serial.printf(
      "Temperatura  : %.2f C\n",
      temperatura_scd41
    );

    Serial.printf(
      "Umidade      : %.2f %%\n",
      umidade_scd41
    );

  } else {

    Serial.println(
      "Medicao de CO2 ainda nao disponivel."
    );
  }

  Serial.println();
}
/*// ============================================================================
// 9. FUNÇÕES DO MiCS-6814 - CO E NO2
// ============================================================================

uint16_t lerMediaADC(
  uint8_t pino,
  uint8_t quantidadeLeituras
) {

  uint32_t soma = 0;

  for (
    uint8_t i = 0;
    i < quantidadeLeituras;
    i++
  ) {

    soma += analogRead(pino);

    delay(2);
  }

  return soma / quantidadeLeituras;
}


void iniciarMICS6814() {

  pinMode(MICS_CO_PIN, INPUT);
  pinMode(MICS_NO2_PIN, INPUT);

  // ADC do ESP32 com resolucao de 12 bits:
  // valores entre 0 e 4095
  analogReadResolution(12);

  // Aumenta a faixa de entrada do ADC
  analogSetPinAttenuation(
    MICS_CO_PIN,
    ADC_11db
  );

  analogSetPinAttenuation(
    MICS_NO2_PIN,
    ADC_11db
  );

  Serial.println(
    "MiCS-6814 iniciado."
  );

  Serial.println(
    "CO no GPIO34 e NO2 no GPIO35."
  );
}


void lerMICS6814() {

  // Media de 20 leituras para reduzir ruido
  co_adc = lerMediaADC(
    MICS_CO_PIN,
    20
  );

  no2_adc = lerMediaADC(
    MICS_NO2_PIN,
    20
  );

  // Valor aproximado em milivolts,
  // obtido pelo ADC do ESP32
  co_mV = analogReadMilliVolts(
    MICS_CO_PIN
  );

  no2_mV = analogReadMilliVolts(
    MICS_NO2_PIN
  );
}


void mostrarMICS6814serial() {

  Serial.println("----- MiCS-6814 -----");

  Serial.printf(
    "CO  bruto : %u ADC | %lu mV\n",
    co_adc,
    static_cast<unsigned long>(co_mV)
  );

  Serial.printf(
    "NO2 bruto : %u ADC | %lu mV\n",
    no2_adc,
    static_cast<unsigned long>(no2_mV)
  );

  Serial.println(
    "CO e NO2 em ppm: aguardando calibracao."
  );

  Serial.println();
}*///Gerlandio


// ============================================================================
// 7. FUNÇÕES DO OLED
// ============================================================================

void mostraMensagemOled(const char* linha1, const char* linha2 = "") { //G - caractere apenas leitura
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(linha1);
  display.println(linha2);
  display.display();
}

void mostraMedicoesOled(uint16_t pm1_0, uint16_t pm2_5, uint16_t pm4_0, //G - variavel dado inteiro de 16 bits
                        uint16_t pm10_0, uint16_t tamanhoParticula) {
  display.clearDisplay();
  DateTime agora = obterDataHora(); //DateTime agora = rtc.now(); // Gerlandio colocar a data e hora
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SPS30 - Qualidade \x03"); // Gerlandio 20/10/2026 coração

  display.printf("%02d/%02d %02d:%02d:%02d\n",
               agora.day(),
               agora.month(),
               agora.hour(),
               agora.minute(),
               agora.second());
  //display.printf("PM1.0 : %u ug/m3\n", pm1_0); // Gerlandio 18/10/2026 Abrindo espaço no OLED
  display.printf("PM2.5 : %u ug/m3\n", pm2_5); //Gerlandio 20/10/2026 micro \xE6 não funcionou
  //display.printf("PM4.0 : %u ug/m3\n", pm4_0); // Gerlandio 18/10/2026 Abrindo espaço no OLED
  display.printf("PM10  : %u ug/m3\n", pm10_0);
  display.printf("Part. : %u nm\n", tamanhoParticula); // Gerlandio Valor é em nm - nanometro
  
  if (scd41Disponivel && co2_ppm > 0) {
    display.printf("CO2   : %u ppm\n", co2_ppm);
    } else {
    display.println("CO2   : ---");
  } 

  display.printf("T=%.1f U=%.0f%% P=%.1f\n", // Gerlandio 20/10/2026 colocando a %
               temperatura_bme,
               umidade_bme,
               pressao_bme);
  //display.printf("Gerlandio Teste OLED"); // Gerlandio Funcionou
  display.display();
}

// ============================================================================
// 8. FUNÇÕES DO CARTÃO MICROSD
// ============================================================================

void iniciaCartaoSd() {
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  sdDisponivel = SD.begin(SD_CS_PIN, SPI);

  if (!sdDisponivel) {
    Serial.println("Aviso: cartao SD nao encontrado; log desativado.");
    return;
  }

  if (!SD.exists(ARQUIVO_LOG)) {
    File arquivo = SD.open(ARQUIVO_LOG, FILE_WRITE);
    if (arquivo) {
      //arquivo.println("data_hora,tempo_ms,temp_c,umidade_pct,pressao_hpa,pm1_0_ug_m3,pm2_5_ug_m3,pm4_0_ug_m3,pm10_0_ug_m3,nc0_5,nc1_0,nc2_5,nc4_0,nc10_0,tamanho_particula_nm");//Gerlandio
      arquivo.println(
        "data_hora,"
        "tempo_ms,"
        "temp_bme_c,"
        "umidade_bme_pct,"
        "pressao_bme_hpa,"
        "pm1_0_ug_m3,"
        "pm2_5_ug_m3,"
        "pm4_0_ug_m3,"
        "pm10_0_ug_m3,"
        "nc0_5_np_cm3,"
        "nc1_0_np_cm3,"
        "nc2_5_np_cm3,"
        "nc4_0_np_cm3,"
        "nc10_0_np_cm3,"
        "tamanho_particula_nm," // Gerlandio é nm
        "co2_ppm,"
        "temp_scd41,"
        "umidade_scd41"
        //"co_adc,"
        //"co_mV,"
        //"no2_adc,"
        //"no2_mV,"
        //"latitudes,"
        //"longitude,"
        //"altitude_gps_m,"
        //"velocidade_gps_kmh,"
        //"satelites_gps"
        );//Gerlandio
      arquivo.close();
    }
  }

  Serial.println("Cartao SD pronto. Gravando em /sps30_log.csv");
}

void registraMedicaoSd(uint16_t pm1_0, uint16_t pm2_5, uint16_t pm4_0,
                        uint16_t pm10_0, uint16_t nc0_5, uint16_t nc1_0,
                        uint16_t nc2_5, uint16_t nc4_0, uint16_t nc10_0,
                        uint16_t tamanhoParticula) {
  if (!sdDisponivel) {
    return;
  }

  File arquivo = SD.open(ARQUIVO_LOG, FILE_APPEND);
  if (!arquivo) {
    Serial.println("Erro: nao foi possivel abrir o arquivo de log no SD.");
    sdDisponivel = false;
    return;
  }

  //Gerlandio
  DateTime agora = obterDataHora(); //DateTime agora = rtc.now(); //Gerlandio teste sem 

  float tempCSV = temperatura_bme;
  float umidCSV = umidade_bme;
  float pressCSV = pressao_bme;

  if (!bmeDisponivel) {

    tempCSV = -999;
    umidCSV = -999;
    pressCSV = -999;
  }

  int co2CSV = -999;
  float tempScdCSV = -999.0;
  float umidScdCSV = -999.0;

  if (
    scd41Disponivel &&
    co2_ppm > 0 &&
    !isnan(temperatura_scd41) &&
    !isnan(umidade_scd41)
  ) {
    co2CSV = co2_ppm;
    tempScdCSV = temperatura_scd41;
    umidScdCSV = umidade_scd41;
  }

  /*arquivo.printf("%04d-%02d-%02d %02d:%02d:%02d,%lu,%.2f,%.2f,%.2f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                 agora.year(),agora.month(),agora.day(),agora.hour(),agora.minute(),agora.second(),
                 millis(),tempCSV,umidCSV,pressCSV,pm1_0,pm2_5,pm4_0,pm10_0,nc0_5,
                 nc1_0,nc2_5,nc4_0,nc10_0,tamanhoParticula);*/ //Gerlandio 21/10/2026


  /*arquivo.printf("%04d-%02d-%02d %02d:%02d:%02d,%lu,%.2f,%.2f,%.2f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                 agora.year(),agora.month(),agora.day(),agora.hour(),agora.minute(),agora.second(),
                 millis(),temperatura_bme,umidade_bme,pressao_bme,pm1_0,pm2_5,pm4_0,pm10_0,nc0_5,
                 nc1_0,nc2_5,nc4_0,nc10_0,tamanhoParticula);*/

  
  // FUTURO - SPS30 + BME280 + SCD41 + MiCS6814 + GPS

  arquivo.printf(
  "%04d-%02d-%02d %02d:%02d:%02d,%lu,"
  "%.2f,%.2f,%.2f,"
  "%u,%u,%u,%u,"
  "%u,%u,%u,%u,%u,"
  "%u,"
  "%d,%.2f,%.2f\n", // Gerlandio \n é importante 
  //"%u,%lu,"
  //"%u,%lu,"
  //"%.6f,%.6f,%.2f,%.2f,"
  //"%lu\n",

  agora.year(),agora.month(),agora.day(),
  agora.hour(),agora.minute(),agora.second(),

  millis(),
  // Gerlandio Tratamento da Temperatura
  tempCSV,//temperatura_bme, // Gerlandio trocar para tempCSV pq faz tratamento caso sensor de problema valor de -999
  umidCSV,//umidade_bme,
  pressCSV,//pressao_bme,

  pm1_0,
  pm2_5,
  pm4_0,
  pm10_0,

  nc0_5,
  nc1_0,
  nc2_5,
  nc4_0,
  nc10_0,

  tamanhoParticula,

  co2CSV,
  tempScdCSV,
  umidScdCSV

  //co_adc,
  //co_mV,

  //no2_adc,
  //no2_mV,

  //latitude,
  //longitude,
  //altitudeGps,
  //velocidadeGps,

  //satelitesGps
  );// Gerlandio 21/10/2026  


  arquivo.close();
}


// ============================================================================
// 9. FUNÇÕES WI-FI, NTP E RTC
// ============================================================================

void conectarWiFi(){

  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Gerlandio modificado WiFi.begin(ssid, senha);
  Serial.println("Entrou no WIFI"); // Gerlandio Teste

  int tentativas = 0;

  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500); // 
    Serial.print(".");
    tentativas++;
    Serial.println(tentativas); // Gerlandio teste
    Serial.println("Gerlandio Teste Wifi100"); // Gerlandio Teste

  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi indisponivel. Sistema continuara funcionando.");
  }

}


bool atualizarRTCcomNTP() {// Atualização do RTC pelo NTP

  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );

  struct tm timeinfo;

  Serial.println("Obtendo hora da internet...");

  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("Erro ao obter hora pelo NTP!");
    return false;
  }

  /*rtc.adjust(DateTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  ));

  Serial.println("RTC atualizado pelo NTP!");*/ //Gerlandio 13/10/2026

  if (rtcDisponivel) { //Gerlandio 13/10/2026

  rtc.adjust(DateTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  ));

  Serial.println("RTC atualizado pelo NTP.");

  } else {

    Serial.println("Hora obtida do NTP.");
  }

  return true;
}

/*void verificarWiFi() {

  if (WiFi.status() == WL_CONNECTED && !rtcSincronizadoNTP) {
    rtcSincronizadoNTP = atualizarRTCcomNTP(); // verificar
    return;
  }

  if (millis() - ultimaTentativaWiFi >= 60000) { // agora só tenta de 60 em 60 segundos

    ultimaTentativaWiFi = millis();

    Serial.println("Wi-Fi desconectado. Tentando reconectar...");

    WiFi.disconnect();
    WiFi.begin(ssid, senha);
  }
}*/ // Rotina não ficou Legal

/*void iniciarRTC() {

  if (!rtc.begin(&wireOled)) {
    Serial.println("Erro ao encontrar DS3231");
    while (1){ 
      delay(1000)
    };
  }

  if (rtc.lostPower()) {

    Serial.println("RTC perdeu alimentacao!");
  
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  }

  Serial.println("RTC iniciado");
}*/// Teste Gerlandio

/*void iniciarRTC() {

  if (!rtc.begin(&wireOled)) {

    Serial.println("Erro ao encontrar DS3231");

    while (1) {
      delay(1000);
    }
  }

  Serial.println("RTC iniciado");

  if (rtc.lostPower()) {

    Serial.println("RTC perdeu alimentacao");

    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}*/ //Gerlandio dia 13/10/2026

void iniciarRTC() {//Gerlandio 13/10/2026

  rtcDisponivel = rtc.begin(&wireOled);

  if (!rtcDisponivel) {

    Serial.println("RTC nao encontrado.");
    Serial.println("O sistema tentara usar o NTP.");

    return;
  }

  Serial.println("RTC iniciado.");

  if (rtc.lostPower()) {

    Serial.println("RTC perdeu alimentacao.");

    rtc.adjust(
      DateTime(F(__DATE__), F(__TIME__))
    );
  }
}



// Apenas mostra a hora do RTC
void mostrarHoraRTC() {

  DateTime agora = obterDataHora(); //DateTime agora = rtc.now(); // Gerlandio

  Serial.printf(
    "%02d/%02d/%04d %02d:%02d:%02d\n",
    agora.day(),
    agora.month(),
    agora.year(),
    agora.hour(),
    agora.minute(),
    agora.second()
  );
} // Teste Gerlandio

// ============================================================================
// Função única para obter data e hora
// RTC -> se existir
// NTP -> se RTC estiver ausente
// ============================================================================
DateTime obterDataHora() {

  if (rtcDisponivel) {
    return rtc.now();
  }

  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {

    return DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    );
  }

  // Sem RTC e sem NTP
  return DateTime(2000, 1, 1, 0, 0, 0);
} // Gerlandio

// ============================================================================
// 10. SETUP — INICIALIZAÇÃO DO SISTEMA
// ============================================================================

void setup() {
  Serial.begin(115200);
  //MQTT - Gerlandio
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(1024); // Gerlandio 512 para 1024
  mqttClient.setKeepAlive(30); //??? Gerlandio não entendi o que é isso

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // Gerlandio item novo 07/10/2026
  wireOled.begin(SDA2_PIN, SCL2_PIN); // Gerlandio item novo 07/10/2026
  wireOled.setClock(100000); // Gerlandio item novo 07/10/2026

  /*// GPS NEO-6M
  iniciarGPS();*///Gerlandio GPS

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("Erro: OLED SSD1306 nao encontrado em 0x3C.");
    while (true) {
      delay(10000); //Gerlandio Aumentando os delays
    }
  }

  //Gerlandio - Comentado para teste
  bmeDisponivel = bme.begin(BME280_ADDR, &wireOled); // Gerlandio modificação para o segundo I2C 

  if (bmeDisponivel) {
    Serial.println("BME280 iniciado.");
  } else {
    Serial.println("ATENCAO: BME280 nao encontrado.");
  }

  // SCD41 - CO2
  iniciarSCD41();

  /*// MiCS-6814 - CO e NO2
  iniciarMICS6814();*///Gerlandio

  iniciarRTC (); // Gerlandio teste desativando 
  Serial.println("Gerlandio Teste apos RTC 10"); // Gerlandio Teste
  delay (1000); // Gerlandio

  conectarWiFi(); // Gerlandio

  /*if (WiFi.status() == WL_CONNECTED) { // Gerlandio

    if (atualizarRTCcomNTP()) {

      //Serial.println("RTC sincronizado com NTP"); //Gerlandio 13/10/26

      if (rtcDisponivel) {

        Serial.println("RTC sincronizado com NTP");

      } else {

        Serial.println(
          "RTC ausente. Utilizando horario do NTP."
        );
      }

      mostrarHoraRTC();

    } else {

      Serial.println("Falha no NTP. Usando horario do RTC.");
    }
  }*/ // Gerlandio 13/10/2026

  if (WiFi.status() == WL_CONNECTED) { // Gerlandio 13/10/2026

    if (atualizarRTCcomNTP()) {

      if (rtcDisponivel) {

        Serial.println("RTC sincronizado com NTP");

        mostrarHoraRTC(); // Gerlandio 13/10/26

      } else {

        Serial.println(
          "RTC ausente. Utilizando horario do NTP."
        );
      }



    } else {

      if (rtcDisponivel) {

        Serial.println(
          "Falha no NTP. Usando horario do RTC."
        );

      } else {

        Serial.println(
          "ATENCAO: RTC ausente e NTP indisponivel."
        );
      }
    }

    } else {

    if (!rtcDisponivel) {

      Serial.println(
        "ATENCAO: RTC ausente e sem conexao WiFi."
      );

      Serial.println(
        "Data e hora nao estarao disponiveis."
      );
    }
  }

  // Gerlandio
  if (WiFi.status() == WL_CONNECTED) {
  conectaMqtt();
  }

  //Serial.println("Teste apos o WIFI"); // Gerlandio Teste
  //delay (1000); // Gerlandio

  Serial.println("Horario atual do RTC:");
  Serial.println("Gerlandio Teste 20");
  //mostrarHoraRTC(); // Gerlandio Teste


  ///////

  mostraMensagemOled("Inicializando...GG", "SPS30 + OLED"); // Gerlandio Teste
  iniciaCartaoSd();

  // Endereco I2C padrao do SPS30: 0x69
  sps30.begin(Wire, SPS30_I2C_ADDR_69);
  sps30.stopMeasurement();
  delay(1000); //Gerlandio Aumentando os delays

  int16_t erro = sps30.startMeasurement(
      SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16); // Gerlandio Teste para descobrir se está medindo em micro ou nano 12/10/2026

  if (erro != 0) {
    Serial.println("Erro: nao foi possivel iniciar o SPS30.");
    mostraMensagemOled("Erro no SPS30", "Verifique o I2C");
    while (true) {
      delay(1000); //Gerlandio Aumentando os delays
    }
  }

  Serial.println("SPS30 iniciado. Leitura de particulas em andamento.");
}


//// Teste Leitura Tamanho da Particula - Resultado em Nano
//Gerlandio Setup para teste de nano e micro 
/*
void setup() {

  Serial.begin(115200);

  Wire.begin(21,22);

  sps30.begin(Wire, SPS30_I2C_ADDR_69);

  Serial.println("Iniciando SPS30...");

}*/
//////////////////////////////////////////////////////////////

// ============================================================================
// 11. LOOP — AQUISIÇÃO, EXIBIÇÃO, REGISTRO E TRANSMISSÃO
// ============================================================================

void loop() {

  // Gerlandio Verificando Wifi
  //verificarWiFi(); //// não é bom, toda vez que Wifi cai ele reinicia

  uint16_t dadosProntos = 0;
  uint16_t pm1_0, pm2_5, pm4_0, pm10_0;
  uint16_t nc0_5, nc1_0, nc2_5, nc4_0, nc10_0;
  uint16_t tamanhoParticula;

  //delay(10000); //Gerlandio Tempo entre medições // Fazer a leitura por millis

  mantemMqtt(); // Gerlandio
    
  //atualizaGPS(); //Gerlandio Deve executar continuamente

  // Medição Gerlandio
  if (millis() - ultimaMedicao < INTERVALO_MEDICAO) {
    return;
  }

  ultimaMedicao = millis();
  //

  //Serial.println("Entrou aqui 1"); // Gerlandio deu certo aqui

  lerBME280(); // Gerlandio dados de temperatura, umidaderelativa e pressão Para teste acender LCD

  lerSCD41(); // Gerlandio
  /*lerMICS6814();*///Gerlandio

  int16_t erro = sps30.readDataReadyFlag(dadosProntos);
  if (erro != 0 || !dadosProntos) {
    Serial.println("Aguardando dados do SPS30...");
    mostraMensagemOled("Aguardando", "dados do SPS30...");
    return;
  }

  erro = sps30.readMeasurementValuesUint16(
      pm1_0, pm2_5, pm4_0, pm10_0,
      nc0_5, nc1_0, nc2_5, nc4_0, nc10_0,
      tamanhoParticula);

  if (erro != 0) {
    Serial.println("Erro ao ler os valores do SPS30.");
    mostraMensagemOled("Erro de leitura", "do SPS30");
    return;
  }
  mostrarHoraRTC(); // Gerlandio para mostrar as horas na simulação
  Serial.println("----- Qualidade do ar ----- "); // Gerlandio Teste
  //Gerlandio temperatura, Umidade, Pressão BME280
  Serial.printf("Temperatura : %.2f C\n", temperatura_bme);
  Serial.printf("Umidade     : %.2f %%\n", umidade_bme);
  Serial.printf("Pressao     : %.2f hPa\n\n", pressao_bme);
  ///
  Serial.printf("PM1.0 : %u ug/m3\n", pm1_0);
  Serial.printf("PM2.5 : %u ug/m3\n", pm2_5);
  Serial.printf("PM4.0 : %u ug/m3\n", pm4_0);
  Serial.printf("PM10  : %u ug/m3\n", pm10_0);
  Serial.printf("Tamanho tipico: %u nm\n\n", tamanhoParticula); // Gerlandio Valor é em nm - nanometro

  //* Gerlandio - Colocando a medição concentração númerica para mostrar
  Serial.printf("NC0.5 : %u np/cm3\n", nc0_5);
  Serial.printf("NC1.0 : %u np/cm3\n", nc1_0);
  Serial.printf("NC2.5 : %u np/cm3\n", nc2_5);
  Serial.printf("NC4.0 : %u np/cm3\n", nc4_0);
  Serial.printf("NC10  : %u np/cm3\n", nc10_0);

  mostrarSCD41serial(); // Gerlandio

  /*mostrarGPSserial();
    
    mostrarMICS6814serial();*/


  mostraMedicoesOled(pm1_0, pm2_5, pm4_0, pm10_0, tamanhoParticula);
  registraMedicaoSd(pm1_0, pm2_5, pm4_0, pm10_0,
                    nc0_5, nc1_0, nc2_5, nc4_0, nc10_0, tamanhoParticula);

  //Serial.println("Entrou aqui 2"); //Gerlandio deu certo aqui
  publicaMedicaoAmbiental( //Gerlandio pq só esses
    pm1_0,
    pm2_5,
    pm4_0,
    pm10_0,
    nc0_5,
    nc1_0,
    nc2_5,
    nc4_0,
    nc10_0,
    tamanhoParticula
  );

}


// ============================================================================
// X. Teste tamanho da Particula
// ============================================================================
// Gerlandio teste de leitura em nano ou micro do SPS30
//// Teste Leitura Tamanho da Particula - Resultado em Nano
//Gerlandio Setup para teste de nano e micro 
/*
void loop() {

  int16_t erro;

  // ======================
  // LEITURA UINT16
  // ======================

  uint16_t mc1_u, mc25_u, mc4_u, mc10_u;
  uint16_t nc05_u, nc10_u, nc25_u, nc40_u, nc100_u;
  uint16_t tps_u;

  sps30.stopMeasurement();
  delay(1000);

  sps30.startMeasurement(
      SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_UINT16);

  delay(3000);

  erro = sps30.readMeasurementValuesUint16(
      mc1_u,
      mc25_u,
      mc4_u,
      mc10_u,
      nc05_u,
      nc10_u,
      nc25_u,
      nc40_u,
      nc100_u,
      tps_u);

  if (erro != 0) {
    Serial.println("Erro leitura UINT16");
    return;
  }

  // ======================
  // LEITURA FLOAT
  // ======================

  float mc1_f, mc25_f, mc4_f, mc10_f;
  float nc05_f, nc10_f, nc25_f, nc40_f, nc100_f;
  float tps_f;

  sps30.stopMeasurement();
  delay(1000);

  sps30.startMeasurement(
      SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);

  delay(3000);

  erro = sps30.readMeasurementValuesFloat(
      mc1_f,
      mc25_f,
      mc4_f,
      mc10_f,
      nc05_f,
      nc10_f,
      nc25_f,
      nc40_f,
      nc100_f,
      tps_f);

  if (erro != 0) {
    Serial.println("Erro leitura FLOAT");
    return;
  }

  // ======================
  // COMPARAÇÃO
  // ======================

  Serial.println();
  Serial.println("================================");

  Serial.printf(
      "TPS UINT16 = %u\n",
      tps_u);

  Serial.printf(
      "TPS FLOAT  = %.6f\n",
      tps_f);

  Serial.printf(
      "TPS UINT16/1000 = %.6f\n",
      tps_u / 1000.0);

  Serial.println("================================");

  delay(10000);
}*/
