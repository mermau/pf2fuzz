# Collateral Effects FYN-2 — emulazione fisica del Shin-ei Companion FUZZ BOX FY-2 (VST3 / AU / Standalone)

Plugin in C++17 con JUCE 8. Riferimento: lo schema dell'originale Shin-ei FY-2 (transistor
NEC 2SC536 classe F, pot Cosmos B50K), che nei valori coincide con la replica GGG "SECF" priva
delle aggiunte moderne e con il kit FuzzDog FY2 V3 senza SCOOP né boost.

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

## Interno (pulsante INTERNO nella barra superiore)

Come aprire il fondo del pedale. Tutti i comandi sono automatizzabili e assegnabili via MIDI
(clic destro → MIDI Learn); "VALORI ORIGINALI" riporta batteria a 9 V e transistor a hFE 226.

- **BATT / DC**: l'originale funzionava solo a batteria; DC simula un alimentatore stabilizzato
  da 9 V (comodità, non fedeltà).
- **CARICA**: tensione della batteria da 9,6 V (nuova) a 5,4 V (esaurita, 0,9 V per elemento),
  con la curva di scarica tipica di una 9 V alcalina; il pannello mostra tensione e carica
  residua. La zona rossa, fino a 4,5 V, va oltre la scarica, per sonorità "strane".
  Cosa cambia: il FY-2 si autopolarizza, quindi non "sputacchia" come un Fuzz Face: scendendo
  di tensione il volume cala (−7 dB a 5,4 V, −9 dB a 4,5 V), il suono si comprime e le armoniche
  pari crescono molto (da +4 a +13 dB sulle dispari suonando forte): fuzz più caldo,
  asimmetrico, quasi ottavato. Sotto i 4,5 V torna simmetrico e cala soltanto: per questo il
  comando si ferma lì.
- **hFE Q1 / hFE Q2**: guadagno dei due 2SC536, da 60 a 560 (classi D–G); l'originale è in
  classe F (160–320). Q1 decide quanto satura il primo stadio, Q2 quanto è "affamato" lo stadio
  d'uscita: riproduce le differenze fra un esemplare e l'altro.

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
L'elenco è condiviso da tutti i progetti: si trova in `~/Library/Application Support/Collateral Effects/FYN-2/`
(macOS) o in `%APPDATA%\Collateral Effects\FYN-2\` (Windows). Ogni progetto ricorda quale chitarra era scelta. Le chitarre salvate con la versione "PF-2 Fuzz" vengono recuperate in automatico.
Se modifichi R o L di una chitarra salvata, il menu mostra "Personalizzato" finché non la salvi di nuovo.

Il modello include anche la capacità propria del pick-up (120–180 pF), il cavo (470 pF) e il
carico dei potenziometri della chitarra (125 kΩ con pot da 250k per i single coil, 250 kΩ con
pot da 500k per P-90 e humbucker). La resistenza in continua dei pick-up è quasi sempre indicata
dal costruttore; l'induttanza meno spesso: se non la conosci, parti dai valori tipici.

La chitarra va collegata all'ingresso Hi-Z (strumento) della scheda audio. La scheda registra il
pick-up quasi a vuoto, con il suo picco di risonanza (2,5–4 kHz) e gli acuti attenuati sopra di
esso; il FY-2, che carica il pick-up con pochi kΩ, quel picco non lo sente. Con un pick-up
selezionato il plugin ricostruisce quindi il segnale del pick-up prima del circuito (correzione
limitata sopra i 10 kHz per non esaltare il fruscio della scheda). Con "Ideale" la correzione è
disattivata.

## Scaricare la versione per Mac (Apple Silicon e Intel)

Ogni modifica su `main` viene compilata automaticamente da GitHub Actions su macOS
(universal binary: M1–M4 e Intel).

1. Nel repository apri **Actions → Build macOS**, scegli l'ultima esecuzione riuscita e scarica
   l'artefatto **FYN2-macOS** (serve essere collegati a GitHub). Per una versione "ufficiale"
   crea un tag `v1.0.0`: la build compare anche in **Releases**, scaricabile senza login.
2. Estrai lo zip (contiene a sua volta `FYN2-macOS.zip`, estrai anche quello).
3. Il plugin non è firmato con un certificato Apple Developer, quindi macOS lo blocca al primo
   avvio. Apri il Terminale e trascina dentro la cartella `FYN-2` dopo questo comando:
   ```
   xattr -dr com.apple.quarantine 
   ```
   poi fai doppio clic su **Installa FYN-2.command** (chiede la password di amministratore).
   Copia VST3 e AU in `/Library/Audio/Plug-Ins/` e l'app Standalone in Applicazioni, e
   rimuove la vecchia versione "PF-2 Fuzz".
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

Test del modello: `build/circuit_test` (punto di lavoro, distorsione al variare di FUZZ, CPU) e
`build/click_test` (assenza di click e salti nel risolutore), eseguiti anche su GitHub a ogni build.

## Usare la KeyLab

- **Ableton Live**: metti il plugin sulla traccia audio della chitarra; crea una traccia
  MIDI con ingresso KeyLab e in *MIDI To* scegli la traccia della chitarra → "FYN-2".
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
- **Q1 e Q2 con il modello di Ebers-Moll**, parametri del NEC 2SC536 classe F montato
  sull'originale (marcatura "C536 F7G"): hFE 226, centro della classe 160–320 del datasheet;
  Cob 3.5 pF. Risolti con Newton-Raphson sulle 4 tensioni di giunzione. Punto di lavoro
  calcolato: Q1 3.2 V, Q2 0.9 V. Le tensioni misurate da GGG (6.6 V e 2.0 V) si riferiscono
  alla loro replica con transistor a basso guadagno: con gli originali il FY-2 è più
  "affamato" e distorce di più, come osserva la stessa GGG ("più hFE, fuzz più cattivo").
- **Pot Cosmos B50K**: nella siglatura giapponese "B" indica la curva lineare, già usata
  per FUZZ e VOL.
- Polarizzazione, clipping asimmetrico, spostamento del bias per la carica di C3, somma in
  controfase dei due stadi sul FUZZ e carico della rete d'uscita sui collettori emergono
  dalla simulazione stessa.
- Il circuito gira sempre a 350–390 kHz: oversampling 8× a 44.1/48 kHz, 4× a 88.2/96 kHz,
  2× a 176.4/192 kHz (latenza 6 campioni). L'aliasing delle armoniche del fuzz resta sotto i −55 dB.
- Calcolo in forma di variabili di stato con matrici precalcolate, Newton-Raphson con
  predittore e limitazione delle giunzioni (pnjlim, come SPICE): circa il 9% di un core a 8×.
- In bypass, finita la dissolvenza, il circuito non viene calcolato (come il pedale true
  bypass, il cui ingresso è scollegato) e non consuma CPU.

**Limiti noti**: i valori dei pick-up sono tipici, non misurati sulle tue chitarre; la
ricostruzione del segnale del pick-up presuppone un ingresso Hi-Z da 1 MΩ e il volume della
chitarra al massimo; l'hFE reale varia da esemplare a esemplare
(160–320 nella classe F), e il modello ne usa il valore centrale; il
verso del FUZZ (orario = più armoniche) è dedotto dal modello. Tutti i valori sono in
`struct Components` e `struct BJT` dentro `Source/FuzzCircuit.h`.

## Grafica e caratteri

Interfaccia ispirata al FY-2 originale (vernice nera martellata, pannello in acciaio, targhetta
rossa), con LED di stato aggiunto. Caratteri inclusi: Archivo Black (SIL Open Font License) e
Yellowtail (Apache 2.0), licenze in `resources/fonts/`.
