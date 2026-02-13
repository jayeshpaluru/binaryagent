#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BINARY_OUT "program.binary.txt"
#define DEFAULT_PROGRAM_OUT "program.html"
#define DEFAULT_PROMPT "Kinetic glassmorphism playground"
#define DEFAULT_STYLE "sunset"

static void print_usage(const char *name) {
    printf(
        "Usage:\n"
        "  %s generate [--prompt TEXT] [--style sunset|mint|neon] [--binary-out FILE]\n"
        "  %s encode --program-in FILE [--binary-out FILE]\n"
        "  %s decode [--binary-in FILE|-] [--program-out FILE]\n\n"
        "  %s validate [--binary-in FILE|-]\n\n"
        "Examples:\n"
        "  %s generate --prompt \"futuristic landing page\" --style neon\n"
        "  %s decode < agent_output.binary.txt\n"
        "  %s decode --binary-in program.binary.txt --program-out restored.html\n"
        "  %s validate --binary-in agent_output.binary.txt\n",
        name, name, name, name, name, name, name, name
    );
}

static bool is_valid_style(const char *style) {
    return strcmp(style, "sunset") == 0 || strcmp(style, "mint") == 0 || strcmp(style, "neon") == 0;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, n + 1);
    return out;
}

static char *sanitize_text(const char *input) {
    size_t n = strlen(input);
    char *out = (char *)malloc(n + 1);
    if (!out) {
        return NULL;
    }

    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '<' || c == '>' || c == '&' || c == '"' || c == '\'' || c == '\\') {
            out[i] = ' ';
        } else if (isprint(c) || isspace(c)) {
            out[i] = (char)c;
        } else {
            out[i] = ' ';
        }
    }
    out[n] = '\0';
    return out;
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

static char *encode_binary(const unsigned char *bytes, size_t len) {
    if (len == 0) {
        return xstrdup("");
    }

    size_t out_len = (len * 9) - 1;
    char *out = (char *)malloc(out_len + 2);
    if (!out) {
        return NULL;
    }

    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            out[pos++] = ((bytes[i] >> bit) & 1) ? '1' : '0';
        }
        if (i + 1 < len) {
            out[pos++] = ' ';
        }
    }
    out[pos++] = '\n';
    out[pos] = '\0';
    return out;
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
        token[8] = '\0';

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

static const char *style_palette(const char *style, const char *key) {
    if (strcmp(style, "mint") == 0) {
        if (strcmp(key, "bg0") == 0) return "#eafff5";
        if (strcmp(key, "bg1") == 0) return "#d9f6ff";
        if (strcmp(key, "ink") == 0) return "#0f1722";
        if (strcmp(key, "panel") == 0) return "rgba(255,255,255,0.62)";
        if (strcmp(key, "a") == 0) return "#1ec7a5";
        if (strcmp(key, "b") == 0) return "#2c8bf0";
        return "#6c7a89";
    }

    if (strcmp(style, "neon") == 0) {
        if (strcmp(key, "bg0") == 0) return "#100b1d";
        if (strcmp(key, "bg1") == 0) return "#1d1233";
        if (strcmp(key, "ink") == 0) return "#ecf4ff";
        if (strcmp(key, "panel") == 0) return "rgba(20,20,34,0.72)";
        if (strcmp(key, "a") == 0) return "#4cf2ff";
        if (strcmp(key, "b") == 0) return "#ff4fcf";
        return "#8993a8";
    }

    if (strcmp(key, "bg0") == 0) return "#fff3e8";
    if (strcmp(key, "bg1") == 0) return "#ffd8d2";
    if (strcmp(key, "ink") == 0) return "#201c24";
    if (strcmp(key, "panel") == 0) return "rgba(255,255,255,0.64)";
    if (strcmp(key, "a") == 0) return "#ff7d4d";
    if (strcmp(key, "b") == 0) return "#7e57ff";
    return "#7a6f84";
}

static char *build_visual_program_html(const char *prompt, const char *style) {
    const char *tpl =
"<!doctype html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"utf-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"  <title>Binary Agent Program</title>\n"
"  <style>\n"
"    :root{--bg0:%s;--bg1:%s;--ink:%s;--panel:%s;--a:%s;--b:%s;--m:%s;}\n"
"    *{box-sizing:border-box} body{margin:0;min-height:100vh;font-family:Menlo,Consolas,monospace;color:var(--ink);background:radial-gradient(1200px 600px at 15%% 10%%,color-mix(in oklab,var(--a),transparent 80%%),transparent),linear-gradient(140deg,var(--bg0),var(--bg1));display:grid;place-items:center;padding:18px;}\n"
"    .card{width:min(920px,100%%);padding:18px;border-radius:20px;background:var(--panel);backdrop-filter:blur(8px);border:1px solid color-mix(in oklab,var(--ink),transparent 85%%);box-shadow:0 20px 45px rgba(0,0,0,.18)}\n"
"    h1{margin:.2rem 0 0;font-size:clamp(1.4rem,2.8vw,2.2rem)} p{margin:.3rem 0 1rem;color:var(--m)}\n"
"    .row{display:grid;grid-template-columns:1.2fr .8fr;gap:16px} @media(max-width:860px){.row{grid-template-columns:1fr}}\n"
"    canvas{width:100%%;height:360px;border-radius:14px;display:block;background:radial-gradient(circle at 65%% 20%%,color-mix(in oklab,var(--a),transparent 72%%),transparent 32%%),#0b111a;border:1px solid rgba(255,255,255,.1)}\n"
"    .panel{border-radius:14px;padding:12px;background:rgba(255,255,255,.52);border:1px solid rgba(0,0,0,.08)}\n"
"    .stats{display:grid;gap:10px;margin-top:10px}.chip{padding:10px;border-radius:12px;background:rgba(255,255,255,.58)}\n"
"    button{border:0;border-radius:999px;padding:10px 14px;font-weight:700;cursor:pointer;background:linear-gradient(90deg,var(--a),var(--b));color:white}\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <main class=\"card\">\n"
"    <h1>Binary-Coded Visual Program</h1>\n"
"    <p>Prompt: %s</p>\n"
"    <section class=\"row\">\n"
"      <canvas id=\"scene\" width=\"760\" height=\"360\"></canvas>\n"
"      <aside class=\"panel\">\n"
"        <strong>Controls</strong>\n"
"        <div class=\"stats\">\n"
"          <div class=\"chip\" id=\"nodeCount\">Nodes: 0</div>\n"
"          <div class=\"chip\" id=\"speedVal\">Speed: 1.00x</div>\n"
"          <button id=\"shuffle\">Shuffle Field</button>\n"
"        </div>\n"
"      </aside>\n"
"    </section>\n"
"  </main>\n"
"  <script>\n"
"    const c=document.getElementById('scene');const x=c.getContext('2d');\n"
"    const nodeCount=document.getElementById('nodeCount');const speedVal=document.getElementById('speedVal');\n"
"    const A=getComputedStyle(document.documentElement).getPropertyValue('--a').trim();\n"
"    const B=getComputedStyle(document.documentElement).getPropertyValue('--b').trim();\n"
"    const rnd=(n=1)=>Math.random()*n;\n"
"    let speed=1;\n"
"    function make(n=70){return Array.from({length:n},()=>({x:rnd(c.width),y:rnd(c.height),vx:rnd(2)-1,vy:rnd(2)-1,r:2+rnd(3)}));}\n"
"    let pts=make(); nodeCount.textContent='Nodes: '+pts.length;\n"
"    function link(p,q){const dx=p.x-q.x,dy=p.y-q.y,d=Math.hypot(dx,dy);if(d<120){x.globalAlpha=(1-d/120)*0.25;x.beginPath();x.moveTo(p.x,p.y);x.lineTo(q.x,q.y);x.stroke();}}\n"
"    function step(){x.clearRect(0,0,c.width,c.height);const g=x.createLinearGradient(0,0,c.width,c.height);g.addColorStop(0,A);g.addColorStop(1,B);x.strokeStyle=g;\n"
"      for(let i=0;i<pts.length;i++){const p=pts[i];p.x+=p.vx*speed;p.y+=p.vy*speed;if(p.x<0||p.x>c.width)p.vx*=-1;if(p.y<0||p.y>c.height)p.vy*=-1;\n"
"        x.globalAlpha=.95;x.fillStyle=i%%2?A:B;x.beginPath();x.arc(p.x,p.y,p.r,0,Math.PI*2);x.fill();for(let j=i+1;j<pts.length;j++)link(p,pts[j]);}\n"
"      requestAnimationFrame(step);\n"
"    }\n"
"    c.addEventListener('mousemove',e=>{const r=c.getBoundingClientRect();speed=Math.min(2.2,0.4+((e.clientX-r.left)/r.width)*2);speedVal.textContent='Speed: '+speed.toFixed(2)+'x';});\n"
"    document.getElementById('shuffle').addEventListener('click',()=>{pts=make(50+Math.floor(Math.random()*40));nodeCount.textContent='Nodes: '+pts.length;});\n"
"    step();\n"
"  </script>\n"
"</body>\n"
"</html>\n";

    char *clean_prompt = sanitize_text(prompt);
    if (!clean_prompt) {
        return NULL;
    }

    size_t needed = (size_t)snprintf(
        NULL,
        0,
        tpl,
        style_palette(style, "bg0"),
        style_palette(style, "bg1"),
        style_palette(style, "ink"),
        style_palette(style, "panel"),
        style_palette(style, "a"),
        style_palette(style, "b"),
        style_palette(style, "m"),
        clean_prompt
    );

    char *out = (char *)malloc(needed + 1);
    if (!out) {
        free(clean_prompt);
        return NULL;
    }

    snprintf(
        out,
        needed + 1,
        tpl,
        style_palette(style, "bg0"),
        style_palette(style, "bg1"),
        style_palette(style, "ink"),
        style_palette(style, "panel"),
        style_palette(style, "a"),
        style_palette(style, "b"),
        style_palette(style, "m"),
        clean_prompt
    );

    free(clean_prompt);
    return out;
}

static const char *arg_value(int argc, char **argv, int *idx, const char *flag) {
    if (*idx + 1 >= argc) {
        fprintf(stderr, "error: %s requires a value\n", flag);
        return NULL;
    }
    (*idx)++;
    return argv[*idx];
}

static int cmd_generate(int argc, char **argv) {
    const char *prompt = DEFAULT_PROMPT;
    const char *style = DEFAULT_STYLE;
    const char *binary_out = DEFAULT_BINARY_OUT;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--prompt") == 0) {
            const char *v = arg_value(argc, argv, &i, "--prompt");
            if (!v) return 1;
            prompt = v;
        } else if (strcmp(argv[i], "--style") == 0) {
            const char *v = arg_value(argc, argv, &i, "--style");
            if (!v) return 1;
            style = v;
        } else if (strcmp(argv[i], "--binary-out") == 0) {
            const char *v = arg_value(argc, argv, &i, "--binary-out");
            if (!v) return 1;
            binary_out = v;
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!is_valid_style(style)) {
        fprintf(stderr, "error: invalid style '%s' (expected: sunset, mint, neon)\n", style);
        return 1;
    }

    char *html = build_visual_program_html(prompt, style);
    if (!html) {
        fprintf(stderr, "error: failed to generate visual program\n");
        return 1;
    }

    size_t html_len = strlen(html);
    char *binary = encode_binary((const unsigned char *)html, html_len);
    if (!binary) {
        free(html);
        fprintf(stderr, "error: failed to encode binary\n");
        return 1;
    }

    bool ok = write_file_bytes(binary_out, (const unsigned char *)binary, strlen(binary));

    if (!ok) {
        free(binary);
        free(html);
        return 1;
    }

    printf("Generated visual program\n");
    printf("Prompt:  %s\n", prompt);
    printf("Style:   %s\n", style);
    printf("Binary:  %s (%zu source bytes)\n", binary_out, html_len);
    printf("Decode with: %s decode --binary-in %s --program-out %s\n", argv[0], binary_out, DEFAULT_PROGRAM_OUT);

    free(binary);
    free(html);
    return 0;
}

static int cmd_encode(int argc, char **argv) {
    const char *program_in = NULL;
    const char *binary_out = DEFAULT_BINARY_OUT;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--program-in") == 0) {
            const char *v = arg_value(argc, argv, &i, "--program-in");
            if (!v) return 1;
            program_in = v;
        } else if (strcmp(argv[i], "--binary-out") == 0) {
            const char *v = arg_value(argc, argv, &i, "--binary-out");
            if (!v) return 1;
            binary_out = v;
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!program_in) {
        fprintf(stderr, "error: --program-in is required\n");
        return 1;
    }

    unsigned char *program = NULL;
    size_t program_len = 0;
    if (!read_file_bytes(program_in, &program, &program_len)) {
        return 1;
    }

    char *binary = encode_binary(program, program_len);
    free(program);

    if (!binary) {
        fprintf(stderr, "error: failed to encode binary\n");
        return 1;
    }

    bool ok = write_file_bytes(binary_out, (const unsigned char *)binary, strlen(binary));
    free(binary);

    if (!ok) {
        return 1;
    }

    printf("Encoded '%s' to '%s'\n", program_in, binary_out);
    return 0;
}

static int cmd_decode(int argc, char **argv) {
    const char *binary_in = NULL;
    const char *program_out = DEFAULT_PROGRAM_OUT;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--binary-in") == 0) {
            const char *v = arg_value(argc, argv, &i, "--binary-in");
            if (!v) return 1;
            binary_in = v;
        } else if (strcmp(argv[i], "--program-out") == 0) {
            const char *v = arg_value(argc, argv, &i, "--program-out");
            if (!v) return 1;
            program_out = v;
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    unsigned char *binary_text = NULL;
    size_t binary_len = 0;
    const char *source_name = NULL;
    if (!binary_in || strcmp(binary_in, "-") == 0) {
        if (!read_stream_bytes(stdin, &binary_text, &binary_len)) {
            return 1;
        }
        source_name = "stdin";
    } else {
        if (!read_file_bytes(binary_in, &binary_text, &binary_len)) {
            return 1;
        }
        source_name = binary_in;
    }

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    bool ok = decode_binary_text((const char *)binary_text, &decoded, &decoded_len);
    free(binary_text);

    if (!ok) {
        return 1;
    }

    if (decoded_len == 0) {
        fprintf(stderr, "error: binary input was empty\n");
        free(decoded);
        return 1;
    }

    ok = write_file_bytes(program_out, decoded, decoded_len);
    free(decoded);

    if (!ok) {
        return 1;
    }

    printf("Decoded '%s' to '%s' (%zu bytes)\n", source_name, program_out, decoded_len);
    return 0;
}

static int cmd_validate(int argc, char **argv) {
    const char *binary_in = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--binary-in") == 0) {
            const char *v = arg_value(argc, argv, &i, "--binary-in");
            if (!v) return 1;
            binary_in = v;
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    unsigned char *binary_text = NULL;
    size_t binary_len = 0;
    const char *source_name = NULL;
    if (!binary_in || strcmp(binary_in, "-") == 0) {
        if (!read_stream_bytes(stdin, &binary_text, &binary_len)) {
            return 1;
        }
        source_name = "stdin";
    } else {
        if (!read_file_bytes(binary_in, &binary_text, &binary_len)) {
            return 1;
        }
        source_name = binary_in;
    }

    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    bool ok = decode_binary_text((const char *)binary_text, &decoded, &decoded_len);
    free(binary_text);

    if (!ok) {
        return 1;
    }

    if (decoded_len == 0) {
        fprintf(stderr, "error: binary input was empty\n");
        free(decoded);
        return 1;
    }

    free(decoded);
    printf("Valid binary stream: %s (%zu bytes)\n", source_name, decoded_len);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "generate") == 0) {
        return cmd_generate(argc, argv);
    }

    if (strcmp(argv[1], "encode") == 0) {
        return cmd_encode(argc, argv);
    }

    if (strcmp(argv[1], "decode") == 0) {
        return cmd_decode(argc, argv);
    }

    if (strcmp(argv[1], "validate") == 0) {
        return cmd_validate(argc, argv);
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    fprintf(stderr, "error: unknown command '%s'\n\n", argv[1]);
    print_usage(argv[0]);
    return 1;
}
