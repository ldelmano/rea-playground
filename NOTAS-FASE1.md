# REA: avaliação, notas parciais (Fase 1, bloqueada)

## Instalado
- JADX 1.5.6, release oficial de github.com/skylot/jadx. Instalado em /opt/tools/jadx.
  - SHA-256 do zip: 545ea2be9c242511bc145755cf4bda2485ade42966e096f8b4d3da2a230e8974
  - O release não publica um checksum para conferir.
- O JDK 21 já vinha no container (OpenJDK 21.0.12).
- rea-agents@6.4.0, instalado em /opt/rea com versão fixa.
  - SHA-256 do tarball: 59eb1604b8db75f522d4a975c36ce6b4a2ad465e43bad165f9a8f728391db9b8
  - O pacote não tem scripts preinstall, install ou postinstall, nem ele nem as dependências. Instalação em cerca de 8 s.

## O que `rea setup --yes --client claude_code` alterou (conferido com diff)
- `~/.claude.json`: foi adicionado `mcpServers.rea = {command: "npx", args: ["-y","rea-agents@6.4.0","mcp"]}`.
  - O backup foi gravado em `~/.claude.json.rea.backup`.
  - O resto do arquivo ficou intacto.
- `~/.claude/skills/reverse-engineer-anything/`: SKILL.md mais a pasta references/.
- Não alterou PATH, arquivos rc, settings.json nem outros agentes.
- Não precisou de TTY. `--dry-run` mostra o plano antes de aplicar.
- O MCP não aparece nas ferramentas da sessão já aberta, porque o Claude Code carrega o MCP na inicialização. Por isso a avaliação usa a CLI `rea`.
- `npx -y` busca o pacote no registry npm a cada inicialização do cliente. A versão fixa reduz o risco, mas o pacote não fica vendorizado.

## Bloqueios
1. O proxy da sessão nega o f-droid.org (403 no CONNECT), então não dá para baixar o APK do Catima pela página oficial.
   - Os releases do GitHub do Catima (v2.45.1) não trazem APK.
2. O REA não usa o JADX oficial diretamente.
   - A decompilação Android (`search-android-classes`, `inspect-android-method`) exige `REA_JADX_MCP_JAR`, apontando para o jadx-headless-mcp 0.7.1.
   - Esse JAR é de terceiros: github.com/1013503897/jadx-headless-mcp. Ver dist/.../android/JadxRelease.js e JadxConfiguration.js.
   - `decode-android-resources` exige o apktool.
   - Nenhum dos dois está na lista de downloads permitidos.
3. A Fase 2 exige Ghidra 12.1.x (testado com 12.1.4) via `GHIDRA_INSTALL_DIR`. Isso é compatível com o pedido de "Ghidra 12.x".

## Rede e telemetria (análise estática do pacote)
- Não há código de telemetria.
- Hosts encontrados no código:
  - hopperapp.com: só no instalador opcional do Hopper.
  - api.github.com: só no install.sh.
  - npm: em `rea update`.
- Nenhum texto tentando dar instruções ao agente foi achado no pacote nem na skill.
