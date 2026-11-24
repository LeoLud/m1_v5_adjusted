# MonsGeek M1 V5 VIA ISO – angepasste Firmware

Inoffizieller ISO-Port der verbesserten Firmware von
[atorata/m1_v5_us_firmware](https://github.com/atorata/m1_v5_us_firmware) mit eigenen Erweiterungen.
Die ISO-spezifischen Werte (Layout, LED-Zuordnung, USB-ID) stammen aus MonsGeeks offiziellem
Quellcode (`keyboards/monsgeek/m1_v5/m1_v5_uk`, Branch `wireless`).

> ⚠️ **Nutzung auf eigenes Risiko.** Nur für die **M1 V5 VIA ISO** (USB-ID `342D:E4C2`).
> Nicht für die US/ANSI-Version, die M1 V5 TMR/HE oder andere Modelle.

---

## Inhalt

1. [Funktionen](#1-funktionen)
2. [Tastenbelegung der Fn-Ebene](#2-tastenbelegung-der-fn-ebene)
3. [Vorbereitung](#3-vorbereitung)
4. [Firmware bauen (GitHub Actions)](#4-firmware-bauen-github-actions)
5. [Firmware flashen](#5-firmware-flashen)
6. [VIA einrichten](#6-via-einrichten)
7. [OpenRGB einrichten](#7-openrgb-einrichten)
8. [Firmware anpassen](#8-firmware-anpassen)
9. [Notfall und Wiederherstellung](#9-notfall-und-wiederherstellung)
10. [Fehlerbehebung](#10-fehlerbehebung)
11. [Lizenz](#11-lizenz)

---

## 1. Funktionen

**Aus der Firmware von atorata übernommen**

- Höhere maximale Helligkeit (200 statt 104 von 255)
- Kein Aufblinken beim Ändern von Helligkeit, Tempo oder Effekt
- Schnellere Tastenreaktion, NKRO bleibt nach Werksreset erhalten
- Mausfunktionen in VIA belegbar
- OpenRGB-Unterstützung (VialRGB) inklusive Direct-Modus
- Einstellungen bleiben beim Flashen erhalten (außer beim Flashen mit Esc)

**ISO-Port und eigene Erweiterungen**

- ISO-Layout mit `<>|`, `#` und großem Enter, korrekte LED-Zuordnung
- Verbindungen auf **Fn + 1–5** mit Rückmeldung auf derselben Taste
- **Fn gehalten:** F1–F4 zeigen die aktive Ebene (grün), 1–5 den Verbindungsmodus, die Leertaste den Akkustand
- **Nach dem Aufwachen:** (abschaltbar mit Fn + I) 1 Sekunde Akkustand (Leertaste) und Verbindungsmodus (1–5)
- **Caps Lock** leuchtet weiß mit 100 %
- **Kabelmodus:** Kabel steckt, PC aus/Standby → nach 5 Sekunden Schlafzustand (alles dunkel). Kein Kabel → Taste 5 blinkt, nach 30 Sekunden Schlaf
- **Standardbeleuchtung:** einfarbig `#002952`
- **Farbkorrektur** mit getrennter Farb- und Helligkeitsstufe: satte Mischfarben, gleicher Farbton auf allen Helligkeitsstufen, gleichmäßiges Dimmen; am dunklen Ende kein Kippen in falsche Farben. Abschaltbar in `config.h` (`ISO_GAMMA_ENABLE`)
- **Farbabgleich** je Kanal (`ISO_BALANCE_R/G/B` in `config.h`), Blau ab Werk auf 217 von 255 (wirkt etwa wie 75 %)
- **Lila-Korrektur** (`ISO_PURPLE_RED_CUT` in `config.h`): senkt bei Rot-Blau-Mischungen den Rotanteil für satteres Lila
- **Lila satt halten** (`ISO_PURPLE_RICH` in `config.h`): hält im Lila-Bereich das Rot-Blau-Verhältnis (ab Werk 140), Lila läuft bei Breathing ohne blauen Nachklang aus, andere Farben bleiben gleich
- **OpenRGB-Helligkeit** über den ganzen Regler (0–255 wird auf die Maximalhelligkeit umgerechnet)
- **Hue/Saturation** funktionieren wie bei normalen QMK/VIA-Tastaturen (stufenlos, mit Umlauf)
- Farbe bleibt beim Effektwechsel erhalten
- Korrigierte Farben bei Fn + G und der Akkuwarnung
- Behobener Fehler: Tastatur stürzte im 2.4-GHz-Betrieb ab, wenn OpenRGB sie im Schlafmodus abfragte (z. B. beim Gerätescan). Im Schlaf werden VIA-/OpenRGB-Anfragen jetzt ignoriert
- Behobener Fehler: Tiefschlaf bei leerem Akku wurde schon durch einen einzelnen kurzen 0-%-Messwert ausgelöst

### Farben der Anzeigen

| Anzeige | Farbe |
|---|---|
| Bluetooth 1 / 2 / 3 (Tasten 1, 2, 3) | Blau |
| 2.4 GHz (Taste 4) | Grün |
| Kabel/USB (Taste 5) | Weiß |
| Aktive Ebene (F1–F4) | Grün |
| Akkustand (Leertaste) | Grün = voll → Gelb → Rot = leer |
| Caps Lock | Weiß 100 % |
| Win-Sperre (linke Win-Taste) | schwaches Weiß, überschreibt alle Effekte (in der aktuellen Belegung keine Taste dafür) |

---

## 2. Tastenbelegung der Fn-Ebene

Standardbelegung nach dem Flashen mit Esc oder nach einem Werksreset. Sie entspricht der
VIA-Sicherung `via_v5_angepasst.json`. In VIA lässt sich alles umbelegen.

**Grundebene (Windows), Abweichungen vom Aufdruck**

| Taste | Funktion |
|---|---|
| Pos1 (rechte Spalte, unter dem Drehknopf) | Win + Tab (Taskansicht) |
| Drehknopf drehen / drücken | Leiser/Lauter / Stumm |

**Fn-Ebene (Windows)**

| Fn + | Funktion |
|---|---|
| `1` / `2` / `3` | Bluetooth 1 / 2 / 3 (3 Sekunden halten = koppeln) |
| `4` | 2.4 GHz (3 Sekunden halten = koppeln) |
| `5` | Kabel/USB |
| `^` (3 Sekunden halten) | **Werksreset** (löscht alle Einstellungen und die VIA-Belegung) |
| Pos1 | Win + D (Desktop anzeigen) |
| `Q` / `A` | Nächster / vorheriger Beleuchtungseffekt |
| `W` / `S` | **Helligkeit + / −** |
| `E` / `D` | Farbton + / − |
| `R` / `F` | Sättigung + / − |
| `T` / `G` | Effekt-Tempo + / − |
| `H` | Akku-LEDs an/aus (grün blinkt = an, rot = aus) |
| `Enter` | Beleuchtung an/aus |
| `B` und `Leertaste` | Akkustand über die Zahlenreihe 1–0 |
| `-` | Effekt „einfarbig“ |
| `↑` / `↓` | Sättigung + / − |
| `←` / `→` | Farbton − / + |
| `I` | **Aufwach-Anzeige an/aus** (Akku + Verbindung nach dem Aufwachen; grün blinkt = an, rot = aus) |
| `P` | Druck |
| `F1`–`F8` | Arbeitsplatz, Mail, Suche, Browser-Start, Medienauswahl, Play/Pause, Zurück, Weiter |
| Drehknopf drehen / drücken | Titel zurück/weiter / Play/Pause |
| `Shift rechts` + `Esc` | **Bootloader** (Flashmodus) |

Im Mac-Modus (Ebene 3) gelten zum Teil andere Belegungen, siehe VIA.

**Ebenen-Nummern (wie in VIA):** 0 = Windows-Grundebene, 1 = Windows-Fn, 2 = Mac-Grundebene, 3 = Mac-Fn,
4 = Fn + Shift rechts. Unter Windows zeigt Fn deshalb **F1**, im Mac-Modus **F3**, Fn + Shift rechts **F4**.

> 💡 Belege auf der Fn-Ebene immer mindestens **eine Taste für USB** (Kabelmodus).
> Ohne sie kommst du aus einem Funkmodus nur per Werksreset oder Neu-Flashen zurück.

---

## 3. Vorbereitung

Einmalig bereitlegen:

- **QMK Toolbox** (Windows/macOS): <https://github.com/qmk/qmk_toolbox/releases>. Beim ersten Start alle vorgeschlagenen Treiber installieren.
- **Offizielle MonsGeek-Firmware für die M1 V5 VIA ISO** als Rettungsanker (Link im
  [MonsGeek-FAQ](https://monsgeek.com/questions/qmk-via)). **Nicht** die ANSI/US-Datei verwenden.
- **GitHub-Konto** zum Bauen der Firmware.
- Den Quellcode dieses Ordners (ZIP).
- Die VIA-Definition `M1_V5_ISO_atorata_VIA.json`.

Prüfe vorher die USB-ID deiner Tastatur: **Geräte-Manager → Tastatur → Eigenschaften → Details → Hardware-IDs**.
Dort muss `VID_342D&PID_E4C2` stehen. Bei einer anderen ID diese Firmware **nicht** flashen.

---

## 4. Firmware bauen (GitHub Actions)

GitHub kompiliert die Firmware kostenlos in der Cloud. Lokal muss nichts installiert werden.

### Erstmalig: Repository anlegen

1. Auf <https://github.com> anmelden, oben rechts **„+“ → „New repository“**.
2. Name vergeben (z. B. `m1_v5_iso_firmware`), Public oder Private, **„Create repository“**.
3. Auf der leeren Seite **„uploading an existing file“** anklicken.
4. ZIP entpacken, den **Inhalt** des Ordners `m1_v5_iso_firmware` ins Browserfenster ziehen, **„Commit changes“**.
5. Prüfen, ob der Ordner `.github/workflows/build.yml` angekommen ist. Ordner mit Punkt am Anfang
   lässt der Browser oft weg. Falls er fehlt: **„Add file“ → „Create new file“**, als Name
   `.github/workflows/build.yml` eintippen und den Inhalt aus der ZIP hineinkopieren.

### Bauen

- Jeder Upload (Commit) startet automatisch einen Build.
- Manuell: **Actions → „Build firmware“ → „Run workflow“**.
- Nach ca. 5–10 Minuten erscheint ein grüner Haken.

### Firmware herunterladen

1. Reiter **Actions** öffnen und den **obersten** Durchlauf mit grünem Haken anklicken.
2. Unten bei **Artifacts** auf **„firmware“** klicken (ZIP-Download).
3. Entpacken. Darin liegt die Firmware als `.bin`.

> Artifacts werden nach ca. 90 Tagen automatisch gelöscht. Die `.bin` daher lokal sichern.

### Dateien aktualisieren

- **Datei im Hauptordner** (z. B. `config.h`, `keyboard.json`, `m1_v5_uk.c`): auf der Repo-Startseite
  **„Add file“ → „Upload files“**, Datei hineinziehen, committen. Gleichnamige Dateien werden ersetzt.
- **Datei in einem Unterordner** (z. B. `keymaps/default/keymap.c`, `rgb_record/rgb_record.c`): erst in den
  Ordner klicken, **dort** „Add file“ → „Upload files“.
- Bei vielen Änderungen am sichersten: den kompletten Ordnerinhalt erneut hochladen.
- Werden mehrere Dateien einzeln hochgeladen, entsteht pro Upload ein Build. Nur der **oberste**
  Durchlauf enthält alle Änderungen. Ältere können ignoriert oder abgebrochen werden.

---

## 5. Firmware flashen

1. **VIA-Belegung sichern** (VIA → „Save + Load“ → Save), falls du etwas angepasst hast.
2. **QMK Toolbox** öffnen, auf **„Open“** klicken und die `.bin` auswählen.
3. **Bootloader starten**, eine der beiden Varianten:
   - **USB-Kabel abziehen, Esc gedrückt halten, Kabel einstecken.** Löscht dabei alle gespeicherten
     Einstellungen. Empfohlen, wenn neue Standardwerte gelten sollen.
   - **Fn + Shift rechts + Esc.** Einstellungen bleiben erhalten.
4. In QMK Toolbox erscheint eine gelbe Meldung zum WB32-DFU-Gerät, der Button „Flash“ wird aktiv.
5. **„Flash“** klicken und warten, bis „OK“ erscheint. Kabel währenddessen **nicht** abziehen.
6. **„Exit DFU“** klicken, Kabel kurz ab- und wieder anstecken.
7. Testen: alle Tasten (VIA → Key Tester), Beleuchtung, Verbindungen.

> Flashen funktioniert **nur per Kabel**. Neue Standardwerte (Belegung, Standardfarbe) gelten erst nach
> dem Flashen mit Esc oder nach einem Werksreset (Fn + `^`, 3 Sekunden).

---

## 6. VIA einrichten

Die Tastatur steht nicht in VIAs Gerätedatenbank. Daher einmalig die JSON-Definition laden.

**Mit usevia.app**

1. <https://usevia.app> in **Chrome oder Edge** öffnen (Firefox und Safari unterstützen kein WebHID).
2. **Settings → „Show Design tab“** einschalten, **„Use V2 definitions“** aus lassen.
3. **Design → „Load“** → `M1_V5_ISO_atorata_VIA.json` auswählen.
4. **Configure → „Authorize device“** → M1 V5 auswählen.

**Alternativ:** <https://via.evove.top>, MonsGeeks VIA-Seite. Funktioniert ohne JSON.

**Hinweise**

- Sondertasten (BT1–3, 2.4G, USB, Akku-LEDs usw.) findest du unter **„Custom“**.
- Tastenkombinationen über **Special → „Any“**, z. B.:
  - `C(S(KC_ESC))` – Task-Manager
  - `G(KC_TAB)` – Taskansicht
  - `G(KC_D)` – Desktop anzeigen
  - `G(KC_E)` – Explorer, `G(KC_L)` – Bildschirm sperren
- Die **Fn-Ebene** ist unter Windows **Ebene 1**, im Mac-Modus **Ebene 3**. Wer beide Modi nutzt, belegt beide.
- VIA funktioniert **per Kabel und über 2.4 GHz**. Nur jeweils **ein** Programm darf auf die Tastatur
  zugreifen: VIA-Tabs schließen, bevor OpenRGB läuft, und umgekehrt.
- Bei Problemen mit „gekoppelt, aber lädt nicht“: Schloss-Symbol in der Adresszeile →
  Website-Einstellungen → Berechtigungen zurücksetzen, Tastatur neu einstecken, JSON neu laden.

---

## 7. OpenRGB einrichten

Benötigt **OpenRGB 1.0 oder neuer**.

1. VIA schließen.
2. OpenRGB → **Einstellungen → Manuelle Geräte** → Gerät vom Typ **„QMK VialRGB“** hinzufügen:

   | Feld | Wert |
   |---|---|
   | Name | `M1 V5` |
   | USB VID | `342D` |
   | USB PID | `E4C2` |

3. Unten auf **„Speichern“** klicken (Menü daneben: **„Speichern und neu scannen“**).
4. OpenRGB **komplett beenden** (Infobereich → Rechtsklick → „Exit“) und neu starten.
5. Unter **Einstellungen → Unterstützte Geräte** muss „M1 V5“ mit Haken erscheinen.

**Hinweise**

- **Direct-Modus:** Die Gesamthelligkeit der Tastatur (Fn + W / S bzw. VIA) wirkt auch im Direct-Modus. Bei maximaler Helligkeit erscheinen die Farben genau wie in OpenRGB gesetzt.
- **Helligkeit:** OpenRGB hat für VialRGB-Geräte keinen eigenen Helligkeitsregler. Die Helligkeit steckt im
  Farbwert, also im Farbwähler die Helligkeit (Value) herunterziehen. Das gilt auch für Regenbogeneffekte.
- Der Helligkeitsregler nutzt den ganzen Bereich; intern wird er auf die Maximalhelligkeit (200) umgerechnet.
- Über 2.4 GHz funktioniert OpenRGB, aber **Direct-Modus besser per Kabel** (Bandbreite, Akku).
- Im Funkbetrieb zählen Änderungen aus OpenRGB (Modus, Farben) als Aktivität: Die Tastatur schläft beim Einstellen nicht ein. Effekte, die OpenRGB dauerhaft streamt (Effects-Plugin), halten sie damit allerdings dauerhaft wach. Solange sie schläft, ignoriert sie OpenRGB und VIA. Scannt OpenRGB in dieser Zeit nach Geräten, wird die Tastatur nicht gefunden: kurz eine Taste drücken und neu scannen.

---

## 8. Firmware anpassen

Die wichtigsten Stellen im Quellcode:

| Was | Datei | Stelle |
|---|---|---|
| Standardbelegung aller Ebenen | `keymaps/default/keymap.c` | `keymaps[][]` |
| Drehknopf pro Ebene | `keymaps/default/keymap.c` | `encoder_map` |
| Standardeffekt, -farbe, -helligkeit | `keyboard.json` | `rgb_matrix.default` |
| Maximale Helligkeit | `keyboard.json` | `rgb_matrix.max_brightness` |
| LEDs und Farben der Verbindungsanzeigen | `config.h` | `HS_RGB_BLINK_INDEX_*`, `HS_LBACK_COLOR_*`, `HS_PAIR_COLOR_*` |
| Ebenen-, Verbindungs-, Akku- und Aufwach-Anzeigen | `m1_v5_uk.c` | Abschnitt „Zusätzliche Anzeigen (ISO-Port)“ |
| Dauer der Aufwach-Anzeige | `m1_v5_uk.c` | `#define ISO_IND_WAKE_MS` (in ms) |
| Caps-Lock-Farbe | `m1_v5_uk.c` | `rgb_matrix_set_color(HS_RGB_INDEX_CAPS, …)` |
| USB-ID | `keyboard.json` | `usb.pid` (`0xE4C2` für ISO) |
| Gammakorrektur an/aus | `config.h` | `#define ISO_GAMMA_ENABLE` |
| Untergrenze, ab der eine LED als aus gilt | `m1_v5_uk.c` | `ISO_MIN_INPUT` (Standard 6) |
| Farbabgleich Rot/Grün/Blau | `config.h` | `ISO_BALANCE_R`, `ISO_BALANCE_G`, `ISO_BALANCE_B` (0–255) |
| Lila-Korrektur | `config.h` | `ISO_PURPLE_RED_CUT` (0 = aus … 255; höher = Lila blauer/tiefer) |
| Lila satt halten | `config.h` | `ISO_PURPLE_RICH` (0 = aus … 255 = voll; nur Lila-Bereich) |

### Farben in Hex umrechnen

`keyboard.json` erwartet Farbton (`hue`), Sättigung (`sat`) und Helligkeit (`val`), jeweils **0–255**.
Beispiel `#002952` → `hue 149`, `sat 255`, `val 82`. Mit Gammakorrektur (1.8) und Blauabgleich (217) ergibt `val 160` dieselbe Blau-Helligkeit wie ohne Korrektur `val 82`.
Umrechnung: Farbton in Grad × 255 / 360, Sättigung und Helligkeit in Prozent × 255 / 100.

### ⚠️ Fallstrick: Farbreihenfolge

Der Datentyp `RGB` speichert bei dieser Tastatur intern in der Reihenfolge **Grün, Rot, Blau**.
Schreibweisen wie `(RGB){RGB_GREEN}` oder `(RGB){0x00, 0xFF, 0x00}` vertauschen deshalb Rot und Grün.
Farben immer mit **benannten Feldern** angeben:

```c
(RGB){.r = 0x00, .g = 0xFF, .b = 0x00}   // grün, korrekt
```

`rgb_matrix_set_color(index, r, g, b)` ist davon nicht betroffen.

### LED-Nummern (Auswahl)

| Taste | LED |
|---|---|
| F1 / F2 / F3 / F4 | 22 / 21 / 20 / 19 |
| 1 / 2 / 3 / 4 / 5 | 25 / 26 / 27 / 28 / 29 |
| Caps Lock | 52 |
| Win links | 80 |
| Leertaste | 82 |

---

## 9. Notfall und Wiederherstellung

**In einem Funkmodus festgesteckt (keine Taste für USB/2.4 belegt)**

1. **Fn + `^` drei Sekunden halten** → Werksreset, die Standardbelegung ist zurück, dann **Fn + 5** für Kabel.
2. Falls das nicht geht: Kabel abziehen, **Esc halten**, Kabel einstecken, Firmware neu flashen.

**Tastatur reagiert nicht mehr / falsche Firmware**

1. Kabel abziehen, **Esc halten**, Kabel einstecken → Bootloader.
2. Offizielle MonsGeek-ISO-Firmware oder die eigene `.bin` mit QMK Toolbox flashen.

**Bootloader startet auch mit Esc nicht** (z. B. defekte Testversion)

- Laut MonsGeek: Leertasten-Switch ziehen, die **zwei Kontaktpunkte unter der Leertaste** mit einer Pinzette
  überbrücken und dabei das Kabel einstecken. Dann normal flashen.

Der Bootloader liegt fest im Chip und wird beim Flashen nicht überschrieben. Die Tastatur lässt sich daher
praktisch immer wiederherstellen.

---

## 10. Fehlerbehebung

| Problem | Ursache / Lösung |
|---|---|
| Beleuchtung komplett aus, Tastatur funktioniert | Sofort: **Fn + W** (Helligkeit +). Ab dieser Version schaltet die Firmware die LED-Stromversorgung selbst wieder ein (trat nach OpenRGB-Helligkeit 0 + Schlaf auf) |
| Win-Taste reagiert nicht auf Beleuchtung | Win-Sperre aktiv. In der aktuellen Belegung gibt es dafür keine Taste: in VIA `GU_TOGG` belegen oder mit Esc flashen (setzt sie zurück) |
| Win-Taste funktioniert nicht | Ebenfalls Win-Sperre, siehe oben |
| usevia.app erkennt die Tastatur nicht | JSON im Design-Tab laden; Chrome/Edge; via.evove.top schließen; Berechtigungen zurücksetzen |
| OpenRGB findet die Tastatur nicht | Eintrag **speichern**, OpenRGB komplett beenden und neu starten, VIA schließen |
| Neue Standardbelegung/-farbe greift nicht | Mit **Esc** flashen oder **Fn + `^`** 3 Sekunden (Werksreset) |
| Fn + 1–5 machen nichts | VIA-Belegung stammt noch von vorher → Werksreset oder in VIA auf Ebene 1/3 eintragen |
| Taste ohne Funktion | VIA → **Key Tester**: leuchtet sie auf → Belegung prüfen; bleibt sie dunkel → Switch-Beinchen/Hotswap-Fassung prüfen |
| Taste über Pfeil rechts sendet „Ende“ | So belegt (Pos1 sitzt unter dem Drehknopf). In VIA umbelegbar |
| Farben anders als am Mainboard | Normal (andere LEDs, Lichtführung, Helligkeitsgrenze). Nach Auge abgleichen |
| Licht bleibt nach dem Herunterfahren an (Kabelmodus) | Geht nach 5 Sekunden aus. Alternativ im BIOS „ErP Ready“ aktivieren (USB stromlos im ausgeschalteten Zustand) |
| Tastatur schläft im Funkbetrieb | 2.4 GHz nach 2 Minuten, Bluetooth nach 1 Minute ohne Tastendruck (ohne Verbindung nach 30 s). Per Kabel nie. Einstellbar in `wls/wls.h` (`HS_SLEEP_TIMEOUT`, `HS_SLEEP_TIMEOUT_2G4`) |
| Tastatur verliert die Verbindung und wacht nicht auf | Akku leer (Tiefschlaf bei 0 %) → Kabel anstecken und laden. Akkustand prüfen: Fn halten, Leertaste zeigt rot = leer |

### Energiesparen

| Situation | Schlafmodus nach | Aufwachen |
|---|---|---|
| Kabelmodus, Kabel steckt, PC an | nie | – |
| Kabelmodus, Kabel steckt, PC aus/Standby | nach 5 Sekunden alles dunkel | sobald der PC läuft |
| Kabelmodus, **kein Kabel** | Taste 5 blinkt, nach 30 Sekunden ohne Tastendruck Schlaf | Taste oder Kabel einstecken |
| 2.4 GHz, verbunden | 2 Minuten ohne Tastendruck | beliebige Taste |
| Bluetooth, verbunden | 1 Minute ohne Tastendruck | beliebige Taste |
| Funk, keine Verbindung | 30 Sekunden | beliebige Taste |
| Akku 0 % | 1 Minute | nur per Kabel |

---

## 11. Lizenz

GPL-2.0-or-later, wie QMK, MonsGeeks Code und die Firmware von atorata (siehe `LICENSE`).
Bei Weitergabe der `.bin` auch den Quellcode mitgeben oder verlinken. `LICENSE` und die
Copyright-Hinweise in den Dateien nicht entfernen.
