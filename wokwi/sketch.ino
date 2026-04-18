/*
 * ==================================================================================
 * FarmTech Solutions - Fase 2 Cap 1
 * Sistema de Irrigacao Inteligente - Cultura: Soja
 * ==================================================================================
 * Versao: v2
 * Autor: Ronaldo Nishime
 *
 * Descricao:
 *   Monitora sensores de Nitrogenio (N), Fosforo (P), Potassio (K) via botoes,
 *   pH do solo via LDR (simulado), e umidade do solo via DHT22. Decide ligar 
 *   ou desligar a bomba de irrigacao (rele) com base em logica otimizada para
 *   a cultura da soja.
 *
 *   Feedback visual:
 *     - LCD 16x2 I2C: exibe valores dos sensores e estado da bomba
 *     - LEDs verdes (N, P, K): acendem conforme botao correspondente pressionado
 *     - LED amarelo (pH OK): acende quando pH esta na faixa ideal
 *     - LED vermelho (BOMBA): acende quando a bomba esta ligada
 *
 * Logica de decisao da bomba:
 *   - Umidade < 40%:                 bomba LIGA  (irrigacao necessaria)
 *   - Umidade > 70%:                 bomba DESLIGA (solo saturado)
 *   - Umidade entre 40% e 70%:       LIGA se pH ideal (5.5 a 6.3) E (P ou K presente)
 *                                    DESLIGA caso contrario
 *   - N e apenas monitorado (soja realiza Fixacao Biologica de Nitrogenio)
 *
 * Faixas ideais para soja (fonte: EMBRAPA):
 *   - pH: 5.5 a 6.3
 *   - Umidade: 50% a 85% da agua disponivel no solo
 * ==================================================================================
 */

#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Pinos ----------
#define PIN_BTN_N          18
#define PIN_BTN_P          19
#define PIN_BTN_K           5
#define PIN_LDR_PH         34
#define PIN_DHT_UMID       23
#define PIN_RELE_BOMBA     13

#define PIN_LED_N           4
#define PIN_LED_P          16
#define PIN_LED_K          17
#define PIN_LED_PH_OK      25
#define PIN_LED_BOMBA      13   // Compartilhado com o rele (mesmo sinal)

#define PIN_I2C_SDA        21
#define PIN_I2C_SCL        22

// ---------- Parametros dos sensores ----------
#define DHT_TIPO           DHT22
#define ADC_MAX            4095     // Resolucao do ADC do ESP32 (12 bits)
#define PH_MAX             14.0     // Escala maxima de pH

// ---------- Faixas ideais para soja ----------
#define PH_MIN_IDEAL        5.5
#define PH_MAX_IDEAL        6.3
#define UMID_LIMITE_INF    40.0   // Abaixo disso: liga bomba
#define UMID_LIMITE_SUP    70.0   // Acima disso: desliga bomba

// ---------- Intervalos ----------
#define INTERVALO_LEITURA_MS   2000   // Ciclo principal (minimo DHT22)
#define INTERVALO_LCD_MS       3000   // Alterna entre tela 1 e tela 2

// ---------- Endereco I2C do LCD ----------
#define LCD_ADDR       0x27
#define LCD_COLS       16
#define LCD_ROWS        2

// ---------- Objetos ----------
DHT dht(PIN_DHT_UMID, DHT_TIPO);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---------- Controle de alternancia de tela do LCD ----------
unsigned long ultimaTrocaLCD = 0;
int telaLCD = 0;  // 0 = tela sensores, 1 = tela bomba/NPK

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("==================================================");
  Serial.println("FarmTech Solutions - Sistema de Irrigacao da Soja");
  Serial.println("==================================================");

  // Botoes com pull-up interno
  pinMode(PIN_BTN_N, INPUT_PULLUP);
  pinMode(PIN_BTN_P, INPUT_PULLUP);
  pinMode(PIN_BTN_K, INPUT_PULLUP);

  // LEDs de status
  pinMode(PIN_LED_N, OUTPUT);
  pinMode(PIN_LED_P, OUTPUT);
  pinMode(PIN_LED_K, OUTPUT);
  pinMode(PIN_LED_PH_OK, OUTPUT);

  // Rele (e LED vermelho compartilham o mesmo pino)
  pinMode(PIN_RELE_BOMBA, OUTPUT);
  digitalWrite(PIN_RELE_BOMBA, LOW);

  // Inicializa sensor DHT
  dht.begin();

  // Inicializa I2C e LCD
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("FarmTech Soja");
  lcd.setCursor(0, 1);
  lcd.print("Inicializando..");
  delay(1500);
  lcd.clear();

  Serial.println("Sistema inicializado. Iniciando leituras...");
  Serial.println();
}

void loop() {
  // ---------- Leitura dos sensores ----------
  bool temN = (digitalRead(PIN_BTN_N) == LOW);
  bool temP = (digitalRead(PIN_BTN_P) == LOW);
  bool temK = (digitalRead(PIN_BTN_K) == LOW);

  int leituraLDR = analogRead(PIN_LDR_PH);
  float ph = (leituraLDR / (float)ADC_MAX) * PH_MAX;

  float umidade = dht.readHumidity();

  if (isnan(umidade)) {
    Serial.println("ERRO: falha na leitura do DHT22. Tentando novamente...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Erro leitura");
    lcd.setCursor(0, 1);
    lcd.print("DHT22");
    delay(INTERVALO_LEITURA_MS);
    return;
  }

  // ---------- Logica de decisao da bomba ----------
  bool phIdeal = (ph >= PH_MIN_IDEAL && ph <= PH_MAX_IDEAL);
  bool ligarBomba = false;
  String justificativa = "";

  if (umidade < UMID_LIMITE_INF) {
    ligarBomba = true;
    justificativa = "Umidade baixa (< 40%): irrigacao necessaria";
  }
  else if (umidade > UMID_LIMITE_SUP) {
    ligarBomba = false;
    justificativa = "Umidade alta (> 70%): solo saturado";
  }
  else {
    bool temNutrienteCritico = (temP || temK);
    if (phIdeal && temNutrienteCritico) {
      ligarBomba = true;
      justificativa = "Umidade intermediaria, pH ideal e P/K presente";
    } else {
      ligarBomba = false;
      if (!phIdeal) {
        justificativa = "Umidade intermediaria, mas pH fora da faixa ideal";
      } else {
        justificativa = "Umidade intermediaria, mas falta P e K";
      }
    }
  }

  // ---------- Atualizacao dos atuadores e LEDs ----------
  digitalWrite(PIN_RELE_BOMBA, ligarBomba ? HIGH : LOW);
  digitalWrite(PIN_LED_N, temN ? HIGH : LOW);
  digitalWrite(PIN_LED_P, temP ? HIGH : LOW);
  digitalWrite(PIN_LED_K, temK ? HIGH : LOW);
  digitalWrite(PIN_LED_PH_OK, phIdeal ? HIGH : LOW);

  // ---------- Atualizacao do LCD (alterna entre 2 telas) ----------
  if (millis() - ultimaTrocaLCD > INTERVALO_LCD_MS) {
    telaLCD = 1 - telaLCD;
    ultimaTrocaLCD = millis();
    lcd.clear();
  }

  if (telaLCD == 0) {
    // Tela 1: umidade e pH
    lcd.setCursor(0, 0);
    lcd.print("Umidade: ");
    lcd.print(umidade, 1);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("pH: ");
    lcd.print(ph, 2);
    lcd.print(phIdeal ? " [OK]" : " [!!]");
  } else {
    // Tela 2: bomba e NPK
    lcd.setCursor(0, 0);
    lcd.print("Bomba: ");
    lcd.print(ligarBomba ? "LIGADA" : "desligada");
    lcd.setCursor(0, 1);
    lcd.print("N:");
    lcd.print(temN ? "1" : "0");
    lcd.print(" P:");
    lcd.print(temP ? "1" : "0");
    lcd.print(" K:");
    lcd.print(temK ? "1" : "0");
  }

  // ---------- Saida no Monitor Serial ----------
  Serial.println("----------------------------------------");
  Serial.print("Nitrogenio (N): "); Serial.println(temN ? "PRESENTE" : "ausente");
  Serial.print("Fosforo (P):    "); Serial.println(temP ? "PRESENTE" : "ausente");
  Serial.print("Potassio (K):   "); Serial.println(temK ? "PRESENTE" : "ausente");
  Serial.print("pH (LDR):       "); Serial.print(ph, 2);
  Serial.print(" (leitura ADC: "); Serial.print(leituraLDR); Serial.print(")");
  Serial.println(phIdeal ? " [IDEAL]" : " [fora da faixa]");
  Serial.print("Umidade solo:   "); Serial.print(umidade, 1); Serial.println(" %");
  Serial.print("Bomba:          "); Serial.println(ligarBomba ? "LIGADA" : "desligada");
  Serial.print("Motivo:         "); Serial.println(justificativa);
  Serial.println();

  delay(INTERVALO_LEITURA_MS);
}