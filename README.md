# PF-2 Fuzz — emulazione fisica del Shin-Ei Companion Fuzz FY-2 (VST3 / AU / Standalone)

Plugin in C++17 con JUCE 8. Riferimento: schema GGG "SECF" (replica del FY-2, con
tensioni misurate ai transistor), privato delle aggiunte moderne (booster, LED,
resistenza anti-pop). La topologia coincide con il kit FuzzDog FY2 V3 senza SCOOP né boost.

## Comandi

Come sul pedale originale: **FUZZ**, **VOLUME** (entrambi 50k lineari) e **footswitch**.

| Comando | CC predefinito |
|---|---|
| FUZZ | 20 |
| VOLUME | 21 |
| Footswitch (bypass) | 64 (pedale sustain) |
| INPUT / OUTPUT (trim di servizio, barra superiore) | — |

Il FY-2 originale ha un'uscita molto bassa (circa 30 mV rms). Il plugin la porta a un livello
utilizzabile con +18 dB di guadagno pulito dopo il circuito: OUTPUT a 0 dB equivale ad
alzare l'amplificatore, il suono del pedale non cambia.

Il pedale è mono: il plugin prende la chitarra da qualunque ingresso arrivi (sinistro o destro) e copia l'uscita su tutti i canali.

**MIDI Learn**: clic destro su un comando → *MIDI Learn* → muovi un controllo della KeyLab.
La mappatura è salvata nel progetto. Il footswitch commuta alla pressione: imposta il
pulsante in modalità *Gate/Momentary* in Arturia MIDI Control Center.

## Pick-up (barra superiore)

Il FY-2 ha un ingresso a bassissima impedenza: il pick-up non è "isolato" come davanti a un
ampli, la sua resistenza e la sua induttanza fanno parte del circuito e tagliano gli acuti
prima del fuzz. Scegli il tipo della chitarra che stai suonando:

| Opzione | R | L | Effetto rispetto a "Ideale" (a 4 kHz, segnale debole) |
|---|---|---|---|
| Ideale | 10 kΩ | — | comportamento senza pick-up (versione precedente) |
| Single coil | 6 kΩ | 2.5 H | circa −15 dB |
| P-90 | 8 kΩ | 6 H | circa −23 dB |
| Humbucker | 10 kΩ | 5 H | circa −20 dB |
| Personalizzato | 2–20 kΩ | 0.5–10 H | valori del tuo pick-up |

**Le mie chitarre**: con il pulsante "…" accanto al menu salvi i valori attuali con il nome
della chitarra ("Tele", "SG"...) e li richiami poi dal menu, nella sezione *Le mie chitarre*.
Salvare con un nome già esistente aggiorna quella chitarra; dallo stesso pulsante si elimina.
L'elenco è condiviso da tutti i progetti: si trova in `~/Library/Application Support/PF-2 Fuzz/`
(macOS) o in `%APPDATA%\PF-2 Fuzz\` (Windows). Ogni progetto ricorda quale chitarra era scelta.
Se modifichi R o L di una chitarra salvata, il menu mostra "Personalizzato" finché non la salvi di nuovo.

Il modello include anche la capacità del cavo (470 pF). La resistenza in continua dei pick-up
è quasi sempre indicata dal costruttore; l'induttanza meno spesso: se non la conosci, parti
dai valori tipici. La chitarra va collegata all'ingresso Hi-Z (strumento) della scheda audio.

## Scaricare la versione per Mac (Apple Silicon e Intel)

Ogni modifica su `main` viene compilata automaticamente da GitHub Actions su macOS
(universal binary: M1–M4 e Intel).

1. Nel repository apri **Actions → Build macOS**, scegli l'ultima esecuzione riuscita e scarica
   l'artefatto **PF2Fuzz-macOS** (serve essere collegati a GitHub). Per una versione "ufficiale"
   crea un tag `v1.0.0`: la build compare anche in **Releases**, scaricabile senza login.
2. Estrai lo zip (contiene a sua volta `PF2Fuzz-macOS.zip`, estrai anche quello).
3. Il plugin non è firmato con un certificato Apple Developer, quindi macOS lo blocca al primo
   avvio. Apri il Terminale e trascina dentro la cartella `PF-2 Fuzz` dopo questo comando:
   ```
   xattr -dr com.apple.quarantine 
   ```
   poi fai doppio clic su **Installa PF-2 Fuzz.command**. Copia VST3 e AU nelle cartelle dei
   plugin dell'utente e l'app Standalone in Applicazioni.
4. Riavvia la DAW (in Logic: se non compare, *Plug-in Manager → Reimposta e riscansiona*).

**Standalone**: al primo avvio macOS chiede il permesso di usare il microfono (cioè gli ingressi
della scheda audio): concedilo. JUCE avvia la Standalone con l'ingresso silenziato per evitare
fischi di feedback: compare la scritta *"Audio input is muted to avoid feedback loop"*; premi il
pulsante accanto oppure togli la spunta *Mute audio input* in **Options → Audio/MIDI Settings**,
dove scegli anche la scheda audio come ingresso e uscita.

## Compilazione

Serve CMake ≥ 3.22. JUCE 8.0.4 viene scaricato automaticamente
(oppure `-DJUCE_DIR=/percorso/JUCE`).

**macOS (VST3 + AU, universal binary)** — con Xcode installato:
```
cmake -B build -G Xcode
cmake --build build --config Release
```
I plugin vengono copiati in `~/Library/Audio/Plug-Ins/VST3` e `.../Components`.
Per l'AU, se Logic non lo vede: `killall -9 AudioComponentRegistrar` e riapri.

**Windows (VST3)** — con Visual Studio 2022:
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```
(la copia in `C:\Program Files\Common Files\VST3` richiede il terminale come amministratore).

Test del modello: `build/circuit_test` (punto di lavoro, distorsione al variare di FUZZ, CPU).

## Usare la KeyLab

- **Ableton Live**: metti il plugin sulla traccia audio della chitarra; crea una traccia
  MIDI con ingresso KeyLab e in *MIDI To* scegli la traccia della chitarra → "PF-2 Fuzz".
  In alternativa, usa la mappatura MIDI di Live (Cmd/Ctrl+M) sui parametri del plugin.
- **Logic Pro**: l'AU è un normale effetto (Audio FX), da inserire sulla traccia audio della
  chitarra. Non riceve CC direttamente: per la KeyLab usa l'assegnazione controller di Logic
  (Cmd+K, muovi il comando nel plugin e poi la manopola sulla tastiera). Il MIDI Learn interno
  e i CC predefiniti valgono per VST3 e Standalone.
- In VST3 i CC arrivano tramite i parametri "MIDI CC" che JUCE espone: è il meccanismo
  standard del formato, per questo nell'elenco automazioni ne vedrai molti.

## Il modello

Il circuito viene risolto per intero a ogni campione, senza curve di distorsione inventate:

- **Analisi nodale** a 12 nodi con tutti i componenti (C1–C8, R1–R6, R9, FUZZ, VOLUME,
  carico dell'ampli 1 MΩ, alimentazione 9 V); condensatori con la regola dei trapezi.
- **Q1 e Q2 con il modello di Ebers-Moll** (β diretto 38, inverso 2, Is 1e-14, capacità Cμ),
  risolti con Newton-Raphson sulle 4 tensioni di giunzione. Il punto di lavoro calcolato
  (Q1 6.64 V, Q2 2.03 V) coincide con le misure GGG (6.6 V e 2.0 V).
- Polarizzazione, clipping asimmetrico, spostamento del bias per la carica di C3, somma in
  controfase dei due stadi sul FUZZ e carico della rete d'uscita sui collettori emergono
  dalla simulazione stessa.
- Il circuito gira sempre a 176–192 kHz: oversampling 4× a 44.1/48 kHz, 2× a 88.2/96 kHz
  (latenza 6 o 4 campioni). Circa il 12% di un core.

**Limiti noti**: i valori dei pick-up sono tipici, non misurati sulle tue chitarre; il segnale
registrato dalla scheda contiene già la risonanza del pick-up a vuoto, quindi la ricostruzione
del carico è un'approssimazione; i transistor originali C536F7G non
sono documentati, i parametri sono stati scelti per riprodurre le tensioni misurate; il
verso del FUZZ (orario = più armoniche) è dedotto dal modello. Tutti i valori sono in
`struct Components` e `struct BJT` dentro `Source/FuzzCircuit.h`.
