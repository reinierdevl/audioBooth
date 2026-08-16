# Use-cases ESP32-S3 telefoon-MP3-speler

## 1. Doel en uitgangspunten

Het apparaat is een zelfstandige MP3-speler op basis van een ESP32-S3 N16R8
met een ES8311-audiocodec en een 4x4-toetsenbord. De speler gebruikt de
telefoonhoorn van het telephone-booth-project.

- Audio wordt uit het interne flashgeheugen afgespeeld.
- Er is geen externe server of cloudverbinding nodig.
- De eerste uitvoering gebruikt FFat.
- Een SD-kaart is nog niet aanwezig, maar kan later worden toegevoegd.
- De firmware wordt ontwikkeld voor de Arduino IDE in C++/Arduino-stijl.
- De ES8311 gebruikt I2S-BCLK als interne klokbron; een aparte MCLK wordt niet
  gebruikt.

## 2. Hardware

### Hoorncontact

| Functie | GPIO |
|---|---:|
| Hoorncontact | 1 |

### 4x4-toetsenbord

| Functie | GPIO |
|---|---:|
| Rij 0 | 4 |
| Rij 1 | 5 |
| Rij 2 | 6 |
| Rij 3 | 7 |
| Kolom 0 | 15 |
| Kolom 1 | 16 |
| Kolom 2 | 17 |
| Kolom 3 | 18 |

De definitieve fysieke toetsindeling moet tijdens een hardwaretest worden
vastgesteld.

### ES8311

| Functie | GPIO |
|---|---:|
| I2C SDA | 8 |
| I2C SCL | 9 |
| I2S DOUT | 10 |
| I2S BCLK | 12 |
| I2S WS/LRCK | 13 |

GPIO 11 (MCLK) en GPIO 14 (DIN) worden voor deze toepassing niet gebruikt.

### Overige aansluitingen

| Functie | GPIO |
|---|---:|
| WS-statusled | 48 |
| Booth-ID 0 | 39 |
| Booth-ID 1 | 40 |
| Booth-ID 2 | 41 |
| Booth-ID 3 | 42 |

De Booth-ID-ingangen hebben in de eerste versie geen vastgelegde functie.

## 3. Spelertoestanden

### 3.1 Hoorn neergelegd: stand-by

Bij het neerleggen van de hoorn:

1. De actieve MP3 wordt onmiddellijk gestopt.
2. Audiobuffers worden leeggemaakt.
3. De actieve keuzeboom en de huidige keuze worden gewist.
4. Toetsacties worden genegeerd.
5. De speler wacht tot de hoorn opnieuw wordt opgenomen.
6. De webinterface wordt beschikbaar volgens de actieve wifi-modus.

### 3.2 Hoorn opgenomen: actieve sessie

Bij het opnemen van de hoorn:

1. De webinterface wordt onmiddellijk geblokkeerd.
2. Een eventueel achtergebleven sessie wordt gewist.
3. De speler gaat naar de root van de keuzeboom.
4. De speler leest `/keyflow.txt`.
5. Het via `ini=` aangewezen MP3-bestand wordt gestart; standaard is dit
   `/main.mp3`.
6. Het toetsenbord wordt tijdens het afspelen continu gescand.
7. Een geldige keuze wordt uitgevoerd nadat de toets stabiel is losgelaten.

Iedere keer dat de hoorn wordt opgenomen, begint een volledig nieuwe sessie.
Eerder afgespeelde audio of een eerdere menupositie wordt niet hervat.

## 4. Audio afspelen

### UC-01: Hoofdaudio starten

**Voorwaarde:** de hoorn ligt neer.

**Gebeurtenis:** de gebruiker neemt de hoorn op.

**Resultaat:** de speler opent `/keyflow.txt` en speelt het MP3-bestand uit de
`ini=`-regel. De standaardconfiguratie gebruikt `ini=main.mp3`.

### UC-02: Keuzetoets verwerken

**Voorwaarden:**

- De hoorn is opgenomen.
- De toets is stabiel ingedrukt en daarna stabiel losgelaten.
- De actieve `keyflow.txt` bevat een koppeling voor deze toets.

**Resultaat:**

1. De huidige MP3 wordt onderbroken.
2. Als de toets naar een MP3-bestand verwijst, wordt dat bestand uit de
   huidige directory gestart.
3. Als de toets naar een directory verwijst, wordt de `keyflow.txt` in die
   directory geladen en het via `ini=` aangewezen MP3-bestand gestart.
4. Alleen bij een directoryverwijzing wordt de nieuwe directory het actieve
   knooppunt.

Als de actieve `keyflow.txt` geen koppeling voor de toets bevat, blijft de
huidige MP3 spelen.

### UC-03: Keuzeboom doorlopen

- De keuzeboom ondersteunt maximaal vier keuzeniveaus.
- `ini=` bepaalt welk MP3-bestand als eerste wordt afgespeeld bij het betreden
  van een directory.
- Een geldige toets heeft één van twee acties:
  - een vrij benoemd MP3-bestand uit de huidige directory afspelen;
  - naar een andere directory gaan en daar het via `ini=` aangewezen
    MP3-bestand afspelen.
- Iedere directory waarnaar een toets verwijst, bevat een eigen
  `keyflow.txt`.
- Als geen toets wordt ingedrukt en het huidige MP3-bestand eindigt, bepaalt
  `no_key=` of alfabetisch het volgende normale MP3-bestand wordt gestart, het
  laatst afgespeelde bestand wordt herhaald of de speler stil blijft.
- Bij `no_key=next` wordt na het alfabetisch laatste normale MP3-bestand weer
  het eerste normale MP3-bestand uit dezelfde directory gestart.
- `error.mp3` is gereserveerd voor foutafhandeling en maakt geen deel uit van
  de normale alfabetische afspeellus.

### UC-04: Fout tijdens audio of keuzeboom

Bij een fout in de actieve directory:

1. De speler probeert `/actieve_directory/error.mp3` af te spelen.
2. Na deze foutmelding probeert de speler opnieuw het via `ini=` aangewezen
   MP3-bestand van de actieve directory te starten.
3. Als juist dit `ini`-bestand het probleem veroorzaakte, of als
   `error.mp3` ontbreekt of niet afspeelbaar is, gaat de speler één niveau
   omhoog.
4. In de bovenliggende directory probeert de speler daar de foutmelding en
   het `ini`-bestand te gebruiken.
5. Deze terugval kan doorgaan tot de rootdirectory.

Ook de rootdirectory bevat een `keyflow.txt`, een via `ini=` aangewezen
startbestand en een `/error.mp3`. Als herstel in de root niet mogelijk is,
blijft de speler stil totdat de hoorn wordt neergelegd.

### UC-05: Sessie beëindigen

**Gebeurtenis:** de gebruiker legt de hoorn neer.

**Resultaat:** alle speleractiviteiten stoppen onmiddellijk en het apparaat
gaat terug naar stand-by.

## 5. Toetsenbord

### UC-06: Toetsen scannen tijdens audio

- Het 4x4-toetsenbord wordt iedere 5 tot 10 ms gescand.
- De scan is niet-blokkerend.
- Debouncing gebruikt geen `delay()`.
- Een keuze-event ontstaat pas bij het stabiel loslaten van een toets.
- Het scannen mag de MP3-weergave niet hoorbaar beïnvloeden.

Een afzonderlijke keypadtaak is in eerste instantie niet nodig. De
audiolibrary gebruikt de voorzieningen van de dual-core ESP32-S3 en I2S-DMA.

## 6. Wifi en webinterface

### UC-07: Verbinden met ingesteld SSID

1. Bij het opstarten leest het apparaat de opgeslagen wifi-instellingen.
2. Het apparaat probeert verbinding te maken met het ingestelde SSID.
3. Bij succes wordt de webinterface via het bestaande lokale netwerk
   aangeboden.

Wanneer de hoorn neerligt, zijn in deze modus de volgende functies
beschikbaar:

- bestanden uploaden;
- directories maken;
- bestanden downloaden.

### UC-08: Eigen hotspot starten

Als geen verbinding met het ingestelde SSID kan worden gemaakt, start het
apparaat een eigen wifi-hotspot.

Wanneer de hoorn neerligt, zijn in deze modus alleen de volgende functies
beschikbaar:

- bestanden uploaden;
- directories maken.

Downloaden via de eigen hotspot is niet beschikbaar.

### UC-09: Webinterface blokkeren

Wanneer de hoorn is opgenomen, zijn geen activiteiten via de webinterface
mogelijk. Dit geldt voor:

- openen of bekijken van webpagina's;
- bekijken van bestanden en directories;
- uploaden;
- directories maken;
- downloaden;
- wifi-instellingen of andere HTTP-functies.

Ieder verzoek krijgt de configureerbare melding:

```text
User interface not available, device in use
```

Wanneer de hoorn wordt neergelegd, wordt de webinterface automatisch weer
beschikbaar. Een herstart is niet nodig.

### Beschikbaarheidstabel

| Netwerkmodus | Hoorn | Webpagina | Upload | Directory maken | Download |
|---|---|---:|---:|---:|---:|
| Bestaand SSID | Neergelegd | Ja | Ja | Ja | Ja |
| Bestaand SSID | Opgenomen | Nee | Nee | Nee | Nee |
| Eigen hotspot | Neergelegd | Ja | Ja | Ja | Nee |
| Eigen hotspot | Opgenomen | Nee | Nee | Nee | Nee |

## 7. Configuratie

Voorgestelde systeeminstelling in `/config.txt`:

```ini
web_busy_message=User interface not available, device in use
```

- `web_busy_message` bepaalt de melding wanneer de webinterface is
  geblokkeerd.
- Ontbrekende waarden vallen terug op de standaardwaarden.
- Onbekende configuratieregels worden genegeerd.

Het startbestand van de telefoonspeler staat niet in `/config.txt`, maar in de
`ini=`-regel van `/keyflow.txt`.

## 8. Buiten scope van de eerste versie

- Externe server- of clouddiensten.
- Internetstreams.
- Webactiviteiten terwijl de hoorn is opgenomen.
- Downloaden via de eigen hotspot.
- Opnemen via de ES8311.
- Firmware-update via de webinterface.
- Een SD-kaart.
- Bestanden verwijderen of hernoemen via de webinterface.

## 9. Nog te bepalen

- Definitieve fysieke toetsindeling.
- Manier waarop SSID en wachtwoord voor het eerst worden ingesteld.
- Naam en beveiliging van de eigen hotspot.
- Betekenis van de statusledkleuren.
- Definitieve FFat-partitiegrootte.
