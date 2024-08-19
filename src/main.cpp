#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <arduinoFFT.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#define SD_CS 8 //SD CS to PIN 5 --> if different should specifed in SD.begin(PIN)
#include <SD.h>
bool existSD = true;

// Configuración de pines para botones, micrófono y DFPlayer Mini
#define BUTTON_PIN1 32  // Botón para desplazarse entre opciones
#define BUTTON_PIN2 33  // Botón para seleccionar opción
#define BUTTON_PIN3 25  // Botón no usado aquí
#define BUTTON_INTERRUPT_PIN 36 // Botón no usado aquí
#define DEBOUNCE_DELAY 50 // Tiempo de antirrebote en milisegundos

#include <JPEGDecoder.h>

#define PIN_MP3_TX 15 // Conecta al RX del módulo DFPlayer Mini
#define PIN_MP3_RX 5  // Conecta al TX del módulo DFPlayer Mini
#define MIC_PIN 34    // Pin del micrófono

// Variables de FFT y frecuencias
#define SAMPLES 512               // Número de muestras para FFT
#define SAMPLING_FREQUENCY 4000   // Frecuencia de muestreo del ADC
arduinoFFT FFT = arduinoFFT();
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned int sampling_period_us;
unsigned long microseconds;

// Inicialización de la pantalla TFT
TFT_eSPI tft = TFT_eSPI(); 

// Inicialización del DFPlayer Mini
SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;

// Variables de estado
volatile bool inMenu = true;
volatile bool inMode1 = false;
volatile bool inMode2 = false;
volatile bool inMode3 = false;
volatile bool inWiFiMode = false; // Nuevo estado para el modo WiFi
int selectedMode = 0; // Variable para rastrear el modo seleccionado en el menú
int selectedOption = 0; // Variable para rastrear la opción seleccionada dentro de un modo
int correctNoteIndex = 0; // Índice de la nota correcta en el juego
int ganancia = 0; // Puntuación obtenida
int attempts = 0; // Número de intentos
bool noteSelected = false; // Para saber si se seleccionó una nota
int selectedNote = -1; // Índice de la nota seleccionada en Modo 2

// Notas y frecuencias
const char* notas[] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "DoA"};
const double noteFrequencies[] = {261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25}; // Frecuencias de las notas
const char* gameOptions[5]; // Almacenar opciones del juego

uint16_t color = tft.color565(245,31,6);


// Buffer para almacenar frecuencias detectadas
const int BUFFER_SIZE = 50;
double frequencyBuffer[BUFFER_SIZE];
int bufferIndex = 0;
// configuración de firebase
#define ssid "NETLIFE-DOMINGO"
// Cambia esto por el nombre de usuario correcto
const char* contrasena = "Domingo-1974";
#define URL "https://beethoven-s-pitch-default-rtdb.firebaseio.com/"
#define APIKEY "AIzaSyCG576a6bKVFFX2KBFffw3VTJoR7_GAI7E"

#define usere "ancapueb@espol.edu.ec"
#define pass "Bee123"

FirebaseData myFireBaseData;
FirebaseConfig config;
FirebaseAuth auth; 
unsigned long dataMillis = 0;
int count = 0;
// 

// Variables de antirrebote
int lastButtonState1 = HIGH;
unsigned long lastDebounceTime1 = 0;
bool buttonPressed1 = false;

int lastButtonState2 = HIGH;
unsigned long lastDebounceTime2 = 0;
bool buttonPressed2 = false;

// Funciones de visualización
void displayMenu();
void displayWelcome();
void displayMode1();
void displayMode2();
void displayWiFiMode();
void displayGraphScreen(int noteIndex);
void displayMode3();
void displayListenScreen();
void displayGameOptions(const char* options[], int numOptions);
void displayResults();
void returnToMenu();
void app();
void returnToModo2();
void jpegRender(int xpos, int ypos);


// Función para manejar el antirrebote y la selección de botones
void handleButtonPresses();

// Funciones adicionales
void startGame();
void playRandomNote();
void checkAnswer();
double detectFrequency();
void drawGraph(double detectedFreq, double desiredFreq,int noteIndex2);
/*void drawSdJpeg(const char *filename, int xpos, int ypos) ;*/

void setup() {
  Serial.begin(115200);

  // Inicialización de la pantalla
  tft.init();
  tft.setRotation(1); // Modo horizontal
  tft.fillScreen(TFT_WHITE);

  // Inicialización del DFPlayer Mini
  softwareSerial.begin(9600);
  if (player.begin(softwareSerial)) {
    Serial.println("DFPlayer Mini conectado correctamente.");
    player.volume(25); // Establece el volumen (0 a 30)
  } else {
    Serial.println("¡Error al conectar con el DFPlayer Mini!");
  }

  // Configuración de los pines de los botones
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(BUTTON_INTERRUPT_PIN, INPUT_PULLUP);
  //potenciometro
  

  

  // Inicialización de FFT
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));

  // Inicializar el buffer de frecuencias
  memset(frequencyBuffer, 0, sizeof(frequencyBuffer));
  

  // Mostrar pantalla de bienvenida
  displayWelcome();
  delay(3000); // Mostrar "Bienvenidos" por 3 segundos

  // Mostrar el menú principal
  displayMenu();
  player.play(2);
  // ********** SD card **********
 /* if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    existSD = false;
    //return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    existSD = false;
    //return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);*/
  
}

void loop() {
  handleButtonPresses();
}

void displayWelcome() {
  //245,31,6
  uint16_t color = tft.color565(245,31,6); 
   
  tft.fillScreen(color);
  tft.setTextColor(TFT_BLACK, color);
  tft.setTextSize(3);
  int16_t x = (tft.width() - tft.textWidth("Bienvenidos")) / 2;
  int16_t y = (tft.height() - tft.fontHeight()) / 2;
  tft.setCursor(x, y);
  tft.println("Bienvenidos");
  /*drawSdJpeg("/Bee.jpg",1, 1);*/
  player.play(1); // Reproduce el archivo de bienvenida (1)
}

void displayMenu() {
  uint16_t color = tft.color565(245,31,6);
  uint16_t color2 = tft.color565(236,65,44); 
  tft.fillScreen(color); // Limpiar la pantalla
  inMenu = true;
  bool isPlaying = false;

    
  
  tft.setTextColor(TFT_RED, color); // Cambia el color a rojo
  tft.setTextSize(5); // Aumenta el tamaño del texto

  // Título del menú
  tft.fillRoundRect((tft.width() - tft.textWidth("Menu")) / 3, 10,  230, 60,7, TFT_BLACK);
  tft.fillRoundRect((tft.width() - tft.textWidth("Menu")) / 2.75, 20,  200, 40, 7, color);
  tft.setCursor((tft.width() - tft.textWidth("Menu")) / 2, 20);
  tft.println("Menu");

  tft.setTextSize(4); // Restablece el tamaño del texto
  tft.setTextColor(TFT_BLUE, color2); // Cambia el color a azul para los botones

  // Botones para los modos
  const char* modes[] = {"Modo 1", "Modo 2", "Modo 3", "WiFi"};
  int numModes = sizeof(modes) / sizeof(modes[0]);
  

  for (int i = 0; i < numModes; i++) {
    if (selectedMode == i) {
      tft.setTextColor(TFT_ORANGE, color);
      tft.fillRoundRect(100, 75+i*40,230, 30,3, color); // Borrar cualquier área azul no deseada + i*40
      tft.fillTriangle(110, 80+i*40, 110, 100+i*40, 130, 90+i*40, TFT_ORANGE);
      tft.setCursor((tft.width() - tft.textWidth(modes[i])) / 2, 80 + i*40);
      tft.println(modes[i]);
    } else {
      tft.setTextColor(TFT_BLACK, color);
      tft.setCursor((tft.width() - tft.textWidth(modes[i])) / 2, 80 + i*40);
      tft.println(modes[i]);
    }
 

  }
  // Texto en la parte inferior izquierda
  tft.setTextColor(color2, color); // Cambia el color a negro
  tft.setTextSize(3); // Ajusta el tamaño del texto si es necesario
  tft.setCursor(10, tft.height() - 30); // Posición en la esquina inferior izquierda
  tft.println("No te rindas, tu puedes!");
  
}

void displayMode1() {
  uint16_t color = tft.color565(245,31,6);
  player.stop();
  //uint16_t color2 = tft.color565(255, 66, 0);
  tft.fillScreen(color); // Limpiar la pantalla
  
  
  tft.setTextColor(TFT_BLACK, color);
  tft.setTextSize(4);
  uint16_t colors[]= {TFT_ORANGE, TFT_YELLOW, TFT_ORANGE, TFT_YELLOW, TFT_ORANGE,TFT_YELLOW,TFT_ORANGE,TFT_YELLOW,TFT_RED};
  const char* options[] = {"Do", "Re", "Mi", "Fa", "Sol","La","Si","DoA","Salir"};
 
  int numOptions = sizeof(options) / sizeof(options[0]);

  for (int i = 0; i < 5; i++) {
    if (selectedOption == i) {
      
      tft.setTextColor(colors[i], color);
      tft.fillRect(50, 45+ i*35, 50, 30, color); // Borrar cualquier área azul no deseada
      tft.setCursor((tft.width() - tft.textWidth(options[i]))/4,  75 + i*50);
      tft.println(options[i]);
      player.play(11);
    } else {
      tft.setTextColor(TFT_BLACK,color);
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 4, 75 + i*50);
      tft.println(options[i]);

    }
    tft.setTextColor(TFT_WHITE, color);
    tft.setCursor((tft.width() - tft.textWidth("..Afina tu memoria..")) / 2, 10);
    tft.println("..Afina tu memoria..");
  }
  for (int i = 5; i < numOptions; i++) {
    if (selectedOption == i) {
      tft.setTextColor(colors[i], color);
      tft.fillRect(200, 5+ i*35, 50, 30, color); // Borrar cualquier área azul no deseada
      tft.setCursor((tft.width() - tft.textWidth(options[i]))/1.25, -175 + i*50);
      tft.println(options[i]);
      player.play(11);
    } else {
      tft.setTextColor(TFT_BLACK,color);
      tft.setCursor((tft.width() - tft.textWidth(options[i]))/1.25, -175 +i*50);
      tft.println(options[i]);
    }
    tft.setTextColor(TFT_WHITE, color);
    tft.setCursor((tft.width() - tft.textWidth("..Afina tu memoria..")) / 2, 10);
    tft.println("..Afina tu memoria..");
  }
  

}

void displayMode2() {
  player.stop();
  tft.fillScreen(TFT_BLACK); // Limpiar la pantalla
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(4);
  const char* options[] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "DoA", "Salir"};
  int numOptions = sizeof(options) / sizeof(options[0]);


  for (int i = 0; i < 3; i++) {
    if (selectedOption == i) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.fillRect(5, 40 + i*30, 70, 30, TFT_BLACK); // Borrar cualquier área azul no deseada
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 6, 120 + i*50);
      tft.println(options[i]);
    } else {
      tft.setTextColor(color, TFT_BLACK);
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 6, 120 + i*50);
      tft.println(options[i]);
    }
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setCursor((tft.width() - tft.textWidth("...Play by heart...")) / 2, 10);
    tft.println("...Play by heart...");
  }
  for (int i = 3; i < 6; i++) {
    if (selectedOption == i) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.fillRect(130, 40 + i*30, 70, 30, TFT_BLACK); // Borrar cualquier área azul no deseada
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 2, -30 + i*50);
      tft.println(options[i]);
    } else {
      tft.setTextColor(color, TFT_BLACK);
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 2, -30 + i*50);
      tft.println(options[i]);
    }
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setCursor((tft.width() - tft.textWidth("...Play by heart...")) / 2, 10);
    tft.println("...Play by heart...");
  }
  for (int i = 6; i < numOptions; i++) {
    if (selectedOption == i) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.fillRect(40, 40 + i*30, 70, 30, TFT_BLACK); // Borrar cualquier área azul no deseada
      tft.setCursor((tft.width() - tft.textWidth(options[i])) /1.1, -185 + i*50);
      tft.println(options[i]);
    } else {
      tft.setTextColor(color, TFT_BLACK);
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 1.1, -185 + i*50);
      tft.println(options[i]);
    }
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setCursor((tft.width() - tft.textWidth("...Play by heart...")) / 2, 10);
    tft.println("...Play by heart...");
  }

}

void displayGraphScreen(int noteIndex) {
  uint16_t color = tft.color565(245,31,6);
  inMode2 = false; // Salir de Modo 2 para evitar interferencias
  tft.fillScreen(color); // Limpiar la pantalla
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(3);
  
  double desiredFreq = noteFrequencies[noteIndex]; // Frecuencia deseada para la nota
  double detectedFreq = 0.0;
  int noteIndex2= noteIndex;


  while (!inMode2) {
    
    detectedFreq = detectFrequency(); // Detectar frecuencia actual
    frequencyBuffer[bufferIndex] = detectedFreq; // Almacenar en el buffer
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE; // Incrementar índice circular

    drawGraph(detectedFreq, desiredFreq,noteIndex2); // Dibujar el gráfico

    // Dibujar botón "Salir"
    tft.fillRoundRect(310, 270, 120, 45, 4, TFT_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(TFT_ORANGE);
    tft.setCursor(330, 280);
    tft.println("Salir");

    // Esperar por la entrada del usuario
    if (digitalRead(BUTTON_PIN2) == LOW) {
      returnToModo2(); // Volver al menú del Modo 2
    }

    delay(700); // Refrescar la pantalla cada 500 ms
  }
}

void displayMode3() {
 
  tft.fillScreen(TFT_WHITE); // Limpiar la pantalla
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  const char* options[] = {"Start", "Salir"};
  int numOptions = sizeof(options) / sizeof(options[0]);

  for (int i = 0; i < numOptions; i++) {
    if (selectedOption == i) {
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
      tft.fillRect(50, 50 + i*40, 200, 30, TFT_WHITE); // Borrar cualquier área azul no deseada
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 2, 55 + i*40);
      tft.println(options[i]);
    } else {
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 2, 55 + i*40);
      tft.println(options[i]);
    }
  }
 
}

void displayWiFiMode() {
  inWiFiMode = true;
  tft.fillScreen(TFT_WHITE); // Limpiar la pantalla
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Escaneando redes WiFi...");

  // Escaneo de redes WiFi
  int n = WiFi.scanNetworks();
  tft.fillScreen(TFT_WHITE); // Limpiar pantalla después de escaneo

  if (n == 0) {
    tft.println("No se encontraron redes.");
  } else {
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.println("Redes disponibles:");
    for (int i = 0; i < n; ++i) {
      // Mostrar las redes disponibles con el SSID
      tft.println(WiFi.SSID(i));
      delay(10);
    }
  }

  // Botón "Salir"
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.fillRect(50, 220, 200, 30, TFT_BLUE);
  tft.setCursor((tft.width() - tft.textWidth("Salir")) / 2, 225);
  tft.println("Salir");
}

void displayListenScreen() {
  
  tft.fillScreen(TFT_ORANGE); // Limpiar la pantalla
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.setTextSize(7);
  tft.setCursor((tft.width() - tft.textWidth("Escucha....")) / 3, tft.height() /4 - 20);
  tft.println("Escucha....");
  tft.setTextColor(TFT_YELLOW, TFT_ORANGE);
  tft.setTextSize(3);
  tft.setCursor(20,190);
  tft.println("Que nota crees que esta");
  tft.setCursor(180,220);
  tft.println("sonando?");

  // Botón "Listo"
   tft.fillRoundRect(150, 260, 170, 50, 7, TFT_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setCursor((tft.width() - tft.textWidth("Lo tengo!")) / 2, tft.height() - 45);
  tft.println("Lo tengo!");}


void displayGameOptions(const char* options[], int numOptions) {
  tft.fillScreen(TFT_BLACK); // Limpiar la pantalla
  tft.setTextSize(4);
  tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  tft.setCursor(30, 10);
  tft.println("Guess Pitch ¿?");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);

  // Mostrar opciones de notas y botones adicionales
  for (int i = 0; i < numOptions; i++) {
    if (selectedOption == i) {
      tft.setTextColor(TFT_ORANGE, TFT_RED);
      tft.fillRoundRect((tft.width() - tft.textWidth(options[i])) / 2.55, 55 + i*50, 140, 50, 8, TFT_RED); // Borrar cualquier área azul no deseada
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 2, 70+ i*50);
      tft.println(options[i]);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor((tft.width() - tft.textWidth(options[i])) / 2, 65 + i*50);
      tft.println(options[i]);
    }
}

}

void displayResults() {
  app();
  tft.fillScreen(color);
  player.play(12);
  delay(2000);
  tft.setTextColor(TFT_BLACK, color);
  tft.setTextSize(4);
  tft.setCursor((tft.width() - tft.textWidth("Tu puntaje es:")) /3, tft.height() / 2 - 40);
  tft.println("Tu puntaje es:");
  player.play(12);
  delay(2000);
 
  if (60<ganancia){
        tft.setTextColor(TFT_GREEN, color);
        tft.setTextSize(5);
        tft.setCursor((tft.width() - tft.textWidth(String(ganancia) + " puntos")) / 2, tft.height() / 2 + 20);
        tft.println(String(ganancia) + " puntos");
        tft.setTextColor(TFT_YELLOW, color);
        tft.setTextSize(2);
        tft.setCursor(130, 250);
        tft.println("Tu memoria musical esta en su punto");
    }
    else if(40<ganancia && ganancia<60){
        tft.setTextColor(TFT_ORANGE, color);
        tft.setTextSize(5);
        tft.setCursor((tft.width() - tft.textWidth(String(ganancia) + " puntos")) / 2, tft.height() / 2 + 20);
        tft.println(String(ganancia) + " puntos");
        tft.setTextColor(TFT_YELLOW, color);
        tft.setTextSize(2);
        tft.setCursor(135, 250);
        tft.println("Tu memoria musical puede mejorar");
    }
    else {
        tft.setTextColor(TFT_RED, color);
        tft.setTextSize(5);
        tft.setCursor((tft.width() - tft.textWidth(String(ganancia) + " puntos")) / 2, tft.height() / 2 + 20);
        tft.println(String(ganancia) + " puntos");
        tft.setTextColor(TFT_YELLOW, color);
        tft.setTextSize(2);
        tft.setCursor(90, 250);
        tft.println("Tu memoria musical necesita ejercitarse");
    }

  if (millis() - dataMillis > 5000)
    {
        dataMillis = millis();
        Serial.printf("Set int... %s\n", Firebase.setInt(myFireBaseData, "/HelloPurr/usuario", ganancia) ? "ok" : myFireBaseData.errorReason().c_str());
    }
  delay(5000); // Mostrar resultados durante 5 segundos antes de regresar al menú
}

void returnToMenu() {
  inMenu = true;
  inMode1 = false;
  inMode2 = false;
  inMode3 = false;
  inWiFiMode = false;
  ganancia = 0; // Resetea el puntaje
  selectedOption = 0; // Reiniciar opción seleccionada
  displayMenu();
  player.play(2);
  
}

void returnToModo2() {
  inMenu = false;
  inMode1 = false;
  inMode2 = true;
  inMode3 = false;
  inWiFiMode = false;
  displayMode2();
}
void handleButtonPresses() {
  // Leer el estado de los botones con antirrebote
  int reading1 = digitalRead(BUTTON_PIN1);
  int reading2 = digitalRead(BUTTON_PIN2);

  // Antirrebote para BUTTON_PIN1 (Desplazarse en el menú)
  if (reading1 != lastButtonState1) {
    lastDebounceTime1 = millis();
  }
  if ((millis() - lastDebounceTime1) > DEBOUNCE_DELAY) {
    if (reading1 != buttonPressed1) {
      buttonPressed1 = reading1;
      if (buttonPressed1 == LOW) {
        if (inMenu) {
          // Alternar entre "Modo 1", "Modo 2", "Modo 3" y "WiFi"
          selectedMode = (selectedMode + 1) % 4;
          displayMenu();
        } else if (inMode1) {
          // Alternar entre las opciones en el modo 1
          selectedOption = (selectedOption + 1) % 9;
          displayMode1();
        } else if (inMode2) {
          // Alternar entre las opciones en el modo 2
          selectedOption = (selectedOption + 1) % 9;
          displayMode2();
        } else if (inWiFiMode) {
          // Solo una opción, "Salir"
          selectedOption = 0;
          displayWiFiMode();
        } else if (inMode3) {
          // Alternar entre las opciones del juego
          selectedOption = (selectedOption + 1) % (noteSelected ? 5 : 1); // Alterna según opciones disponibles
          if (noteSelected) {
            displayGameOptions(gameOptions, 5);
          } else {
            displayListenScreen();
          }
        }
      }
    }
  }

  // Antirrebote para BUTTON_PIN2 (Seleccionar opción)
  if (reading2 != lastButtonState2) {
    lastDebounceTime2 = millis();
  }
  if ((millis() - lastDebounceTime2) > DEBOUNCE_DELAY) {
    if (reading2 != buttonPressed2) {
      buttonPressed2 = reading2;
      if (buttonPressed2 == LOW) {
        if (inMenu) {
          if (selectedMode == 0) {
            inMenu = false;
            inMode1 = true;
            selectedOption = 0; // Reiniciar opción seleccionada
            displayMode1();
          } else if (selectedMode == 1) {
            inMenu = false;
            inMode2 = true;
            selectedOption = 0; // Reiniciar opción seleccionada
            displayMode2();
          } else if (selectedMode == 2) {
           
            
                inMenu = false;
                inMode3 = true;
                selectedOption = 0; // Reiniciar opción seleccionada
                startGame();
              
          }
          else if (selectedMode == 3)
          {
            inMenu = false;
            inWiFiMode = true;
            selectedOption = 0;
            displayWiFiMode();
          }
        } else if (inMode1) {
          switch (selectedOption) {
            case 0:
              player.play(3); // Reproduce "Do"
              delay(4000);    // Reproduce por 4 segundos
              player.stop();  // Detener reproducción
              displayMode1();
              break;
            case 1:
              player.play(4); // Reproduce "Re"
              delay(4000);    // Reproduce por 4 segundos
              player.stop();  // Detener reproducción
              displayMode1();
              break;
            case 2:
              player.play(5); // Reproduce "Mi"
              delay(4000);    // Reproduce por 4 segundos
              player.stop();  // Detener reproducción
              displayMode1();
              break;
            case 3:
              player.play(6); // Reproduce "Fa"
              delay(4000);    // Reproduce por 4 segundos
              player.stop();  // Detener reproducción
              displayMode1();
              break;
            case 4:
              player.play(7); // Reproduce "Sol"
              delay(4000);    // Reproduce por 4 segundos
              player.stop();  // Detener reproducción
              displayMode1();
              break;
            case 5:
              player.play(8); // Reproduce "La"
              delay(4000);    // Reproduce por 4 segundos
              player.stop();  // Detener reproducción
              displayMode1();
              break;
            case 6:
              player.play(9); // Reproduce "Si"
              delay(4000);    // Reproduce por 4 segundos
              player.stop();  // Detener reproducción
              displayMode1();
              break;
            case 7:
              
              player.play(10); // Reproduce "DoA"
              delay(4000);     // Reproduce por 4 segundos
              player.stop();   // Detener reproducción
              displayMode1();
              break;
            case 8:
              returnToMenu(); // Volver al menú principal
              break;
          }
        } else if (inMode2) {
          if (selectedOption == 8) {
            returnToMenu(); // Salir al menú principal
          } else {
            selectedNote = selectedOption;
            displayGraphScreen(selectedNote); // Mostrar gráfico para la nota seleccionada
          }
        } else if (inWiFiMode) {
          if (selectedOption == 0) {
            returnToMenu(); // Volver al menú principal
          }
        } 
        
        else if (inMode3) {
                  

          if (noteSelected) { // Si las opciones de nota están visibles
            if (selectedOption == 4) { // "Salir"
              displayResults();
              returnToMenu();
            } else if (selectedOption == 3) { // "Seguir"
              noteSelected = false; // Reiniciar selección de nota
              attempts++;
              if (attempts < 3) {
                playRandomNote();
              } else {
                displayResults();
                returnToMenu();
              }
            } else { // Opciones de notas
              checkAnswer();
            }
          } else { // Solo la pantalla de "Escucha"
            if (selectedOption == 0) { // Si se presiona "Listo"
              player.stop(); // Detener reproducción
              noteSelected = true; // Marca que el sonido ha sido escuchado
              selectedOption = 0; // Reiniciar opción seleccionada
              displayGameOptions(gameOptions, 5);
            }
          }
          
        }
      }
    }
  }

  // Actualizar los estados previos de los botones
  lastButtonState1 = reading1;
  lastButtonState2 = reading2;
}

void startGame() {
  
  
  attempts = 0;
  ganancia = 0;
  playRandomNote();

}

void playRandomNote() {
  

  
  randomSeed(millis()); // Seed aleatorio basado en el tiempo
  correctNoteIndex = random(0, 3); // Índice de la nota correcta en las opciones
  int noteToPlay = random(3, 11); // Reproduce una nota aleatoria entre 3 y 10
  Serial.print("Reproduciendo nota: "); 
  Serial.println(noteToPlay); // Imprime el número de archivo que debería estar sonando

  // Generar tres opciones incluyendo la correcta
  bool usedIndexes[8] = {false}; // Array para rastrear notas usadas
  int noteIndex = noteToPlay - 3;
  gameOptions[correctNoteIndex] = notas[noteIndex];
  usedIndexes[noteIndex] = true; // Marca la nota correcta como usada

  for (int i = 1; i < 3; i++) {
    int randomIndex;
    do {
      randomIndex = random(0, 8); // Índice aleatorio para notas
    } while (usedIndexes[randomIndex]); // Repetir hasta encontrar una nota no usada

    usedIndexes[randomIndex] = true; // Marca la nota como usada
    gameOptions[(correctNoteIndex + i) % 3] = notas[randomIndex];
  }
  gameOptions[3] = "Seguir"; // Botón "Seguir"
  gameOptions[4] = "Salir";  // Botón "Salir"

  displayListenScreen();
  player.play(noteToPlay); // Intenta reproducir la nota
  delay(4000); // Reproduce la nota por 4 segundos
  Serial.println("Nota detenida");

  noteSelected = false; // Resetear para el próximo ciclo
  }
  

void checkAnswer() {
  if (selectedOption == correctNoteIndex) {
    ganancia += 20;
  } else {
    ganancia += 0; // Puntaje 0 si la opción es incorrecta
  }
  noteSelected = true; // Marcar que se ha seleccionado una nota
}

double detectFrequency() {
  // Realizar la detección de la frecuencia mediante FFT
  for (int i = 0; i < SAMPLES; i++) {
    microseconds = micros();
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0;
    while (micros() < (microseconds + sampling_period_us)) {}
  }

  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

  double peakFreq = FFT.MajorPeak(vReal, SAMPLES, SAMPLING_FREQUENCY);
  return peakFreq;
}

void drawGraph(double detectedFreq, double desiredFreq, int noteIndex2) {
  tft.fillRect(0, 0, tft.width(), tft.height(), color); // Limpiar la pantalla
  tft.fillRoundRect(20, 5, 415, 40, 7, TFT_BLACK);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextSize(2.5);
  tft.setCursor((tft.width() - tft.textWidth("Entona la nota seleccionada:"))/3.5, 20);
  tft.println("Entona la nota seleccionada:");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(3);
  const char* options[] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "DoA"};
  tft.setCursor(375, 15);
  tft.println(options[noteIndex2]);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);

  // Dibujar ejes del gráfico
  tft.drawLine(50, 50, 50, 220, TFT_BLACK); // Eje Y
  tft.drawLine(50, 220, 400, 220, TFT_BLACK); // Eje X

  // Dibujar la frecuencia deseada
  int desiredY = map(desiredFreq, 0, 600, 220, 50); // Mapeo de frecuencia a la pantalla
  tft.drawLine(40, desiredY, 400, desiredY, TFT_GREEN);
  tft.setCursor(405, desiredY - 10);
  tft.print(desiredFreq, 2);

  // Dibujar las frecuencias detectadas del buffer
  for (int i = 0; i < BUFFER_SIZE; i++) {
    int x = map(i, 0, BUFFER_SIZE - 1, 50, 400);
    int y = map(frequencyBuffer[i], 0, 600, 220, 50);
    tft.fillCircle(x, y, 2, TFT_BLACK);
  }

  // Etiquetas
  tft.setCursor(150, 230);
  tft.println("Frecuencia (Hz)");
}

void app(){
//firebase
  WiFi.begin(ssid,contrasena);
  Serial.print("Conectado a la red:  ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);}  
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config.api_key = APIKEY;
  config.database_url = URL;
  auth.user.email=usere;
  auth.user.password= pass;
  Firebase.reconnectWiFi(true);
  String basep= "/UserData/";

  config.token_status_callback=tokenStatusCallback;
  config.max_token_generation_retry=5;
  
  Firebase.begin(&config, &auth);
  
  Serial.print("Conectado con éxito");
  Firebase.setDoubleDigits(5);} 




  /*void drawSdJpeg(const char *filename, int xpos, int ypos) {

  // Open the named file (the Jpeg decoder library will close it)
  File jpegFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library

  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  Serial.println("===========================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("===========================");

  // Use one of the following methods to initialise the decoder:
  bool decoded = JpegDec.decodeSdFile(jpegFile);  // Pass the SD file handle to the decoder,
  //bool decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)

  if (decoded) {
    // print information about the image to the serial port
    //jpegInfo();
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
}
void jpegRender(int xpos, int ypos) {

  //jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {    // While there is more data in the file
    pImg = JpegDec.pImage ;   // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ( (mcu_y + win_h) >= tft.height())
      JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }

  tft.setSwapBytes(swapBytes);

}*/


