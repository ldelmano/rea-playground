/* Alvo de teste para avaliar o REA (Fase 2). Validacao de serial + string XOR. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* "Licenca valida. Bem-vindo, operador!" com XOR de chave rotativa 0x5A + 7*i */
static const unsigned char secret_msg[] = {
    /* preenchido pelo gerador abaixo */
#include "msg.inc"
};

static void reveal(const unsigned char *src, size_t n, char *out) {
    for (size_t i = 0; i < n; i++)
        out[i] = (char)(src[i] ^ (unsigned char)(0x5A + 7 * i));
    out[n] = '\0';
}

static unsigned mix(const char *s, size_t n) {
    unsigned h = 0x1337;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '-') continue;
        h = ((h * 31u) + (unsigned char)s[i]) ^ (h >> 7);
        h &= 0xFFFFu;
    }
    return h;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Formato: AAAA-BBBB-CCCC-DDDD */
static int check_serial(const char *s) {
    if (strlen(s) != 19) return 0;
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 9 || i == 14) {
            if (s[i] != '-') return 0;
        } else if (!isupper((unsigned char)s[i]) && !isdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    /* grupo 2 e o grupo 1 invertido */
    for (int i = 0; i < 4; i++)
        if (s[5 + i] != s[3 - i]) return 0;
    /* grupo 3: so digitos, soma 20 */
    int sum = 0;
    for (int i = 10; i < 14; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
        sum += s[i] - '0';
    }
    if (sum != 20) return 0;
    /* grupo 4: checksum hexadecimal dos 3 primeiros grupos */
    unsigned h = mix(s, 14), got = 0;
    for (int i = 15; i < 19; i++) {
        int v = hexval(s[i]);
        if (v < 0) return 0;
        got = (got << 4) | (unsigned)v;
    }
    return got == h;
}

int main(int argc, char **argv) {
    char buf[64];
    if (argc != 2) {
        fprintf(stderr, "uso: %s <serial>\n", argv[0]);
        return 2;
    }
    if (check_serial(argv[1])) {
        reveal(secret_msg, sizeof secret_msg, buf);
        puts(buf);
        return 0;
    }
    puts("Serial invalido.");
    return 1;
}
