# Décodage SeaTalk — Total Mileage

## Trame `0x22` — Total Mileage

Format :

    22 02 XX YY 00

Le compteur total est codé sur 16 bits, little-endian.

Formule :

    TOTAL_RAW = XX + (YY << 8)

    TOTAL_NM = TOTAL_RAW / 10

### Exemple

Trame :

    22 02 37 CC 03

Les octets du compteur sont `37 CC` :

    TOTAL_RAW = 0x37 + (0xCC << 8)
              = 0xCC37
              = 52279

Donc :

    TOTAL_NM = 52279 / 10
             = 5227,9 NM

> Remarque : dans une trame `0x22` standard, le dernier octet est normalement `00`.
> S'il vaut une autre valeur, il faut vérifier le contexte de la capture.


---

## Trame `0x25` — Total & Trip Log

Format :

    25 Z4 XX YY UU VV AW

Pour le **Total Log**, les trois éléments sont :

    Z  = nibble haut de l'octet `Z4`
    XX = octet suivant
    YY = octet suivant

Le compteur total est codé sur **20 bits** :

    ZZZZ YYYY YYYY XXXX XXXX XXXX XXXX

La règle correcte est :

    TOTAL_RAW = XX + (YY << 8) + (Z << 16)

ou, de façon équivalente :

    TOTAL_RAW = XX | (YY << 8) | (Z << 16)

Puis :

    TOTAL_NM = TOTAL_RAW / 10


### Exemple avec la trame du Tridata

    25 34 37 CC 39 04 00

Découpage :

    Z  = 3
    XX = 37
    YY = CC

Calcul :

    TOTAL_RAW = 0x37 + (0xCC << 8) + (0x03 << 16)
              = 55 + 52224 + 196608
              = 248887

Donc :

    TOTAL_NM = 248887 / 10
             = 24888,7 NM

Le Tridata affichant environ `24888 NM` est donc parfaitement cohérent.


---

## Résumé

| Datagramme | Octets Total | Formule | Résolution |
|---|---|---|---|
| `0x22` | `XX YY` | `(XX + YY×256) / 10` | 0,1 NM |
| `0x25` | `Z XX YY` | `(XX + YY×256 + Z×65536) / 10` | 0,1 NM |

### À retenir

Pour `0x22` :

    TOTAL = (XX | (YY << 8)) / 10.0

Pour `0x25` :

    Z = (byte1 >> 4)
    TOTAL = (XX | (YY << 8) | (Z << 16)) / 10.0

La documentation originale de Thomas Knauf indique `Z * 4096` pour `0x25`, mais cette valeur est incohérente avec le compteur 20 bits et avec les valeurs réellement émises par les instruments. Pour un compteur 20 bits, `Z` doit être placé dans les bits 16 à 19, donc `Z * 65536`.