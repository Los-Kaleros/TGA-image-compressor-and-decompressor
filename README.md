# TGA kompresor (JBK)

Konzolový program v jazyku C, ktorý komprimuje a dekomprimuje 24-bitové nekomprimované TGA obrázky metódou **JBK** (Jednoduchá Bloková Kompresia). Školské zadanie z predmetu UPR (VŠB-TUO, FEI), zadanie je v [`assignment.txt`](assignment.txt).

## Ako to funguje

1. Obrázok sa rozdelí na štvorcové bloky zadanej veľkosti.
2. Každý blok sa prejde po riadkoch a pixely sa zlučujú do sekvencií (RLE): sekvencia = prvý pixel + počet pixelov, ktoré sú mu „podobné“.
3. Pixely sú podobné, ak súčet absolútnych rozdielov farebných zložiek (R, G, B) nepresiahne `--max-diff`.
4. Sekvencie nikdy nepresahujú hranicu bloku a majú najviac 255 pixelov. Každá sekvencia zaberá 4 bajty (3 bajty farba + 1 bajt dĺžka).
5. Výstupný `.jbk` súbor má hlavičku (šírka, výška, veľkosť bloku, `image_descriptor`) a za ňou pole sekvencií.

Kompresia je stratová pre `--max-diff > 0` (pixely v sekvencii sa nahradia prvým pixelom) a bezstratová pre `--max-diff 0`.

## Použitie

```bash
gcc -O2 -o jbk main.c -lm

./jbk compress   --input carmack.tga --output out.jbk --block-size 8 --max-diff 10
./jbk decompress --input out.jbk     --output decompressed.tga
```

Argumenty môžu byť v ľubovoľnom poradí. Chýbajúci alebo opakovaný argument ukončí program s chybovou hláškou. Rozmery obrázka musia byť násobkom veľkosti bloku. Podporovaný je len 24-bitový RGB TGA bez farebnej mapy a bez kompresie.

## Výsledky (carmack.tga, 786 450 B)

Veľkosť výstupu `.jbk` v bajtoch, nameraná pri kompilácii s `gcc -O2`:

| `--block-size` | `--max-diff 0` | `--max-diff 10` | `--max-diff 40` |
|---|---|---|---|
| 4 | 867 903 | 448 851 | 210 263 |
| 8 | 853 403 | 423 375 | 180 239 |
| 16 | 851 179 | 410 863 | 171 399 |

Pri `--max-diff 0` je výstup **väčší** ako originál (asi 108 %). Obrázok má málo opakujúcich sa pixelov a každá sekvencia stojí 4 bajty namiesto 3. Metóda sa oplatí pre obrázky s veľkými rovnakými plochami alebo pri stratovej kompresii.

## Čo som sa naučil / obmedzenia

- Binárny formát a práca s hlavičkami (`#pragma pack`, `fread`/`fwrite`).
- Dynamické pole sekvencií s `realloc`.
- Spracovanie argumentov príkazového riadka a chybových stavov.
- Obmedzenie: žiadne automatické testy, formát `.jbk` nemá kontrolné číslo (magic), pri poškodenom súbore sa chyba odhalí len čiastočne.
