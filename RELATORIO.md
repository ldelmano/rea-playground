# Avaliação do REA (rea-agents@6.4.0): relatório final

**Veredito:** vale usar localmente, como camada de evidência em cima do JADX ou do Ghidra, para triagem e explicação de APKs e binários pequenos.

- Os fatos que o REA extraiu estavam certos.
- O único erro da Fase 1 foi introduzido pelo modelo que leu a saída, e não pelo REA.
- Ele não substitui ler o código: a descoberta em apps ofuscados pelo R8 é fraca.

## Alvos
| Fase | Alvo | Versão | SHA-256 |
|---|---|---|---|
| 1 | Catima, APK do F-Droid (`catima_1004.apk`) | 2.45.1 (versionCode 1004) | `271980b62f73137db2d843fb4fc35f103c4126d769f816d365206c5c1adff3a9` |
| 1 | Fonte do Catima, tag `v2.45.1` | commit `0527d314` | |
| 2 | `licencheck` (meu C, 77 linhas, `gcc -O2 -s`) | | `228a4307447937c55ad8c6ac14ab1dc2b7594ee26697c1ceecab5564d50a4484` |

## Fase 1: Catima (respostas obtidas só pelo REA, depois conferidas com a fonte)
Os 38 itens checados ficaram assim: 22 corretos, 9 parciais, 1 errado e 0 inventados. Detalhes em `fase1-verificacao.md`; as respostas brutas estão em `fase1-respostas-rea.md`.

| Pergunta | Resposta do REA (resumo) | Veredito | Evidência na fonte |
|---|---|---|---|
| a. Códigos de barras | ZXing core para gerar (13 formatos: AZTEC, CODABAR, CODE_39/93/128, DATA_MATRIX, EAN_8/13, ITF, PDF_417, QR, UPC_A/E). zxing-android-embedded para a câmera. Leitura de imagem, PDF e pkpass com `MultiFormatReader` sem restrição de formato | **Correta** (lista de formatos idêntica). O mapeamento "ordinal → writer" é artefato do R8 e engana; não viu o aviso de formato não suportado | `CatimaBarcode.java:12-26`, `Utils.java:437-441`, `BarcodeImageWriterTask.java:186` |
| b. Importação | Catima (zip com `catima.csv` + PNGs, ou CSV; formatos v1/v2), Fidme (BETA, `loyalty_programs.csv` com `;`), Voucher Vault (JSON). Fluxo: `ImportExportActivity`, depois `MultiFormatImporter`, depois o importador em transação no `DBHelper`. Senha errada vira `BadPassword` | **Parcialmente correta.** Errou ao dizer que imagem, PDF e pkpass entram pela `ScanActivity` (o filtro está na `MainActivity`). Omitiu a opção "Loyalty Card Keychain" e um host de URL. Não viu as colunas do Fidme | `MultiFormatImporter.java:31-58`, `CatimaImporter.java:79-95`, `FidmeImporter.java:59-131`, `AndroidManifest.xml:56-91,149-152` |
| c. Permissões | As 9 batem, mais as custom `READ_CARDS` (protege o `CardsContentProvider`) e `DYNAMIC_RECEIVER…`. Usos: CAMERA em `ScanActivity`; STORAGE só em SDK < 24; BT_CONNECT, POST_NOTIFICATIONS e FOREGROUND_* no `BluetoothServerService` (sync com Wear OS); NFC só para *bloquear* pagamento enquanto o cartão aparece na tela. BLUETOOTH e BLUETOOTH_ADMIN "não localizadas" | **Correta**, nenhuma inventada. Admitiu honestamente que não achou as permissões legadas de BT, que não têm chamada explícita no código | `AndroidManifest.xml:5-22`, `LoyaltyCardViewActivity.java:507-540`, `PermissionUtils.java:23-60`, `BluetoothServerService.kt:92-139` |
| d. Rede e telemetria | Sem `INTERNET`, sem SDK de rede ou analytics. ACRA só por e-mail (`acra-crash@catima.app`), com diálogo e sem envio automático | **Correta.** Não viu que o ACRA depende do flavor (desligado no `gplay`) nem os links da tela "Sobre", que só abrem o navegador | `AndroidManifest.xml:14-22`, `LoyaltyCardLockerApplication.java:23-38`, `build.gradle.kts:34,76` |

**Origem do único erro.** O manifesto decodificado pelo REA mostra o filtro de `application/pdf` na `MainActivity`, conferido em `/opt/apk/pkg.json`. Quem atribuiu o filtro à `ScanActivity` foi o agente que interpretava a saída. Portanto o erro está no par REA + LLM, e não no dado.

## Fase 2: binário nativo (REA com Ghidra 12.1.4)
| Pergunta | Resposta do REA | Veredito | Evidência (`fase2/licencheck.c`) |
|---|---|---|---|
| O que faz e códigos de saída | Valida um serial: 2 = uso errado, 1 = inválido, 0 = válido | Correta | linhas 64-76 |
| Formato `AAAA-BBBB-CCCC-DDDD` | 19 caracteres, hífens em 4, 9 e 14, só maiúsculas e dígitos | Correta | 36-43 |
| Grupo 2 = grupo 1 invertido | Sim | Correta | 44-46 |
| Grupo 3 só com dígitos, soma 20 | Sim | Correta | 47-53 |
| Checksum | seed 0x1337, `h = ((h*31+c) ^ (h>>7)) & 0xFFFF`, hífens ignorados, comparado com 4 dígitos hex maiúsculos | Correta, apesar de `mix()` ter sido inlinada pelo -O2 | 18-25, 54-61 |
| String XOR | 36 bytes em 0x102040, chave `0x5A + 7*i`, decodificada para "Licenca valida. Bem-vindo, operador!" | Correta | 7-16 |
| Serial válido | `AB12-21BA-5555-9F15` (derivado sem executar) | **Correta**: executei e saiu com código 0 | `gen.py` gera outro válido, `K7Q2-2Q7K-5528-9693` |

Ressalva: o alvo é pequeno e sem ofuscação de fluxo. O resultado prova que a integração com o Ghidra funciona, mas não prova robustez em binários reais.

## O que o setup alterou de fato
Comando: `rea setup --yes --client claude_code`. Antes de aplicar, conferi o plano com `--dry-run`; depois confirmei o efeito com diff.
- `~/.claude.json`: entrou `mcpServers.rea = {command:"npx", args:["-y","rea-agents@6.4.0","mcp"]}`. O backup ficou em `~/.claude.json.rea.backup`; o resto do arquivo ficou intacto.
- `~/.claude/skills/reverse-engineer-anything/`: `SKILL.md` e `references/`.
- Não mexeu em PATH, arquivos rc, `settings.json` nem em outros agentes. Não precisou de TTY.
- **Problema:** o registro MCP é gravado sem `env`. Por isso, pelo MCP, o JADX e o Ghidra falham com `capability_unavailable`/`provider_unavailable`; só a CLI funciona (com `REA_JADX_MCP_JAR`, `REA_APKTOOL_COMMAND` e `GHIDRA_INSTALL_DIR` exportados). Para o MCP funcionar, exporte essas variáveis antes de rodar o setup (o setup detecta essas variáveis e grava `env` no registro, segundo o código, mas não testei) ou edite `~/.claude.json` à mão. Além disso, o MCP só aparece depois de reiniciar a sessão.
- **Ponto de atenção:** com `npx -y`, o pacote é buscado no registry npm a cada vez que o cliente inicia. A versão é fixa, mas o pacote não fica guardado localmente.

## Dependências, tempo e problemas
| Item | Versão e origem | SHA-256 | Tempo |
|---|---|---|---|
| JDK | OpenJDK 21.0.12, já estava instalado | | 0 |
| rea-agents | 6.4.0, npm. Sem scripts de instalação, nem no pacote nem nas dependências | tgz `59eb1604…b9b8` | ~8 s |
| JADX | 1.5.6, release oficial. **O REA não usa** | `545ea2be…8974` | segundos |
| jadx-headless-mcp | 0.7.1, terceiro, **exigido** pelo REA (autorizado por você) | `6e5eacf5…ff75`, bate com o valor fixado no REA | segundos |
| apktool | 3.0.3, release oficial (autorizado por você) | `dbf930b0…9423` | segundos |
| Ghidra | 12.1.4 (`ghidra_12.1.4_PUBLIC_20260921.zip`), release oficial. O REA exige 12.1.x | `ddac49f9…d4db` | ~20 s para 570 MB |

Problemas encontrados:
1. O f-droid.org estava bloqueado pela política de rede; foi preciso liberar e reiniciar a sessão.
2. A API e as páginas de release do GitHub estavam bloqueadas. Os nomes dos assets saíram por `git ls-remote` e sondagem de URL, e não deu para conferir checksums publicados, porque nenhum dos releases os publica em arquivo.
3. A documentação (`docs/`) não vem no pacote npm, então os links do README ficam quebrados.

Tempo de análise:
- **Fase 1:** cerca de 80 chamadas ao REA, ~27 min. Cada chamada leva 5 a 15 s, por causa da JVM.
- **Fase 2:** ~6 chamadas. A primeira análise do Ghidra levou ~14 s; com `--snapshot`, as seguintes levaram menos de 1 s.

## Comportamento suspeito
- **Rede inesperada:** nenhuma. Pelo log de falhas do proxy, todas as conexões negadas foram de plugins MCP de design, não do REA. A análise estática do pacote não achou telemetria. Ressalva: o proxy só registra falhas, então conexões bem-sucedidas não foram auditadas.
- **Processos:** nenhuma JVM ou Ghidra ficou viva depois das chamadas, e os diretórios `/tmp/rea-android-*` foram limpos. O único processo persistente é o servidor MCP (`npx rea-agents@6.4.0 mcp`), como esperado.
- **Texto tentando dar instruções:** nenhum no pacote, na skill, no APK nem no binário.

## Limitações de uso observadas
- Não há busca por string ou constante no Android. Achar onde cada permissão é usada exigiu chutar métodos.
- O `trace-android-references` recusa classes do framework (`java.net.*`, `ConnectivityManager`).
- O `search-android-classes` sempre devolve, em `raw_result`, o inventário inteiro de 3.292 classes (~300 KB).
- Classes internas (por exemplo `BluetoothServerService$AcceptThread`) não aparecem.
- Nomes fundidos pelo R8 são mostrados sem aviso. Exemplo: o importador do Fidme aparece como `androidx.transition.Transition.AnonymousClass1`.
- A CLI sugeriu um comando errado em um erro: pediu `list-architectures` quando o certo era `search --kind procedures`.
