#include <WiFi.h>
#include <HTTPClient.h>

// =============== CONFIGURACIÓN WIFI ===============
const char* ssid = "Vigilio-";
const char* password = "240124vg468";

// =============== CONFIGURACIÓN FIREBASE ===============
const char* firebaseURL = "https://overr-2463c-default-rtdb.firebaseio.com/oficina.json";

// =============== PINES DE SENSORES ===============
const int pinKY038 = 34;    // Micrófono AO → GPIO34
const int pinLDR   = 35;    // LDR → GPIO35
const int pinRELE  = 26;    // Relé → GPIO26

// =============== UMBRALES DE AMBIENTE ===============
const int RUIDO_BAJO = 50;     
const int RUIDO_MODERADO = 200;
const int RUIDO_ALTO = 400;

const int LUZ_BAJA = 2000;   // Menos de 2000 = oscuro → prender relé
const int LUZ_ALTA = 3000;   // Más de 3000 = brillante → apagar relé

// =============== VARIABLES DE ESTADO ===============
String estadoActual = "NORMAL";
unsigned long ultimoEnvio = 0;
const unsigned long INTERVALO_ENVIO = 3000; // 3 segundos
bool releEncendido = false;

// ==================================================
//   SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== SISTEMA DE MONITOREO DE OFICINA ===");

  pinMode(pinRELE, OUTPUT);
  digitalWrite(pinRELE, HIGH); // 🔹 Relé apagado al inicio (activo en LOW)

  conectarWiFi();
  Serial.println("Sistema listo - Monitoreando ambiente de oficina...");
}

// ==================================================
//   CONECTAR WIFI
// ==================================================
void conectarWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi conectado - IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n✗ Error conectando WiFi");
  }
}

// ==================================================
//   EVALUAR AMBIENTE
// ==================================================
String evaluarAmbiente(int sonido, int luz) {
  String estado = "";

  // Evaluar ruido
  if (sonido < RUIDO_BAJO) {
    estado = "SILENCIO";
  } else if (sonido < RUIDO_MODERADO) {
    estado = "RUIDO BAJO";
  } else if (sonido < RUIDO_ALTO) {
    estado = "RUIDO MODERADO";
  } else {
    estado = "RUIDO ALTO";
  }

  // ✅ Control del relé (activo en LOW)
  if (luz < LUZ_BAJA) {
    digitalWrite(pinRELE, LOW);   // Encender relé (activo en LOW)
    releEncendido = true;
    Serial.println("🌑 Oscuro → Relé ENCENDIDO");
  } else {
    digitalWrite(pinRELE, HIGH);  // Apagar relé (activo en LOW)
    releEncendido = false;
    Serial.println("☀ Luz detectada → Relé APAGADO");
  }

  return estado;
}

// ==================================================
//   ENVIAR DATOS A FIREBASE
// ==================================================
void enviarDatosFirebase(int sonido, int luz, String estado) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi desconectado - reconectando...");
    conectarWiFi();
    return;
  }

  HTTPClient http;
  http.begin(firebaseURL);
  http.addHeader("Content-Type", "application/json");

  // Crear JSON
  String json = "{";
  json += "\"sonido\":" + String(sonido) + ",";
  json += "\"luz\":" + String(luz) + ",";
  json += "\"estado\":\"" + estado + "\",";
  json += "\"rele\":\"" + String(releEncendido ? "ENCENDIDO" : "APAGADO") + "\",";
  json += "\"timestamp\":" + String(millis());
  json += "}";

  int codigo = http.PUT(json);

  if (codigo == 200) {
    Serial.println("✅ Datos enviados a Firebase");
  } else {
    Serial.printf("❌ Error HTTP: %d\n", codigo);
    if (codigo > 0) {
      Serial.println("Respuesta: " + http.getString());
    }
  }

  http.end();
}

// ==================================================
//   LOOP PRINCIPAL
// ==================================================
void loop() {
  int sonido = 0, luz = 0;

  // Promedio de 10 lecturas
  for (int i = 0; i < 10; i++) {
    sonido += analogRead(pinKY038);
    luz += analogRead(pinLDR);
    delay(10);
  }
  sonido /= 10;
  luz /= 10;

  estadoActual = evaluarAmbiente(sonido, luz);

  // Mostrar valores
  Serial.printf("🔊 Sonido: %d | 💡 Luz: %d | 🔌 Relé: %s\n",
                sonido, luz, releEncendido ? "ENCENDIDO" : "APAGADO");

  // Enviar datos cada INTERVALO_ENVIO
  if (millis() - ultimoEnvio > INTERVALO_ENVIO) {
    enviarDatosFirebase(sonido, luz, estadoActual);
    ultimoEnvio = millis();
  }

  delay(200);
}
