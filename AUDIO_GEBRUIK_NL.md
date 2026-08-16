Door middel van een website die op de esp draait kunnen files worden ge-upload en directories aangemaakt

# Audiogebruik en keuzeboom

## 1. Opslag

De eerste versie gebruikt FFat in het interne flashgeheugen van de ESP32-S3
N16R8. Er is geen SD-kaart nodig.

De root van het bestandssysteem bevat minimaal:

```text
/
├── main.mp3
├── error.mp3
└── keyflow.txt
```

- `/keyflow.txt` bevat `ini=main.mp3`; dit bepaalt welk bestand bij het
  opnemen van de hoorn wordt gestart.
- `/main.mp3` is het standaard startbestand, maar de bestandsnaam is vrij.
- `/error.mp3` is de foutmelding van de rootdirectory.
- `/keyflow.txt` bevat de toetskeuzes voor het hoofdniveau.
- De rootdirectory volgt daarmee dezelfde regels als alle subdirectories.

## 2. Directorystructuur van keuzes

Een toetskeuze in `keyflow.txt` heeft twee mogelijke typen:

1. een MP3-bestand uit de huidige directory afspelen;
2. naar een directory gaan, de `keyflow.txt` daar laden en het lokale
   `ini`-bestand afspelen.

Iedere directory waarnaar wordt verwezen bevat minimaal:

1. een eigen `keyflow.txt`;
2. het via `ini=` aangewezen MP3-bestand.
3. een `error.mp3` voor lokale foutafhandeling.

De MP3-bestandsnamen zijn vrij. Ze hoeven dus niet gelijk te zijn aan de naam
van de directory.

Voorbeeld:

```text
/
├── main.mp3
├── error.mp3
├── keyflow.txt
├── informatie/
│   ├── keyflow.txt
│   ├── error.mp3
│   ├── welkom_informatie.mp3
│   └── openingstijden/
│       ├── keyflow.txt
│       ├── error.mp3
│       └── tijden_vandaag.mp3
└── verhalen/
    ├── keyflow.txt
    ├── error.mp3
    ├── introductie_verhalen.mp3
    └── kort/
        ├── keyflow.txt
        ├── error.mp3
        └── het_korte_verhaal.mp3
```

Een directorynaam identificeert het keuzeknooppunt. De lokale
`keyflow.txt` bepaalt welk vrij benoemd MP3-bestand in die directory wordt
afgespeeld.

## 3. Verwijzingen

Iedere `keyflow.txt` begint met `ini=`. Deze regel wijst het MP3-bestand aan
dat als eerste wordt afgespeeld wanneer de betreffende directory actief
wordt.

Voorbeeld voor `/keyflow.txt`:

```ini
ini=main.mp3
1=extra_uitleg.mp3
2=/informatie
no_key=next
```

Dit betekent:

- bij het starten van de rootsessie wordt `/main.mp3` afgespeeld;
- toets `1` speelt `/extra_uitleg.mp3` in de huidige directory;
- toets `2` gaat naar de directory `/informatie`;
- als de audio eindigt zonder toetskeuze, wordt alfabetisch het volgende
  normale MP3-bestand uit de root gestart.

Voorbeeld voor `/informatie/keyflow.txt`:

```ini
ini=welkom_informatie.mp3
1=meer_informatie.mp3
2=/informatie/openingstijden
no_key=repeat
```

Hiermee wordt bij het betreden van `/informatie` het volgende bestand
afgespeeld:

```text
/informatie/welkom_informatie.mp3
```

Toets `1` speelt het volgende bestand af zonder van directory te wisselen:

```text
/informatie/meer_informatie.mp3
```

Toets `2` gaat naar:

```text
/informatie/openingstijden
```

Voorbeeld van een eindknooppunt
`/informatie/openingstijden/keyflow.txt`:

```ini
ini=tijden_vandaag.mp3
no_key=silence
```

Een bestand zonder toetsverwijzingen is een geldig eindknooppunt.

## 4. Syntaxis van `keyflow.txt`

### Eerste audiobestand

```ini
ini=welkom.mp3
```

- `ini` is verplicht.
- De waarde is een MP3-bestand in dezelfde directory als `keyflow.txt`.
- Het bestand wordt gestart zodra de directory actief wordt.

### Toets speelt een bestand

```ini
1=file_1.mp3
```

- Een waarde die eindigt op `.mp3` verwijst naar een bestand in de huidige
  directory.
- De actieve directory en de actieve `keyflow.txt` veranderen niet.
- Het momenteel afgespeelde bestand wordt gestopt en het gekozen bestand
  wordt gestart.

### Toets gaat naar een directory

```ini
2=/next_dir
```

- Een waarde die begint met `/` verwijst naar een directory vanaf de root van
  FFat.
- In deze directory moet een `keyflow.txt` staan.
- De nieuwe `keyflow.txt` wordt actief.
- Het MP3-bestand uit de lokale `ini=`-regel wordt gestart.

Een geneste directory wordt als volledig pad geschreven:

```ini
2=/informatie/openingstijden
```

### Geen toets ingedrukt

`no_key` bepaalt wat er gebeurt wanneer het huidige MP3-bestand eindigt zonder
dat een geldige toetskeuze is gemaakt.

Er zijn precies drie mogelijkheden.

Alfabetisch het volgende normale MP3-bestand uit dezelfde directory afspelen:

```ini
no_key=next
```

De speler maakt hiervoor een alfabetisch gesorteerde lijst van alle
MP3-bestanden in de actieve directory. Na het laatste bestand wordt weer het
eerste bestand gestart. Dit blijft doorgaan totdat een geldige toets wordt
losgelaten of de hoorn wordt neergelegd.

De sortering is niet hoofdlettergevoelig. `error.mp3` is gereserveerd voor
foutafhandeling en wordt overgeslagen. Het via `ini=` aangewezen bestand en
alle overige MP3-bestanden in de directory maken wel deel uit van de lus.

Het laatst afgespeelde MP3-bestand blijven herhalen:

```ini
no_key=repeat
```

Stil blijven en op een geldige toets of het neerleggen van de hoorn wachten:

```ini
no_key=silence
```

Bij alle drie de opties blijven geldige toetskeuzes actief.

## 5. Regels voor de keuzeboom

- De rootdirectory is het beginniveau.
- Bij het opnemen van de hoorn wordt `/keyflow.txt` geladen en het via `ini=`
  aangewezen MP3-bestand gestart.
- `/keyflow.txt` bepaalt welke keuzes op het beginniveau geldig zijn.
- Iedere `keyflow.txt` moet via `ini=` haar eerste MP3-bestand aanwijzen.
- Een toetswaarde die eindigt op `.mp3` speelt een bestand in de huidige
  directory.
- Een toetswaarde die begint met `/` activeert die directory.
- Iedere geactiveerde directory moet een `keyflow.txt` bevatten.
- MP3-bestandsnamen zijn vrij.
- Directoryverwijzingen zijn absolute paden vanaf de FFat-root.
- Verwijzingen mogen niet `..` gebruiken.
- De keuzeboom ondersteunt maximaal vier opeenvolgende directorykeuzes vanaf
  de root.
- Een ontbrekende toetsverwijzing wordt genegeerd; de huidige MP3 blijft
  spelen.
- Een geldige toets onderbreekt de huidige MP3 en voert de gekoppelde
  bestands- of directoryactie uit.
- Een keuze wordt pas verwerkt nadat de toets stabiel is losgelaten.
- `no_key` accepteert alleen `next`, `repeat` of `silence`.

## 6. Sessiegedrag

### Hoorn opnemen

1. De actieve directory wordt `/`.
2. `/keyflow.txt` wordt geladen.
3. Het via `ini=` aangewezen hoofdbestand, standaard `/main.mp3`, wordt
   afgespeeld.
4. Het toetsenbord wordt tijdens het afspelen gescand.

### Keuze maken

1. De gebruiker drukt een geldige toets in en laat deze los.
2. De huidige MP3 wordt gestopt.
3. Bij een MP3-verwijzing wordt het bestand uit de huidige directory gestart.
4. Bij een directoryverwijzing wordt de gekoppelde directory geopend.
5. De lokale `keyflow.txt` wordt geladen.
6. Het via `ini=` aangewezen MP3-bestand wordt gestart.

### Hoorn neerleggen

1. De actieve MP3 wordt onmiddellijk gestopt.
2. De actieve directory en keuzestatus worden gewist.
3. Toetsacties worden genegeerd.
4. De speler wacht tot de hoorn opnieuw wordt opgenomen.

Bij de volgende keer opnemen begint de speler opnieuw bij `/main.mp3` en
`/keyflow.txt`.

## 7. Foutafhandeling

Iedere directory hoort een gereserveerd bestand te bevatten met exact de naam:

```text
error.mp3
```

Dit bestand wordt nooit door `no_key=next` afgespeeld en wordt uitsluitend
voor foutafhandeling gebruikt.

### Fout in een normaal bestand, toetsdoel of `keyflow.txt`

1. Stop de huidige afspeelactie.
2. Speel `error.mp3` uit de actieve directory.
3. Laad daarna de actieve `keyflow.txt` opnieuw.
4. Speel het bestand uit de lokale `ini=`-regel.

### Fout in het lokale `ini`-bestand

Als het bestand uit `ini=` ontbreekt, ongeldig of niet afspeelbaar is, mag de
speler niet opnieuw hetzelfde herstelpad blijven uitvoeren:

1. Ga één directoryniveau omhoog.
2. Gebruik daar `error.mp3` als dit bestand aanwezig en afspeelbaar is.
3. Laad daarna de `keyflow.txt` van die bovenliggende directory.
4. Speel het daar via `ini=` aangewezen bestand.

### Ontbrekende of defecte `error.mp3`

Als `error.mp3` in de actieve directory ontbreekt of niet afspeelbaar is:

1. Ga één directoryniveau omhoog.
2. Probeer daar de lokale `error.mp3`.
3. Ga daarna naar het lokale `ini`-bestand.

Deze procedure herhaalt zich zo nodig tot de rootdirectory.

### Fout in de rootdirectory

De root bevat eveneens:

```text
/keyflow.txt
/error.mp3
```

De `ini=`-regel van `/keyflow.txt` bepaalt de startaudio bij het opnemen van
de hoorn. Als zowel herstel via `/error.mp3` als het root-`ini`-bestand
onmogelijk is, stopt de speler alle audio en blijft hij stil totdat de hoorn
wordt neergelegd. Zo wordt een oneindige foutlus voorkomen.

## 8. Aanbevolen MP3-formaat

Voor gesproken tekst via een telefoonhoorn:

```text
Container/codec: MP3
Kanalen:         mono
Samplefrequentie: 22.050 Hz
Bitrate:         32 kbit/s CBR
```

Dit is het aanbevolen compromis tussen verstaanbaarheid en opslagduur.

Alternatieven:

| Formaat | Geschatte totale duur bij 11,5 MiB |
|---|---:|
| Mono, 44,1 kHz, 64 kbit/s | circa 25 minuten |
| Mono, 22,05 kHz, 48 kbit/s | circa 33 minuten |
| Mono, 22,05 kHz, 40 kbit/s | circa 40 minuten |
| Mono, 22,05 kHz, 32 kbit/s | circa 50 minuten |
| Mono, 16 kHz, 24 kbit/s | circa 67 minuten |
| Mono, 16 kHz, 16 kbit/s | circa 100 minuten |

De werkelijke totale duur hangt af van de definitieve partitietabel,
bestandssysteemoverhead, MP3-metadata en firmwaregrootte.

## 9. Bestandsvalidatie

Bij het laden van een knooppunt controleert de firmware minimaal:

- bestaat de directory;
- bestaat de lokale `keyflow.txt`;
- bevat `ini=` een veilige MP3-bestandsnaam;
- bestaat het genoemde MP3-bestand;
- eindigt de bestandsnaam op `.mp3`;
- blijft het maximale aantal van vier keuzeniveaus gehandhaafd;
- verwijst iedere toets naar een bestaand MP3-bestand of een bestaande
  directory;
- bevat `no_key` `next`, `repeat` of `silence`;
- wordt `error.mp3` uitgesloten van de normale `next`-afspeellijst.

Een fout in één knooppunt mag de firmware niet laten vastlopen. De fout wordt
via Serial en de statusled gemeld. De verdere foutafhandeling, bijvoorbeeld
terugkeren naar root of wachten op het neerleggen van de hoorn, moet nog
worden bepaald.
