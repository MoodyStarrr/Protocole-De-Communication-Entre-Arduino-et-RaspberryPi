# Protocole de communication Arduino Uno ↔ Raspberry Pi 5

Protocole de trame binaire en C99 pour relier un microcontrôleur (ATmega328P) à un Raspberry Pi 5 par liaison série. Le cœur du projet est un code commun (CRC-8 et décodeur de trames) compilé à l'identique pour les deux cibles.

---

## Compétences démontrées

| Domaine | Ce que le projet met en œuvre |
|---|---|
| Conception de protocole | Délimiteurs de trame, champ de longueur, intégrité par CRC-8, échappement d'octets |
| Machine d'états | Décodeur octet par octet, résistant aux trames tronquées ou corrompues |
| Intégrité des données | CRC-8 écrit à la main (polynôme `0x31`, version réfléchie `0x8C`) |
| Tests | Tests unitaires du CRC et du décodeur, 12 cas limites pour le décodeur |
| Multi-cibles | Même code pour x86/ARM Linux (`gcc`) et AVR 8 bits (`avr-gcc`) |
| Build | Makefile qui synchronise le code commun vers chaque cible |
| Outillage | Ports série virtuels avec `socat`, script Bash de préparation |

---

## Format de trame

```
 0x7E | COMMAND | LENGTH | PAYLOAD (8 octets) | CRC-8 | 0xFF
 START   1 o       1 o        8 octets fixes     1 o     END
```

| Champ | Taille | Rôle |
|---|---|---|
| `START` | 1 octet | `0x7E`, début de trame |
| `COMMAND` | 1 octet | `CMD_SENSOR_READ` (0), `CMD_SET_LED` (1), `CMD_OUTPUT_WRITE` (2) |
| `LENGTH` | 1 octet | Nombre d'octets utiles dans le payload, de 1 à 8 |
| `PAYLOAD` | 8 octets | Données, complétées (padding) jusqu'à 8 octets |
| `CRC` | 1 octet | CRC-8 calculé sur `COMMAND + LENGTH + PAYLOAD` (10 octets) |
| `END` | 1 octet | `0xFF`, fin de trame |

Taille d'une trame sans échappement : 13 octets.

### Échappement (byte stuffing)

L'octet `0x7D` placé devant un octet indique que celui-ci est une donnée et non un caractère de contrôle. Cela permet de transmettre une valeur `0x7E` dans le payload sans que le récepteur la prenne pour un début de trame.

Un `0x7E` non échappé en cours de trame force la resynchronisation : l'ancienne trame est abandonnée et une nouvelle commence.

### CRC-8

Algorithme bit à bit, initialisation à `0x00`, polynôme `0x31` traité en version réfléchie (`0x8C`) : c'est le CRC-8/MAXIM. Valeur de contrôle standard : `CRC("123456789") = 0xA1`, vérifiée dans les tests.

---

## Décodeur : machine d'états

`frame_parser()` reçoit un octet à la fois et renvoie `FRAME_IN_PROGRESS`, `FRAME_VALID` ou `FRAME_INVALID`.

```
 WAIT_START ──0x7E──▶ READ_COMMAND ──▶ READ_LENGTH ──▶ READ_PAYLOAD ──▶ READ_CRC ──▶ READ_END ──0xFF──▶ VALID
                                           │                                │              │
                                    longueur = 0 ou > 8               CRC faux        octet ≠ 0xFF
                                           └────────────────────────────────┴──────────────┴──────▶ INVALID
```

Utilisation :

```c
framer_context_t ctx;
framer_reset(&ctx);

while (/* octet reçu sur la liaison série */) {
    switch (frame_parser(octet, &ctx)) {
        case FRAME_VALID:       /* ctx.current_message est exploitable */ break;
        case FRAME_INVALID:     /* trame rejetée, le décodeur repart de zéro */ break;
        case FRAME_IN_PROGRESS: break;
    }
}
```

---

## Arborescence

```
.
├── common/                  # code source de référence, partagé entre les deux cibles
│   ├── protocol.h           # constantes, type frame_t, commandes
│   ├── crc.c / crc.h        # CRC-8
│   └── framer.c / framer.h  # décodeur de trames
├── raspberry/
│   ├── src/main.c           # démonstration : construit et affiche une trame
│   └── addon/queue.h        # interface d'une file de trames (réservée au côté Raspberry)
├── arduino/                 # cible AVR (sources communes copiées par make)
├── misc/
│   ├── test_crc.c           # tests du CRC
│   ├── test_framer.c        # tests du décodeur
│   └── test_env.sh          # crée deux ports série virtuels reliés (socat)
└── makefile
```

> Le dossier `common/` est la seule source à modifier. Les copies dans `raspberry/` et `arduino/` sont générées par `make` (`sync_raspberry`, `sync_arduino`).

## Compilation et tests

Prérequis : `gcc`, `make`. Pour la cible Arduino : `avr-gcc`, `avrdude`. Pour les ports virtuels : `socat`.

```bash
make                  # compile la démonstration Raspberry -> ./Raspberry_side
make test_crc         # tests unitaires du CRC-8
make test_framer      # tests du décodeur (12 trames)
make flash            # compile et téléverse la cible Arduino
make clean
```

Ports série virtuels pour tester sans matériel :

```bash
./misc/test_env.sh    # lance socat et écrit les deux chemins dans socat_paths.txt
```

### Cas couverts par `test_framer`

Trame correcte, `0xFF` dans le payload, `0x7E` non échappé suivi d'une nouvelle trame, octets de padding, CRC erroné, deux cas d'échappement, octet parasite avant le start, longueurs 0 / 8 / 9, trame sans octet de fin.

---

## Choix de conception

- Payload de taille fixe (8 octets) : le décodeur lit toujours 8 octets, le champ `LENGTH` indique combien sont utiles. Simple et prévisible, au prix d'un peu de bande passante.
- CRC sur les 10 octets (commande, longueur, payload complet) pour protéger aussi le padding.
- Code commun unique : un bug corrigé une fois l'est pour les deux cibles.

## Avancement et pistes

- [x] CRC-8 et tests unitaires
- [x] Décodeur de trames à machine d'états et tests (12 cas)
- [x] Makefile multi-cibles avec synchronisation du code commun
- [x] Démonstration côté Raspberry
- [ ] Implémentation de la file de trames (`queue.h`)
- [ ] Gestion du timeout : abandonner une trame incomplète si la suite n'arrive pas
- [ ] Émission de trames avec échappement côté encodeur
- [ ] Liaison réelle Arduino ↔ Raspberry (UART), avec `termios` côté Linux
