// Bibliotheken für WLAN, Webserver, DHT11, Zeit, Schrittmotor, RGB-LED und HTTP-Requests einbinden
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <time.h>
#include <Stepper.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>

// Pin-Definitionen
#define DHTPIN 0                   // Pin, an dem der DHT11 angeschlossen ist
#define DHTTYPE DHT11              // Typ des Temperatursensors
#define motorPin1 2                // Pins für den Schrittmotor
#define motorPin2 3
#define motorPin3 4
#define motorPin4 5

#define LED_PIN 8                  // Pin für die RGB-LED
#define NUMPIXELS 1                // Nur eine LED wird verwendet

// Initialisierung des LED-Objekts
Adafruit_NeoPixel pixel(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// WLAN-Zugangsdaten
const char* ssid = "A1-62D7CA";
const char* password = "EC83B4PS45";

bool ledState = true;             // Variable, die angibt, ob die LED ein- oder ausgeschaltet ist

// Initialisierung der Sensor-, Motor- und Server-Objekte
DHT dht(DHTPIN, DHTTYPE);         // DHT11 initialisieren
WebServer server(80);             // Webserver auf Port 80 initialisieren
const int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution, motorPin1, motorPin2, motorPin3, motorPin4);

// Variablen für letzte Messung und Intervalle
String lastMeasurement = "";              // Letzte erfolgreiche Messung als String
unsigned long lastRead = 0;               // Zeitstempel der letzten Messung
const unsigned long interval = 20000;     // Intervall von 20 Sekunden zwischen den Messungen

// Arrays für Durchschnittswerte und Zeitstempel (12 x 5 Minuten = 1 Stunde)
const int maxDataPoints = 12;
String timestamps[maxDataPoints];
float avgTemperatures[maxDataPoints];
float avgHumidities[maxDataPoints];
int dataIndex = 0;

// Variablen zur Berechnung der Durchschnittswerte
float tempSum = 0;
float humSum = 0;
int readingsCount = 0;
unsigned long lastAverageTime = 0;

// Gleitende Filterung zur Ausreißererkennung (mit 5 letzten Messungen)
const int lastMeasuresSize = 5;
float lastTemps[lastMeasuresSize] = {0};
float lastHums[lastMeasuresSize] = {0};
int filterIndex = 0;
bool filterFilled = false;

// Funktion zur Zeitsynchronisation via NTP
void initTime() {
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 100000) {
    delay(500); // Warten, bis die Zeit gesetzt wurde
  }
}

// Funktion zum Setzen der LED-Farbe (RGB)
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show(); // Farbe anzeigen
}

// Gibt den aktuellen Zeitstempel als formatierten String zurück
String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}

// Funktion zur Erkennung von Ausreißern mit Toleranzbereich
bool isOutlier(float newValue, float* history, int size, float tolerance = 8.0) {
  int count = filterFilled ? size : filterIndex;
  if (count == 0) return false;

  float sum = 0;
  for (int i = 0; i < count; i++) sum += history[i];
  float avg = sum / count;

  return abs(newValue - avg) > tolerance;
}

void setup() {
  Serial.begin(115200);      // Serielle Schnittstelle starten
  pixel.begin();             // LED initialisieren
  setLED(255, 0, 0);         // Rot: Startstatus

  dht.begin();               // DHT-Sensor starten
  myStepper.setSpeed(10);    // Drehgeschwindigkeit des Motors festlegen

  // Mit WLAN verbinden
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  setLED(0, 0, 0);           // LED aus: WLAN verbunden
  Serial.println("WLAN verbunden!");
  Serial.println(WiFi.localIP());

  initTime();                // Zeit synchronisieren

  // HTTP-Route für Startseite
  // HTML und JS für die Website
  server.on("/", []() {
    server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html><html>  
<head><meta charset="UTF-8"><title>DHT11</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script>
async function updateData() {
  const res = await fetch('/data');
  const txt = await res.text();
  document.getElementById('data').innerText = txt;
}

async function toggleLED() {
  const res = await fetch('/toggleLED');
  const txt = await res.text();
  alert(txt);
}

let tempChart, humidityChart;

async function loadChartData() {
  const res = await fetch('/chartdata');
  const text = await res.text();
  const lines = text.trim().split("\n");

  const labels = [];
  const temps = [];

  lines.forEach(line => {
    const parts = line.split(",");
    if (parts.length === 2) {
      labels.push(parts[0]);
      temps.push(parseFloat(parts[1]));
    }
  });

  if (!tempChart) {
    tempChart = new Chart("myChart", {
      type: "line",
      data: {
        labels: labels,
        datasets: [{
          label: "Temperatur (°C) - Jeder Punkt represäntiert den Durchschnitt der letzten 5 Minuten. Jeder Zeitstempel steht für den Durchschnitt der letzten 5 Minuten zur Zeit des Zeitstempels (von ihm selber).",
          backgroundColor: "rgba(0,0,255,0.3)",
          borderColor: "rgba(0,0,255,1.0)",
          data: temps,
          fill: false
        }]
      },
      options: {
        scales: {
          x: { title: { display: true, text: 'Zeit' } },
          y: { title: { display: true, text: '°C' }, suggestedMin: 0, suggestedMax: 40 }
        }
      }
    });
  } else {
    tempChart.data.labels = labels;
    tempChart.data.datasets[0].data = temps;
    tempChart.update();
  }
}

async function loadHumidityData() {
  const res = await fetch('/humiditychartdata');
  const text = await res.text();
  const lines = text.trim().split("\n");

  const labels = [];
  const hums = [];

  lines.forEach(line => {
    const parts = line.split(",");
    if (parts.length === 2) {
      labels.push(parts[0]);
      hums.push(parseFloat(parts[1]));
    }
  });

  if (!humidityChart) {
    humidityChart = new Chart("humidityChart", {
      type: "line",
      data: {
        labels: labels,
        datasets: [{
          label: "Luftfeuchtigkeit (%) - Jeder Punkt represäntiert den Durchschnitt der letzten 5 Minuten. Jeder Zeitstempel steht für den Durchschnitt der letzten 5 Minuten zur Zeit des Zeitstempels (von ihm selber).",
          backgroundColor: "rgba(0,255,0,0.3)",
          borderColor: "rgba(0,255,0,1.0)",
          data: hums,
          fill: false
        }]
      },
      options: {
        scales: {
          x: { title: { display: true, text: 'Zeit' } },
          y: { title: { display: true, text: '%' }, suggestedMin: 0, suggestedMax: 100 }
        }
      }
    });
  } else {
    humidityChart.data.labels = labels;
    humidityChart.data.datasets[0].data = hums;
    humidityChart.update();
  }
}

setInterval(updateData, 5000);
setInterval(() => {
  loadChartData();
  loadHumidityData();
}, 60000);

window.onload = () => {
  updateData();
  loadChartData();
  loadHumidityData();
};
</script>
</head>
<body>
  <h2>Letzte Messung:</h2>
  <div id="data">Lade...</div>
  <br>
  <button onclick="toggleLED()">LED EIN/AUS</button>
  <br><br>
  <canvas id="myChart" width="400" height="200"></canvas>
  <br><br>
  <canvas id="humidityChart" width="400" height="200"></canvas>
</body></html>
    )rawliteral");
  });

  // Route für letzte Messdaten
  server.on("/data", []() {
    server.send(200, "text/plain", lastMeasurement);
  });

  // LED-Status über Webinterface umschalten
  server.on("/toggleLED", []() {
    ledState = !ledState;
    setLED(ledState ? 0 : 0, ledState ? 255 : 0, 0); // Wenn aktiv, grün
    server.send(200, "text/plain", ledState ? "LED eingeschaltet" : "LED ausgeschaltet");
  });

  // Temperatur-Durchschnittswerte für Diagramm
  server.on("/chartdata", []() {
    String data = "";
    for (int i = 0; i < maxDataPoints; i++) {
      int index = (dataIndex + i) % maxDataPoints;
      if (timestamps[index] != "") {
        data += timestamps[index] + "," + String(avgTemperatures[index], 2) + "\n";
      }
    }
    server.send(200, "text/plain", data);
  });

  // Luftfeuchtigkeits-Durchschnittswerte für Diagramm
  server.on("/humiditychartdata", []() {
    String data = "";
    for (int i = 0; i < maxDataPoints; i++) {
      int index = (dataIndex + i) % maxDataPoints;
      if (timestamps[index] != "") {
        data += timestamps[index] + "," + String(avgHumidities[index], 2) + "\n";
      }
    }
    server.send(200, "text/plain", data);
  });

  server.begin(); // Webserver starten
}

void loop() {
  server.handleClient(); // Anfragen verarbeiten

  // Wenn 20 Sekunden seit letzter Messung vergangen sind oder erste Messung
  if (lastRead == 0 || millis() - lastRead >= interval) {
    lastRead = millis(); // Zeit aktualisieren

    float humidity = dht.readHumidity();        // Feuchtigkeit messen
    float temperature = dht.readTemperature();  // Temperatur messen

    // Wenn Messwerte gültig sind
    if (!isnan(humidity) && !isnan(temperature)) {

      // Ausreißerprüfung
      if (isOutlier(temperature, lastTemps, lastMeasuresSize) || isOutlier(humidity, lastHums, lastMeasuresSize)) {
        Serial.println("Ausreißer erkannt, Messung wird ignoriert.");
        return; // Messung wird ignoriert
      }

      // Messung ins Filterarray eintragen
      lastTemps[filterIndex] = temperature;
      lastHums[filterIndex] = humidity;
      filterIndex = (filterIndex + 1) % lastMeasuresSize;
      if (filterIndex == 0) filterFilled = true;

      // Aktuelle Zeit und Messwerte speichern
      String time = getFormattedTime();
      lastMeasurement = "Zeit: " + time +
                        " - Feuchtigkeit: " + String(humidity, 2) + "%" +
                        ", Temperatur: " + String(temperature, 2) + "°C";
      Serial.println(lastMeasurement);

      // Für Durchschnitt berechnen
      tempSum += temperature;
      humSum += humidity;
      readingsCount++;

      // LED-Farbe an Temperatur anpassen (nur wenn LED aktiv)
      if (ledState) {
        if (temperature > 30) {
          setLED(255, 165, 0);  // Orange
        } else if (temperature >= 17.0 && temperature <= 30.0) {
          setLED(0, 255, 0);    // Grün
        } else {
          setLED(0, 0, 255);    // Blau
        }
      }

      // Durchschnitt alle 5 Minuten berechnen
      if (millis() - lastAverageTime >= 300000 || lastAverageTime == 0) {
        avgTemperatures[dataIndex] = tempSum / readingsCount;
        avgHumidities[dataIndex] = humSum / readingsCount;
        timestamps[dataIndex] = time;
        dataIndex = (dataIndex + 1) % maxDataPoints;

        // Rücksetzen für neue Durchschnittsberechnung
        tempSum = 0;
        humSum = 0;
        readingsCount = 0;
        lastAverageTime = millis();
      }

      // Schrittmotor 15 Sekunden nach Messung für 5 Sekunden drehen lassen
      unsigned long startMotor = millis();
      while (millis() - startMotor < 5000) {
        myStepper.step(stepsPerRevolution / 10); // Langsames Drehen
      }
    } else {
      Serial.println("Fehler beim Lesen vom DHT11");
    }
  }
}