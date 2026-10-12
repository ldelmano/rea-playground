import sys
msg = "Licenca valida. Bem-vindo, operador!".encode()
enc = [b ^ ((0x5A + 7*i) & 0xFF) for i, b in enumerate(msg)]
open(sys.argv[1] + "/msg.inc", "w").write(",".join("0x%02x" % b for b in enc) + "\n")
def mix(s):
    h = 0x1337
    for c in s:
        if c == '-': continue
        h = (((h*31) + ord(c)) ^ (h >> 7)) & 0xFFFF
    return h
pre = "K7Q2-2Q7K-5528"
print(pre + "-%04X" % mix(pre))
