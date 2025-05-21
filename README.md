# Wetterstation

## Eine ITP-Fächerkooperation

Verfasser: **Chatti Selim, Stättner Martin**

Datum: **10.05.2025**

## 1.  Einführung

Im Zuge des ITP und SYT Unterrichts kam es dazu eine einfache, kostengünstige Wetterstation für Bildungs- und Hobbyprojekte zu bauen. Mikrocontroller wie der ESP32-C3 ermöglichen die Realisierung solcher Systeme mit wenig Aufwand. Ziel dieses Projekts war es, eine Wetterstation zu erstellen, die Temperatur- und Luftfeuchtigkeitswerte misst und dabei zusätzlich einen Step-Motor steuert. Als "nice to have" wurde zusätzlich noch ein Discord-Bot ergänzt.

## 2.  Projektbeschreibung

Es wurde ein kompaktes System entwickelt, das mithilfe eines DHT11-Sensors regelmäßig Temperatur und Luftfeuchtigkeit misst. Die Daten werden seriell ausgegeben. Nach jeder Messung wird ein Step-Motor für fünf Sekunden aktiviert, um eine mechanische Bewegung auszuführen. In unserem Fall dreht er sich 5 Sekunden lang. Wie bereits erwähnt wurde auch ein Discord-Bot, welcher die aktuellen Messdaten, nachdem dies beantragt wurde, ausgiebt.

## 3.  Theorie

Die Wetterstation basiert auf dem ESP32-C3 Mikrocontroller, welcher über WLAN-Funktionalität verfügt und sich ideal für IoT-Projekte eignet. Der verwendete DHT11 ist ein einfacher digitaler Sensor zur Messung von Temperatur und Luftfeuchtigkeit. Er überträgt die Daten seriell über eine digitale Leitung. Der Step-Motor ist eine häufig genutzte Komponente zur präzisen mechanischen Bewegung, beispielsweise für Zeiger oder drehbare Module. Der Code für den ESP ist ein .ino-Code, während der Discord-Bot auf Pyhton läuft.

## 4.   Arbeitsschritt

Zuerst haben wir unseren ESP32C3, sowie das Treiberboard unsereres Schrittmotors, auf ein Breadboard gesteckt. Danach haben wir den 5V anschluss vom Motor mit dem 5V anschluss vom ESP32C3 verbunden. Genau das gleiche haben wir auch mit GND gemacht. Der INA Pin vom Motor wurde mit dem PIN 2 verbunden, INB mit Pin 3, INC mit PIN 4 und IND mit dem PIN 5. Der DHT11 wurde auch mit 5V, GND und dem PIN 0 verbunden. Nachdem wir das alles erledigt haben haben wir einen Code geschrieben mit dem wir alles zusammen laufen lassen können. Ebenso mussten wir für den Code Librarys in Arduino installieren damit der Code porblemlos läuft. 

Über den seriellen Monitor werden alle 20 Sekunden Messdaten, nachdem diese überprüft wurden, ausgegeben. Der Step-Motor ist so programmiert, dass er sich nach jeder abgeschlossenen Messung für fünf Sekunden dreht, bevor der Zyklus erneut beginnt.

### Code

Hier ist der Code für den ESP zu finden (HTML/JS wurde nicht in eine extra Datei gegeben um nur eine Datein zu haben, um welche sich gekümmert werden muss. Sprich, wenn es bei dieser Datei keine Probleme gibt funktioniert die ganze Wetterstation. Chat-GPT wurde teilweise zur Hilfe bei Problemen benutzt, bei denen wir selber nicht mehr weitergekommen sind.) : 

```c++
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
```
Hier ist der Python Code für den Discord-Bot zu finden (Dieser wurde mithilfe von Chat-GPT erstellt, da wir Python noch nicht gelernt haben.):

```python
# importiert die benötigten Module für den Discord-Bot und HTTP-Anfragen
import discord
from discord.ext import commands
import requests

# IP-Adresse des ESP32 – ACHTUNG: muss noch angepasst werden, falls sich die IP ändert.
# Verbesserungsidee: Automatische Erkennung der IP in Zukunft
ESP_IP = "http://10.0.0.128"

# Hinweis: Für den Python-Code wurde ChatGPT verwendet, da wir Python im Unterricht noch nicht behandelt haben.

# Token des Discord-Bots (aus dem Discord Developer Portal).
# WICHTIG: Token wurde hier ausgeblendet!
TOKEN = "xxx"

# Erstellt ein "intents"-Objekt, das dem Bot erlaubt, bestimmte Inhalte zu sehen/verarbeiten
# Hier wird vor allem das Recht aktiviert, Nachrichteninhalte zu lesen (für Befehle notwendig)
intents = discord.Intents.default()
intents.message_content = True  # notwendig, um Befehle im Chat auslesen zu können

# Erstellt den eigentlichen Bot mit dem Prefix ">" (also z. B. >ping)
bot = commands.Bot(command_prefix=">", intents=intents)

# Funktion wird ausgeführt, sobald der Bot gestartet ist und bereit ist
@bot.event
async def on_ready():
    print(f"Bot ist online als {bot.user}")  # Ausgabe in der Konsole, zur Bestätigung

# Der eigentliche Befehl: >ping
# Wenn dieser im Discord-Chat eingegeben wird, wird die Funktion darunter ausgeführt
@bot.command()
async def ping(ctx):
    try:
        # Sendet eine GET-Anfrage an den ESP32 an den Pfad /data
        response = requests.get(f"{ESP_IP}/data", timeout=5)

        # Wenn die Antwort vom ESP32 erfolgreich ist (HTTP-Statuscode 200)
        if response.status_code == 200:
            # Sendet die erhaltenen Daten vom ESP32 als Nachricht zurück an den Discord-Chat
            await ctx.send(f"📡 Letzte Messung:\n{response.text}")
        else:
            # Wenn die Verbindung zwar besteht, aber keine gültige Antwort kommt
            await ctx.send("❌ ESP32 antwortet nicht korrekt.")
    except requests.exceptions.RequestException as e:
        # Falls ein Verbindungsfehler auftritt (ESP32 offline, IP falsch, Timeout etc.)
        await ctx.send(f"❌ Fehler beim Verbinden mit dem ESP32:\n`{e}`")

# Startet den Bot mit dem angegebenen Token – ab hier ist der Bot live
bot.run(TOKEN)

```

### Bilder und Schaltungen

#### Schaltplan: 
![Schaltplan](https://github.com/mstaettner/Wetterstation/raw/img/Schaltplan.png)

#### Aufbau in echt während dem Betrieb:
![Aufbau irl während Betrieb](https://github.com/mstaettner/Wetterstation/raw/img/WhatsApp%20Bild%202025-05-20%20um%2017.30.47_40694277.jpg)

### Tabellen

| Komponente	| Beschreibung |
| ----------- | ------------ |
| ESP32-C3	| Mikrocontroller mit WLAN, programmierbar via USB |
| DHT11	Sensor |Sensor für Temperatur und Luftfeuchtigkeit |
| Step-Motor	| Schrittmotor mit Steuerung über Motortreiber |
| Breadboard	| Zum Aufbau der Schaltung |
| Jumperkabel / Kabel	| Verbindung der Komponenten |

Diese Tabelle zeigt die zentralen Komponenten des Projekts.

### Discord Bot
Um die Funktionalität der Wetterstation zu erweitern haben wir uns dazu entschieden einen Discord-Bot in unser Projekt zu integrieren. Dafür wurde die Programmiersprache Python benutzt. \
Der Code ist oben unter dem Punkt Code zu finden. \
Für dieses Pyhton-Programm mussten discord.py und requests.py installiert werden.
![Pip-Installationen](https://github.com/mstaettner/Wetterstation/raw/img/all_pip.jpg)

Der Code dieses Bots läuft auf dem Laptop von Martin Stättner, da es am einfachsten ist den Discord-Bot lokal zu hosten.\
Der Bot wird wie folgt gestartet:
![Bot online](https://github.com/mstaettner/Wetterstation/raw/img/Bot_online.jpg)

Eine erfolgreiche Ausgabe des Bots schaut wie folgt aus:\
![Erfolgreicher Ping](https://github.com/mstaettner/Wetterstation/raw/img/success%20ping.jpg)

## 5.  Zusammenfassung

Das Projekt demonstriert eine einfache Wetterstation auf Basis von günstiger Hardware, die zuverlässig Temperatur und Luftfeuchtigkeit erfasst. Der integrierte Step-Motor bietet Möglichkeiten zur Bewegungsausgabe, etwa für eine mechanische Anzeige.

Die Umsetzung verlief größtenteils problemlos. Eine Herausforderung war die gleichzeitige Ansteuerung des Motors und das Auslesen des Sensors, da beide zeitkritisch sind. Durch den Einsatz von millis konnte dies jedoch gelöst werden.
 
Beim Discord Bot gab es anfangs Probleme, da wir den Code online auf replit laufen lassen wollten und diesen von UptimeRobot ständig "online halten" wollten. Dies stellte sich aber als zu kompliziert heraus, weshalb wir dann auf den lokalen Bot umgestiegen sind.

## 6.  Quellen

(No date a) Chatgpt. Available at: https://chatgpt.com/ (Accessed: 20 May 2025). 

(No date b) YouTube. Available at: https://www.youtube.com/watch?v=dqYqE46zqok&ab_channel=Programmierenlernen (Accessed: 20 May 2025). 

Amos, D. et al. (2025) ESP32 web server - arduino IDE, Random Nerd Tutorials. Available at: https://randomnerdtutorials.com/esp32-web-server-arduino-ide/ (Accessed: 20 May 2025). 

Arduino - DHT11: Arduino tutorial (2025) Arduino Getting Started. Available at: https://arduinogetstarted.com/tutorials/arduino-dht11 (Accessed: 20 May 2025). 

Domenico et al. (2020) ESP32 NTP client-server: Get date and Time (arduino IDE), Random Nerd Tutorials. Available at: https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/ (Accessed: 20 May 2025). 

ESP32-C3-DevKitM-1 (no date) ESP. Available at: https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32c3/hw-reference/esp32c3/user-guide-devkitm-1.html#user-guide-c3-devkitm-1-v1-board-front (Accessed: 20 May 2025). 

joortiz22 (2023) 28BYJ-48 4-phase stepper motor, Arduino Forum. Available at: https://forum.arduino.cc/t/28byj-48-4-phase-stepper-motor/1162926 (Accessed: 20 May 2025). 

Schmidt, M. (2022) Dht11 temperatur sensor per arduino auslesen, Roboter. Available at: https://www.roboter-bausatz.de/projekte/dht11-temperatur-sensor-per-arduino-auslesen?srsltid=AfmBOopS9BJeyRJyWws816lUohxiCfME4eSfn0TZwJunxg6-xpVXj8ic (Accessed: 20 May 2025). 

Staff, L.E. (2022) In-depth: Create a simple ESP32 web server in Arduino Ide, Last Minute Engineers. Available at: https://lastminuteengineers.com/creating-esp32-web-server-arduino-ide/ (Accessed: 20 May 2025). 
Wokwi (no date) https://wokwi.com/projects/398783294547421185 (Accessed: 20 May 2025).
