# importiert die benötigten Module für den Discord-Bot und HTTP-Anfragen
import discord
from discord.ext import commands
import requests

# IP-Adresse des ESP32 – ACHTUNG: muss noch angepasst werden, falls sich die IP ändert.
# Verbesserungsidee: Automatische Erkennung der IP in Zukunft
ESP_IP = "http://10.0.0.128"

# Hinweis: Für den Python-Code wurde ChatGPT verwendet, da wir Python im Unterricht noch nicht behandelt haben.

# Token des Discord-Bots (aus dem Discord Developer Portal).
# WICHTIG: Token niemals öffentlich machen – hier zu Demonstrationszwecken.
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
