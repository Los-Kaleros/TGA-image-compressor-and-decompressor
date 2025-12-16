#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// --- DATOVE STRUKTURY ---

// Struktura pre 24-bitovy pixel
typedef struct {
    uint8_t b, g, r;
} Pixel;

// Struktura pre hlavicku TGA suboru
#pragma pack(push, 1)
typedef struct {
    uint8_t id_length;
    uint8_t colormap_type;
    uint8_t image_type;
    uint16_t colormap_origin;
    uint16_t colormap_length;
    uint8_t colormap_depth;
    uint16_t x_origin;
    uint16_t y_origin;
    uint16_t width;
    uint16_t height;
    uint8_t pixel_depth;
    uint8_t image_descriptor;
} TgaHeader;
#pragma pack(pop)

// Struktura pre komprimacnu sekvenciu
typedef struct {
    Pixel pixel;
    uint8_t length;
} Sequence;

// Struktura pre argumenty prikazoveho riadku
typedef enum { UNSET, COMPRESS, DECOMPRESS } Mode;
typedef struct {
    Mode mode;
    char *input_path;
    char *output_path;
    int block_size;
    int max_diff;
} CliArgs;

// --- FUNKCIE PRE PRACU S TGA ---

int read_tga(const char *filename, TgaHeader *header, Pixel **pixels) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Chyba: Nepodarilo sa otvorit vstupny subor %s\n", filename);
        return -1;
    }

    if (fread(header, sizeof(TgaHeader), 1, file) != 1) {
        fprintf(stderr, "Chyba: Nepodarilo sa precitat hlavicku TGA suboru.\n");
        fclose(file);
        return -1;
    }

    if (header->image_type != 2 || header->pixel_depth != 24) {
        fprintf(stderr, "Chyba: Program podporuje iba 24-bitove nekomprimovane RGB TGA obrazky (image_type 2, pixel_depth 24).\n");
        fclose(file);
        return -1;
    }

    // Skip Image ID field if present
    if (header->id_length > 0) {
        fseek(file, header->id_length, SEEK_CUR);
    }
    
    // Skip color map if present, though image_type 2 typically means no color map
    if (header->colormap_type == 1) {
        fseek(file, header->colormap_length * (header->colormap_depth / 8), SEEK_CUR);
    }

    *pixels = (Pixel *)malloc(header->width * header->height * sizeof(Pixel));
    if (!*pixels) {
        fprintf(stderr, "Chyba: Nedostatok pamate pre pixely.\n");
        fclose(file);
        return -1;
    }

    if (fread(*pixels, sizeof(Pixel), header->width * header->height, file) != (size_t)(header->width * header->height)) {
        fprintf(stderr, "Chyba: Nepodarilo sa nacitat vsetky pixely.\n");
        free(*pixels);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

int write_tga(const char *filename, const TgaHeader *original_header, const Pixel *pixels) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Chyba: Nepodarilo sa vytvorit vystupny subor %s\n", filename);
        return -1;
    }
    
    TgaHeader out_header = *original_header; // Kopirujeme hlavicku z originalu
    out_header.id_length = 0; // Pre jednoduchost vynulujeme id_length a colormap
    out_header.colormap_type = 0;
    out_header.image_type = 2; // Zabezpecime, ze je uncompressed RGB
    out_header.pixel_depth = 24; // Zabezpecime, ze je 24-bit
    // out_header.image_descriptor je uz kopirovany z original_header, co je spravne

    if (fwrite(&out_header, sizeof(TgaHeader), 1, file) != 1) {
        fprintf(stderr, "Chyba: Nepodarilo sa zapisat hlavicku do TGA suboru.\n");
        fclose(file);
        return -1;
    }

    if (fwrite(pixels, sizeof(Pixel), out_header.width * out_header.height, file) != (size_t)(out_header.width * out_header.height)) {
        fprintf(stderr, "Chyba: Nepodarilo sa zapisat vsetky pixely.\n");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}


// --- JBK KOMPRESIA / DEKOMPRESIA ---

int pixel_diff(const Pixel *p1, const Pixel *p2) {
    return abs(p1->r - p2->r) + abs(p1->g - p2->g) + abs(p1->b - p2->b);
}

// --- PARSOVANIE ARGUMENTOV ---

void print_usage(const char* prog_name) {
    fprintf(stderr, "Pouzitie:\n");
    fprintf(stderr, "  %s compress --input <in.tga> --output <out.jbk> --block-size <size> --max-diff <diff>\n", prog_name);
    fprintf(stderr, "  %s decompress --input <in.jbk> --output <out.tga>\n", prog_name);
}

int parse_args(int argc, char *argv[], CliArgs *args) {
    args->mode = UNSET;
    args->input_path = NULL;
    args->output_path = NULL;
    args->block_size = -1;
    args->max_diff = -1;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "compress") == 0) {
        args->mode = COMPRESS;
    } else if (strcmp(argv[1], "decompress") == 0) {
        args->mode = DECOMPRESS;
    } else {
        fprintf(stderr, "Neznamy prikaz: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    for (int i = 2; i < argc; i += 2) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Chyba: Chybajuci parameter pre '%s'.\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
        char *key = argv[i];
        char *value = argv[i+1];

        if (strcmp(key, "--input") == 0) {
            if (args->input_path) { fprintf(stderr, "Chyba: Duplicitny argument: --input\n"); return 1; }
            args->input_path = value;
        } else if (strcmp(key, "--output") == 0) {
            if (args->output_path) { fprintf(stderr, "Chyba: Duplicitny argument: --output\n"); return 1; }
            args->output_path = value;
        } else if (strcmp(key, "--block-size") == 0) {
            if (args->block_size != -1) { fprintf(stderr, "Chyba: Duplicitny argument: --block-size\n"); return 1; }
            args->block_size = atoi(value);
             if (args->block_size <= 0) { fprintf(stderr, "Chyba: Neplatna hodnota pre --block-size (musi byt kladne cislo).\n"); return 1; }
        } else if (strcmp(key, "--max-diff") == 0) {
            if (args->max_diff != -1) { fprintf(stderr, "Chyba: Duplicitny argument: --max-diff\n"); return 1; }
            args->max_diff = atoi(value);
            if (args->max_diff < 0) { fprintf(stderr, "Chyba: Neplatna hodnota pre --max-diff (musi byt nezaporne cislo).\n"); return 1; }
        } else {
            fprintf(stderr, "Chyba: Neznamy argument: %s\n", key);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validacia
    if (!args->input_path || !args->output_path) {
        fprintf(stderr, "Chyba: Chybaju povinne argumenty --input alebo --output.\n");
        print_usage(argv[0]);
        return 1;
    }
    if (args->mode == COMPRESS && (args->block_size == -1 || args->max_diff == -1)) {
        fprintf(stderr, "Chyba: Pre kompresiu su povinne argumenty --block-size a --max-diff.\n");
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}


// --- MAIN LOGIKA ---

int main(int argc, char *argv[]) {
    CliArgs args;
    if (parse_args(argc, argv, &args) != 0) {
        return 1;
    }

    if (args.mode == COMPRESS) {
        printf("Spustam kompresiu...\n");

        TgaHeader header;
        Pixel *pixels = NULL;
        if (read_tga(args.input_path, &header, &pixels) != 0) {
            return 1;
        }

        if (header.width % args.block_size != 0 || header.height % args.block_size != 0) {
            fprintf(stderr, "Chyba: Rozmery obrazku (%dx%d) musia byt nasobkom velkosti bloku (%d).\n", header.width, header.height, args.block_size);
            free(pixels);
            return 1;
        }

        long img_width = header.width;
        long img_height = header.height;
        int block_s = args.block_size;

        // Predpokladana maximalna velkost sekvencii (jeden pixel = jedna sekvencia)
        size_t sequences_capacity = img_width * img_height; 
        Sequence *sequences = (Sequence *)malloc(sequences_capacity * sizeof(Sequence));
        if (!sequences) {
            fprintf(stderr, "Chyba: Nedostatok pamate pre kompresne sekvencie.\n");
            free(pixels);
            return 1;
        }
        size_t sequence_count = 0;

        for (int block_y = 0; block_y < img_height / block_s; ++block_y) {
            for (int block_x = 0; block_x < img_width / block_s; ++block_x) {
                
                Sequence current_seq;
                current_seq.length = 0; // Indikuje prazdnu sekvenciu

                for (int y = 0; y < block_s; ++y) {
                    for (int x = 0; x < block_s; ++x) {
                        long global_x = block_x * block_s + x;
                        long global_y = block_y * block_s + y;
                        
                        // Zabezpecenie, ze global_y * img_width + global_x nepretecie (long) a je v ramci obrazku
                        if (global_y * img_width + global_x >= img_width * img_height) {
                            fprintf(stderr, "Chyba: Pristup mimo hranice obrazku pri kompresii. (%ld, %ld)\n", global_x, global_y);
                            free(pixels);
                            free(sequences);
                            return 1;
                        }

                        Pixel current_pixel = pixels[global_y * img_width + global_x];

                        if (current_seq.length == 0) { // Prvy pixel v sekvencii alebo nova sekvencia
                             current_seq.pixel = current_pixel;
                             current_seq.length = 1;
                        } else if (pixel_diff(&current_pixel, &current_seq.pixel) <= args.max_diff && current_seq.length < 255) {
                            current_seq.length++;
                        } else { // Sekvencia sa prerusi
                            if (sequence_count >= sequences_capacity) { // Realloc ak je potrebne, ale s inicializaciou by nemalo byt
                                sequences_capacity *= 2;
                                sequences = (Sequence *)realloc(sequences, sequences_capacity * sizeof(Sequence));
                                if (!sequences) {
                                    fprintf(stderr, "Chyba: Nedostatok pamate pre kompresne sekvencie pri realokacii.\n");
                                    free(pixels);
                                    return 1;
                                }
                            }
                            sequences[sequence_count++] = current_seq;
                            current_seq.pixel = current_pixel;
                            current_seq.length = 1;
                        }
                    }
                }
                // Ulozenie poslednej sekvencie v bloku, ak existuje
                if (current_seq.length > 0) {
                    if (sequence_count >= sequences_capacity) { // Znovu kontrola kapacity
                        sequences_capacity *= 2;
                        sequences = (Sequence *)realloc(sequences, sequences_capacity * sizeof(Sequence));
                        if (!sequences) {
                            fprintf(stderr, "Chyba: Nedostatok pamate pre kompresne sekvencie pri realokacii.\n");
                            free(pixels);
                            return 1;
                        }
                    }
                    sequences[sequence_count++] = current_seq;
                }
            }
        }
        
        FILE *out_file = fopen(args.output_path, "wb");
        if (!out_file) {
            fprintf(stderr, "Chyba: Nepodarilo sa vytvorit vystupny subor '%s'.\n", args.output_path);
            free(pixels);
            free(sequences);
            return 1;
        }
        
        // JBK hlavicka: width, height, block_size, original_image_descriptor
        fwrite(&header.width, sizeof(uint16_t), 1, out_file);
        fwrite(&header.height, sizeof(uint16_t), 1, out_file);
        uint16_t block_size_16 = (uint16_t)args.block_size;
        fwrite(&block_size_16, sizeof(uint16_t), 1, out_file);
        fwrite(&header.image_descriptor, sizeof(uint8_t), 1, out_file); // Ulozenie originalneho descriptor
        
        // Sekvencie
        if (sequence_count > 0) {
            if (fwrite(sequences, sizeof(Sequence), sequence_count, out_file) != sequence_count) {
                fprintf(stderr, "Chyba: Nepodarilo sa zapisat vsetky sekvencie do JBK suboru.\n");
                fclose(out_file);
                free(pixels);
                free(sequences);
                return 1;
            }
        }
        
        long original_size = (long)header.width * header.height * 3;
        long compressed_size = (3 * sizeof(uint16_t) + sizeof(uint8_t)) + sequence_count * sizeof(Sequence);
        
        fclose(out_file);

        printf("Kompresia dokoncena.\n");
        printf("Statistika:\n");
        printf("  Velkost originalneho obrazku: %ld bajtov\n", original_size);
        printf("  Velkost komprimovaneho suboru: %ld bajtov\n", compressed_size);
        if (original_size > 0) {
             printf("  Pomer kompresie: %.2f%%\n", (double)compressed_size / original_size * 100.0);
        } else {
             printf("  Pomer kompresie: N/A (originalny obrazok ma nulovu velkost)\n");
        }
       
        free(pixels);
        free(sequences);

    } else if (args.mode == DECOMPRESS) {
        printf("Spustam dekompresiu...\n");
        
        FILE *in_file = fopen(args.input_path, "rb");
        if (!in_file) {
            fprintf(stderr, "Chyba: Nepodarilo sa otvorit vstupny JBK subor '%s'.\n", args.input_path);
            return 1;
        }

        TgaHeader decompressed_header_template = {0}; // Pouzijeme pre prenesenie sirky, vysky a descriptor
        uint16_t block_size_16;
        
        // Citanie JBK hlavicky
        if (fread(&decompressed_header_template.width, sizeof(uint16_t), 1, in_file) != 1 ||
            fread(&decompressed_header_template.height, sizeof(uint16_t), 1, in_file) != 1 ||
            fread(&block_size_16, sizeof(uint16_t), 1, in_file) != 1 ||
            fread(&decompressed_header_template.image_descriptor, sizeof(uint8_t), 1, in_file) != 1) {
            fprintf(stderr, "Chyba: Nepodarilo sa precitat JBK hlavicku.\n");
            fclose(in_file);
            return 1;
        }

        fseek(in_file, 0, SEEK_END);
        long file_size = ftell(in_file);
        fseek(in_file, (3 * sizeof(uint16_t) + sizeof(uint8_t)), SEEK_SET); // Posunieme sa za JBK hlavicku

        size_t num_sequences = (file_size - (3 * sizeof(uint16_t) + sizeof(uint8_t))) / sizeof(Sequence);
        Sequence *sequences = (Sequence *)malloc(num_sequences * sizeof(Sequence));
        if (!sequences) {
            fprintf(stderr, "Chyba: Nedostatok pamate pre sekvencie.\n");
            fclose(in_file);
            return 1;
        }

        if (num_sequences > 0 && fread(sequences, sizeof(Sequence), num_sequences, in_file) != num_sequences) {
            fprintf(stderr, "Chyba: Nepodarilo sa nacitat vsetky sekvencie z JBK suboru.\n");
            free(sequences);
            fclose(in_file);
            return 1;
        }
        fclose(in_file);

        long img_width = decompressed_header_template.width;
        long img_height = decompressed_header_template.height;
        int block_s = (int)block_size_16;

        Pixel *pixels = (Pixel *)calloc(img_width * img_height, sizeof(Pixel));
        if (!pixels) {
            fprintf(stderr, "Chyba: Nedostatok pamate pre dekomprimovane pixely.\n");
            free(sequences);
            return 1;
        }

        size_t current_seq_idx = 0;
        int used_in_current_seq = 0;

        for (int block_y = 0; block_y < img_height / block_s; ++block_y) {
            for (int block_x = 0; block_x < img_width / block_s; ++block_x) {
                for (int y_in_block = 0; y_in_block < block_s; ++y_in_block) {
                    for (int x_in_block = 0; x_in_block < block_s; ++x_in_block) {
                        if (current_seq_idx >= num_sequences) {
                            fprintf(stderr, "Chyba: Nespravny format JBK suboru, nedostatok sekvencii pre dany pocet pixelov. (pretecenie na pozicii %d, %d v bloku %d, %d)\n", x_in_block, y_in_block, block_x, block_y);
                            goto cleanup_decompress; // Skocime na cistenie pamate
                        }

                        long global_x = block_x * block_s + x_in_block;
                        long global_y = block_y * block_s + y_in_block;
                        
                        // Zabezpecenie, ze global_y * img_width + global_x nepretecie (long) a je v ramci obrazku
                        if (global_y * img_width + global_x >= img_width * img_height) {
                            fprintf(stderr, "Chyba: Pristup mimo hranice obrazku pri dekompresii. (%ld, %ld)\n", global_x, global_y);
                            goto cleanup_decompress;
                        }

                        pixels[global_y * img_width + global_x] = sequences[current_seq_idx].pixel;
                        used_in_current_seq++;

                        if (used_in_current_seq == sequences[current_seq_idx].length) {
                            current_seq_idx++;
                            used_in_current_seq = 0;
                        }
                    }
                }
            }
        }
        
        // Po prechode vsetkych blokov by mala byt aktualna sekvencia dokoncena a vsetky sekvencie spotrebovane
        if (current_seq_idx < num_sequences || used_in_current_seq != 0) {
             fprintf(stderr, "Upozornenie: JBK subor obsahuje prebytocne sekvencie alebo nedokonceny poslednu sekvenciu.\n");
        }


        if (write_tga(args.output_path, &decompressed_header_template, pixels) != 0) {
            goto cleanup_decompress;
        }
        
        printf("Dekompresia dokoncena.\n");
        
    cleanup_decompress:
        free(pixels);
        free(sequences);
        if (ferror(stderr)) return 1; // Ak bola chyba zapisana na stderr pred cleanup, vratime chybovy kod
    }

    return 0;
}
