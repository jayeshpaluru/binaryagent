#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BINARY_OUT "output.binary.txt"
#define DEFAULT_HTML_OUT "output.html"

static void print_usage(const char *name) {
    printf(
        "Usage:\n"
        "  %s materialize [--binary-in FILE|-] [--html-out FILE] [--binary-out FILE]\n\n"
        "What it does:\n"
        "  1) Reads strict 8-bit binary text from --binary-in (or stdin)\n"
        "  2) Decodes to bytes and validates HTML quality\n"
        "  3) Writes validated HTML to --html-out\n"
        "  4) Optionally writes binary copy to --binary-out\n\n"
        "Binary output behavior:\n"
        "  - if input is FILE and --binary-out is omitted, no binary copy is written\n"
        "  - if input is stdin and --binary-out is omitted, binary is saved to output.binary.txt\n\n"
        "Validation rules:\n"
        "  - must decode to HTML (<html> or <!doctype html>)\n"
        "  - must include CSS styling (<style> or style=)\n"
        "  - must pass creative UI checks (layout + typography + visual depth + motion/interaction)\n\n"
        "Examples:\n"
        "  %s materialize --binary-in agent_output.binary.txt --binary-out run.binary.txt --html-out run.html\n"
        "  %s materialize --binary-out run.binary.txt --html-out run.html < agent_output.binary.txt\n",
        name, name, name
    );
}

static const char *arg_value(int argc, char **argv, int *idx, const char *flag) {
    if (*idx + 1 >= argc) {
        fprintf(stderr, "error: %s requires a value\n", flag);
        return NULL;
    }
    (*idx)++;
    return argv[*idx];
}

static bool read_file_bytes(const char *path, unsigned char **data, size_t *len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: fseek failed for '%s'\n", path);
        fclose(fp);
        return false;
    }

    long size = ftell(fp);
    if (size < 0) {
        fprintf(stderr, "error: ftell failed for '%s'\n", path);
        fclose(fp);
        return false;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: rewind failed for '%s'\n", path);
        fclose(fp);
        return false;
    }

    unsigned char *buf = (unsigned char *)malloc((size_t)size + 1);
    if (!buf) {
        fprintf(stderr, "error: out of memory\n");
        fclose(fp);
        return false;
    }

    size_t got = fread(buf, 1, (size_t)size, fp);
    fclose(fp);

    if (got != (size_t)size) {
        fprintf(stderr, "error: short read for '%s'\n", path);
        free(buf);
        return false;
    }

    buf[got] = '\0';
    *data = buf;
    *len = got;
    return true;
}

static bool read_stream_bytes(FILE *fp, unsigned char **data, size_t *len) {
    size_t cap = 4096;
    size_t used = 0;
    unsigned char *buf = (unsigned char *)malloc(cap + 1);
    if (!buf) {
        fprintf(stderr, "error: out of memory\n");
        return false;
    }

    for (;;) {
        if (used == cap) {
            cap *= 2;
            unsigned char *tmp = (unsigned char *)realloc(buf, cap + 1);
            if (!tmp) {
                fprintf(stderr, "error: out of memory\n");
                free(buf);
                return false;
            }
            buf = tmp;
        }

        size_t n = fread(buf + used, 1, cap - used, fp);
        used += n;

        if (n == 0) {
            if (ferror(fp)) {
                fprintf(stderr, "error: failed reading input stream\n");
                free(buf);
                return false;
            }
            break;
        }
    }

    buf[used] = '\0';
    *data = buf;
    *len = used;
    return true;
}

static bool write_file_bytes(const char *path, const unsigned char *data, size_t len) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "error: cannot write '%s': %s\n", path, strerror(errno));
        return false;
    }

    size_t wrote = fwrite(data, 1, len, fp);
    if (wrote != len) {
        fprintf(stderr, "error: short write for '%s'\n", path);
        fclose(fp);
        return false;
    }

    if (fclose(fp) != 0) {
        fprintf(stderr, "error: close failed for '%s'\n", path);
        return false;
    }

    return true;
}

static bool decode_binary_text(const char *text, unsigned char **out, size_t *out_len) {
    size_t cap = 256;
    size_t len = 0;
    unsigned char *buf = (unsigned char *)malloc(cap);
    if (!buf) {
        fprintf(stderr, "error: out of memory\n");
        return false;
    }

    const char *p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) {
            break;
        }

        char token[9];
        int k = 0;
        while (*p && !isspace((unsigned char)*p)) {
            if (k >= 8) {
                fprintf(stderr, "error: invalid token length (must be 8 bits)\n");
                free(buf);
                return false;
            }
            if (*p != '0' && *p != '1') {
                fprintf(stderr, "error: invalid char '%c' in binary stream\n", *p);
                free(buf);
                return false;
            }
            token[k++] = *p++;
        }

        if (k != 8) {
            fprintf(stderr, "error: token '%.*s' is not 8 bits\n", k, token);
            free(buf);
            return false;
        }

        unsigned char byte = 0;
        for (int i = 0; i < 8; i++) {
            byte = (unsigned char)((byte << 1) | (token[i] - '0'));
        }

        if (len == cap) {
            cap *= 2;
            unsigned char *tmp = (unsigned char *)realloc(buf, cap);
            if (!tmp) {
                fprintf(stderr, "error: out of memory\n");
                free(buf);
                return false;
            }
            buf = tmp;
        }

        buf[len++] = byte;
    }

    *out = buf;
    *out_len = len;
    return true;
}

static bool read_binary_source(const char *binary_in, unsigned char **binary_text, size_t *binary_len, const char **source_name) {
    if (!binary_in || strcmp(binary_in, "-") == 0) {
        if (!read_stream_bytes(stdin, binary_text, binary_len)) {
            return false;
        }
        *source_name = "stdin";
        return true;
    }

    if (!read_file_bytes(binary_in, binary_text, binary_len)) {
        return false;
    }

    *source_name = binary_in;
    return true;
}

static bool contains_nocase(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) {
        return false;
    }

    size_t needle_len = strlen(needle);
    for (const char *h = haystack; *h; h++) {
        size_t i = 0;
        while (i < needle_len && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == needle_len) {
            return true;
        }
    }
    return false;
}

static bool validate_html_quality(const unsigned char *decoded, size_t decoded_len, char *reason, size_t reason_cap) {
    char *text = (char *)malloc(decoded_len + 1);
    if (!text) {
        snprintf(reason, reason_cap, "out of memory");
        return false;
    }

    memcpy(text, decoded, decoded_len);
    text[decoded_len] = '\0';

    bool looks_html = contains_nocase(text, "<!doctype html") || contains_nocase(text, "<html");
    bool has_css = contains_nocase(text, "<style") || contains_nocase(text, "style=");
    bool has_layout = contains_nocase(text, "display:grid") ||
                      contains_nocase(text, "display: grid") ||
                      contains_nocase(text, "display:flex") ||
                      contains_nocase(text, "display: flex") ||
                      contains_nocase(text, "<main") ||
                      contains_nocase(text, "<section");
    bool has_typography = contains_nocase(text, "font-family") &&
                          (contains_nocase(text, "letter-spacing") ||
                           contains_nocase(text, "clamp(") ||
                           contains_nocase(text, "text-transform") ||
                           contains_nocase(text, "@font-face"));

    const char *visual_markers[] = {
        "gradient",
        "box-shadow",
        "backdrop-filter",
        "border-radius",
        "filter:",
        "mix-blend-mode",
        "clip-path",
        "mask-image"
    };
    const char *motion_markers[] = {"animation", "@keyframes", "transition", "transform"};
    const char *interaction_markers[] = {"addEventListener", "onclick", "mousemove", "pointermove", "<button", ":hover", "<input"};

    int visual_count = 0;
    for (size_t i = 0; i < sizeof(visual_markers) / sizeof(visual_markers[0]); i++) {
        if (contains_nocase(text, visual_markers[i])) {
            visual_count++;
        }
    }

    bool has_motion = false;
    for (size_t i = 0; i < sizeof(motion_markers) / sizeof(motion_markers[0]); i++) {
        if (contains_nocase(text, motion_markers[i])) {
            has_motion = true;
            break;
        }
    }

    bool has_interaction = false;
    for (size_t i = 0; i < sizeof(interaction_markers) / sizeof(interaction_markers[0]); i++) {
        if (contains_nocase(text, interaction_markers[i])) {
            has_interaction = true;
            break;
        }
    }

    free(text);

    if (decoded_len < 1400) {
        snprintf(reason, reason_cap, "decoded HTML is too small to be a creative UI (need richer structure/styles)");
        return false;
    }
    if (!looks_html) {
        snprintf(reason, reason_cap, "decoded output is not HTML (missing <html/doctype)");
        return false;
    }
    if (!has_css) {
        snprintf(reason, reason_cap, "decoded HTML must include styling (<style> block or style= attributes)");
        return false;
    }
    if (!has_layout) {
        snprintf(reason, reason_cap, "decoded HTML must include intentional layout structure (grid/flex/section/main)");
        return false;
    }
    if (!has_typography) {
        snprintf(reason, reason_cap, "decoded HTML must include typography design (font-family plus letter-spacing/clamp/text-transform/@font-face)");
        return false;
    }
    if (visual_count < 2) {
        snprintf(reason, reason_cap, "decoded HTML must include at least two visual depth cues (e.g. gradient, box-shadow, backdrop-filter, clip-path)");
        return false;
    }
    if (!has_motion) {
        snprintf(reason, reason_cap, "decoded HTML must include motion cues (animation/@keyframes/transition/transform)");
        return false;
    }
    if (!has_interaction) {
        snprintf(reason, reason_cap, "decoded HTML must include interaction cues (event handlers, controls, or hover states)");
        return false;
    }

    reason[0] = '\0';
    return true;
}

static int cmd_materialize(int argc, char **argv) {
    const char *binary_in = NULL;
    const char *binary_out = NULL;
    bool binary_out_explicit = false;
    const char *html_out = DEFAULT_HTML_OUT;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--binary-in") == 0) {
            const char *v = arg_value(argc, argv, &i, "--binary-in");
            if (!v) return 1;
            binary_in = v;
        } else if (strcmp(argv[i], "--binary-out") == 0) {
            const char *v = arg_value(argc, argv, &i, "--binary-out");
            if (!v) return 1;
            binary_out = v;
            binary_out_explicit = true;
        } else if (strcmp(argv[i], "--html-out") == 0) {
            const char *v = arg_value(argc, argv, &i, "--html-out");
            if (!v) return 1;
            html_out = v;
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    unsigned char *binary_text = NULL;
    size_t binary_len = 0;
    const char *source_name = NULL;
    if (!read_binary_source(binary_in, &binary_text, &binary_len, &source_name)) {
        return 1;
    }

    if (!binary_out_explicit && (!binary_in || strcmp(binary_in, "-") == 0)) {
        binary_out = DEFAULT_BINARY_OUT;
    }

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    bool ok = decode_binary_text((const char *)binary_text, &decoded, &decoded_len);

    if (!ok) {
        free(binary_text);
        return 1;
    }

    if (decoded_len == 0) {
        fprintf(stderr, "error: binary input was empty\n");
        free(decoded);
        free(binary_text);
        return 1;
    }

    char validation_reason[256];
    if (!validate_html_quality(decoded, decoded_len, validation_reason, sizeof(validation_reason))) {
        fprintf(stderr, "error: rejected decoded program: %s\n", validation_reason);
        free(decoded);
        free(binary_text);
        return 1;
    }

    ok = write_file_bytes(html_out, decoded, decoded_len);
    if (ok && binary_out) {
        ok = write_file_bytes(binary_out, binary_text, binary_len);
    }

    free(decoded);
    free(binary_text);

    if (!ok) {
        return 1;
    }

    printf("Materialized agent output\n");
    printf("Source: %s\n", source_name);
    if (binary_out) {
        printf("Binary: %s (%zu bytes)\n", binary_out, binary_len);
    } else {
        printf("Binary: (not written; pass --binary-out FILE to save a copy)\n");
    }
    printf("HTML:   %s (%zu bytes)\n", html_out, decoded_len);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "materialize") == 0) {
        return cmd_materialize(argc, argv);
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    fprintf(stderr, "error: unknown command '%s'\n\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}
