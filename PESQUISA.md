# REA: o que dá para aproveitar (pesquisa e teste com .NET)

Relatórios brutos: `pesquisa/repo-rea.md` (código e docs do repositório oficial), `pesquisa/web.md` (web) e `dotnet/respostas-rea.md` (teste .NET às cegas).

## Conclusão
O REA não decompila sozinho. Ele **organiza e prova** o que outros motores (Ghidra, JADX, ILSpy) extraem.
- O valor dele está em dar a um agente de IA acesso ordenado a esses motores e em gerar evidência rastreável: hash, token e limitações declaradas.
- O ganho de entendimento vem do motor por baixo e do LLM que lê a saída (certo, observado nos 3 testes).
- Para legado de cliente, use o REA como **inventário e trilha de auditoria**, não como decompilador.

## O que o repositório revelou (não estava no pacote npm)
- **Licença e versões:** MIT. Projeto de outubro de 2026, com 5 versões em 4 dias (6.0 a 6.4). O HEAD está cerca de 330 commits além da 6.4.0 (certo).
- **Quem mantém:** mantido por "morluto", com 74 autores no histórico visível (certo). Na web, só achei divulgação em agregadores: nenhum review técnico independente (provável). **Maturidade de produção não demonstrada.**
- **Tecnologias de legado:** zero menções a Delphi, VB6, PowerBuilder, FoxPro, COBOL ou Clipper (certo, conferido com grep). Para esses casos, o REA é só uma casca sobre o Ghidra.
- **"Reconstrução" não gera código.** É um livro de obrigações: cada regra reimplementada precisa de dono, fixtures positivas, negativas e malformadas, e um verificador. Serve para provar que a reimplementação cobre o comportamento observado (certo).
- **Correção do MCP sem env:** documentada indiretamente. É preciso pôr as variáveis no bloco `env` de `mcpServers.rea` e reiniciar; o setup as preserva ao rodar de novo (provável, inferido das docs).
- **Segurança:** o `SECURITY.md` diz "not a sandbox". Não há seção sobre prompt injection em texto decompilado (certo).

## Teste .NET: LibreHardwareMonitor 0.9.6 (.NET Framework 4.7.2, sem PDB)
| Pergunta | Resposta | De onde veio | Veredito na fonte (tag v0.9.6) |
|---|---|---|---|
| Arquitetura | WinForms. `Program.Main` cria `MainForm`. Lib com `Computer`, `IHardware` e `ISensor` | REA (entrada, tipos) + ILSpy | Correta |
| Temperatura Intel | MSR via driver **PawnIO**, não WinRing0. TjMax em 0x1A2, núcleo em 0x19C, pacote em 0x1B1. Instala o PawnIO com `-install` | ILSpy (REA só viu os recursos embutidos) | Correta: `IntelCpu.cs:756-759`, `MainForm.cs:192` |
| Servidor HTTP | Porta padrão 8085. `/data.json`, `/metrics` e `/Sensor?action=Get/Set`. Basic Auth com SHA-256. CORS `*` | ILSpy | Correta: `MainForm.cs:305`, `HttpServer.cs:367-393,547`. Desligado por padrão (`MainForm.cs:316`), o que o agente não confirmou |
| Configurações | XML `LibreHardwareMonitor.config` ao lado do exe | ILSpy | Correta: `MainForm.cs:74,957` |
| P/Invoke | 97 na lib (kernel32, setupapi, nvapi, ADL, nvml…) e 25 no exe | REA | Correta (contagem não reconferida linha a linha) |

**Bug real achado na análise:**
- **O que acontece:** a senha do servidor web é salva já como hash (`MainForm.cs:955` grava `Server.Password`, cujo getter devolve o hash, `HttpServer.cs:77`). Ao reabrir, esse hash passa de novo pelo setter e é hasheado outra vez (`MainForm.cs:308`, `HttpServer.cs:44,78`). A senha original deixa de funcionar depois de reiniciar o programa (certo pela leitura do código; não executei).
- **Quem achou:** a leitura do C# gerado pelo ILSpy. O REA não teve papel.

**Limites do REA no .NET:**
- `inspect-managed-members` **falhou** na DLL principal: os metadados passaram do orçamento interno de 64 MB. O inventário de membros da parte mais importante ficou de fora.
- O REA não lista strings nem mostra comportamento. Sem o ILSpy, as perguntas sobre temperatura, servidor HTTP e configurações só seriam respondidas pelos nomes dos tipos.
- `import-managed-reconstruction` funcionou em 3 métodos e amarra o C# ao hash do IL, o que serve de prova para auditoria. Porém o schema não está documentado na CLI, não existe conversor do ILSpy e ele não verifica se o C# bate com o IL.

## Onde isso vira dinheiro (provável; depende de validar com clientes)
1. **Diagnóstico de sistema legado .NET ou Java com código perdido.** Esse é o caso em que a pilha funciona bem hoje.
   - **Ferramentas:** ILSpy (ou JADX) + Claude Code para entender, e REA para o inventário de dependências nativas, recursos e hashes.
   - **Entregável:** mapa da arquitetura, regras de negócio, integrações e riscos. No teste apareceram um bug e uma API aberta com CORS `*`.
2. **Modernização com prova.** O livro de obrigações do REA (fixtures e verificadores por regra) vira o plano de testes da reescrita: "o sistema novo cobre as N regras observadas no antigo". É diferencial comercial, mas exige disciplina e foi testado em escala pequena (chute quanto ao esforço em sistema grande).
3. **Delphi, VB6, PowerBuilder, FoxPro:** o REA não ajuda. Use ferramentas específicas (IDR para Delphi, VB Decompiler para VB6) e priorize **banco, stored procedures e entrevista com o usuário**.
4. **Micro SaaS:** o uso direto é marginal. Os recursos de web (`analyze-web-bundle` e afins) servem para auditar **o seu próprio** bundle publicado: vazamento de chaves, SDKs esquecidos, comparação entre a fonte e o que foi publicado. Não testei esses recursos.

## Jurídico (não é aconselhamento jurídico)
- **UE:** a Diretiva 2009/24/CE permite observar e testar (art. 5.º, n.º 3). Descompilar só é permitido para interoperabilidade (art. 6.º), sob condições estritas.
- **Brasil:** a Lei 9.609/98 não tem cláusula equivalente clara (provável).
- **Antes de qualquer trabalho:** obtenha por escrito que o cliente é titular dos direitos do software, ou uma autorização do titular.

## O que mudou no ambiente nesta etapa
- O .NET SDK 8.0.131 foi instalado pelo apt do Ubuntu.
- O `ilspycmd` 9.1.0.7988 foi instalado pelo NuGet.
- O alvo LibreHardwareMonitor v0.9.6 foi baixado do release oficial (SHA-256 `086d9f1b5a99e643edc2cfaaac16051685b551e4c5ac0b32a57c58c0e529c001`).
- O repositório oficial do REA foi clonado só para leitura em `/opt/research/rea`. Nada dele foi executado.
