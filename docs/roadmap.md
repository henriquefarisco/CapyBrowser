# CapyBrowser — Roadmap (curto, médio e longo prazo)

**Status:** proposta de planejamento de produto/engenharia. Subordinada
aos documentos de contrato; não altera ABI nem limites até ser refletida
em `compatibility.md` e na matriz cross-repo do CapyOS.
**Versão atual:** `0.0.6`
**Pin CapyOS:** `0.8.0-alpha.261+20260529` (ver [`compatibility.md`](compatibility.md)).
**Política local:** este computador é **somente leitura/edição**. Todo gate
(`make validate`, `make package`, golden fixtures de host, smokes do CapyOS)
é executado **externamente** por humano ou CI.

## Ordem de autoridade deste documento

1. [`compatibility.md`](compatibility.md) — escopo, ABI, modelo de erro, limites.
2. [`capyos-migration.md`](capyos-migration.md) — histórico de extração e fronteira de propriedade.
3. [`README.md`](README.md) e [`../README.md`](../README.md) — visão geral.
4. **Este roadmap** — sequência de entrega; subordinado aos itens acima.
5. Docs autoritativos do CapyOS (leitura):
   - [`../../CapyOS/docs/reference/integration/compatibility-matrix.md`](../../CapyOS/docs/reference/integration/compatibility-matrix.md)
   - [`../../CapyOS/docs/reference/integration/browser-core-integration-contract.md`](../../CapyOS/docs/reference/integration/browser-core-integration-contract.md)
   - [`../../CapyOS/docs/reference/integration/media-codec-integration-contract.md`](../../CapyOS/docs/reference/integration/media-codec-integration-contract.md)

---

## 1. Princípios invariantes (restrições que moldam todo o roadmap)

Estes pontos não são negociáveis em nenhuma fase e limitam o que é possível:

1. **Desacoplamento total do CapyOS.** Nada em `src/` inclui headers do
   kernel/runtime do CapyOS, nem chama DNS/TCP/HTTP/TLS, filesystem, cache,
   cookies, sandbox, compositor, input ou fontes diretamente. Tudo isso é
   consumido por **callbacks de host adapter injetados**.
2. **CapyBrowser não possui:** rede/TLS, política de certificado, cache,
   cookies, sandbox, janela/input/render/fontes, codecs de imagem
   (`capy-codec-image` é do CapyCodecs), ciclo de vida do app e launcher.
3. **JavaScript bloqueado até a Etapa 12.** Etapas 6-7 nunca executam script.
4. **ABI `capy-browser-core` evolui de forma aditiva** dentro de uma major:
   novos campos, novos códigos de erro, novos tipos de nó de display-list,
   novos warnings — nunca remoção ou reuso de semântica existente.
5. **Fail-closed + determinístico.** Mesmo `(bytes de entrada, base URL,
   limites declarados)` → mesma saída, mesma sequência de warnings, mesmo
   veredito aceitar/rejeitar. Entrada malformada gera erro/warning
   determinístico, nunca crash do host.
6. **HTTPS-first.** Sem auto-degradar para HTTP; sem auto-seguir redirect
   não-HTTPS; sem carregar recursos externos automaticamente no modo texto.
7. **Parser tolerante mas determinístico.** Recupera de HTML malformado sem
   abortar, e a recuperação é reproduzível byte a byte.
8. **Gating por Etapa.** O runtime do navegador depende das Etapas 6-7 do
   CapyOS (hoje na **Etapa 4**). O roadmap entrega código host-testável
   **antes** das Etapas abrirem; a integração só ocorre quando elas abrem.

---

## 2. Mapa dos objetivos do produto × realidade da arquitetura

Os objetivos pedidos (navegador para sites modernos, anônimo, seguro, com
texto e imagens, e — no curto prazo — entrar em sites, navegar e baixar
arquivos) são honestamente classificados assim:

| Objetivo | Onde mora | Horizonte realista | Observação |
|---|---|---|---|
| **Seguro** | CapyBrowser (parse/limites) + CapyOS (TLS/sandbox) | Curto em diante | É o pilar nativo do design (fail-closed, HTTPS-first, sem JS). |
| **Exibir texto** | CapyBrowser (HTML-to-text, text runs) | **Curto** (Etapa 6) | Primeiro entregável real e usável. |
| **Exibir imagens** | CapyCodecs (`capy-codec-image`) + display-list | **Médio** (Etapa 7) | CapyBrowser só faz adapter + placeholders; decode é do CapyCodecs. |
| **Baixar arquivos** | **Nova superfície** CapyBrowser + adapter CapyOS (fs/rede) | **Médio** | Não existe contrato hoje; precisa ser especificado (ver §6.1). |
| **Anônimo** | **Novo modo** CapyBrowser (estado efêmero/UA) + CapyOS (transporte/proxy) | **Médio→Longo** | Não existe contrato hoje; anonimato de transporte é do CapyOS (ver §6.2). |
| **Abrir sites modernos sem grandes problemas** | Exige engine **JS** + reflow dinâmico | **Longo** (Etapa 12+) | Em tensão direta com o design atual; parite total é aspiracional. |

**Conclusão honesta:** o curto prazo entrega um navegador **estático e sem
script** (texto, depois imagens estáticas + download). "Sites modernos sem
grandes problemas" é meta de **longo prazo** e fica atrás da Etapa 12 (JS).

---

## 3. Visão geral das fases e marcos de versão

| Fase | Tema | Versão alvo | Etapa CapyOS | Resultado para o usuário |
|---|---|---|---|---|
| **C0** | Higiene + infra de testes | `0.0.6` | — | Base limpa, fixtures, sem drift de docs |
| **C1** | Core de URL | `0.1.0` | — | Parse/normalização determinística |
| **C2** | `CapyBrowse Text` (HTML→texto) | `0.2.0` | 6 (alvo) | Entrar em sites e ler conteúdo (texto + links) |
| **C3** | Adapter de codec de imagem | `0.2.x` | 6→7 | Pipeline de imagem real (PNG/JPEG/WebP via CapyCodecs) |
| **M1** | Parser HTML tolerante (DOM-like) | `0.3.0` | 7 (prep) | Estrutura para layout |
| **M2** | Subset de CSS + cascade | `0.3.x` | 7 (prep) | Estilo básico determinístico |
| **M3** | Layout estático + display-list | `0.4.0` | 7 (alvo) | Página gráfica estática com texto+imagem+links |
| **M4** | Forms + **download** + base de **modo anônimo** | `0.5.0` | 7 | Navegar, enviar forms simples, baixar arquivos |
| **—** | Estabilização da ABI v1 | `1.0.0` | 7 integrada | `capy-browser-core` v1 estável + gates externos |
| **L1** | Performance, streaming, mais CSS | `1.x` | pós-7 | Páginas estáticas maiores e mais rápidas |
| **L2** | Engine **JavaScript** sandboxed | `2.0.0` | 12 | Sites dinâmicos (subset), bump major de contrato |
| **L3** | DOM dinâmico, reflow, fetch via JS | `2.x` | 12+ | Aproximação de "sites modernos" |
| **L4** | Anonimato avançado, mídia, compat | `2.x`–`3.x` | 12+ | Privacidade de transporte, mais formatos |

> Regra de versão: ABI aditiva dentro da major. A introdução de JS (L2) é o
> candidato natural a **bump major** porque altera as garantias de
> determinismo; exige plano de migração Etapa-gated e atualização dos 5
> documentos de contrato (ver §9).

---

## 4. CURTO PRAZO — core estático host-testável e o navegador de texto

Objetivo do bloco: sair de "só um codec depreciado" para **um navegador de
texto real, seguro e determinístico**, sem depender de runtime CapyOS. Tudo
aqui é puro e testável no host.

### Fase C0 — Higiene e infraestrutura de testes (`0.0.6`)

**Objetivo:** base limpa antes de escrever o core.

**Entregáveis:**
- Corrigir drift de documentação **(já concluído — ver §11)**:
  - `SECURITY.md` ("CapyBrowser 0.0.3" → referência a `VERSION`).
  - Skill `capybrowser-project-map` (`0.0.4`/`alpha.244` → `0.0.6`/`alpha.261`).
  - Centralizar a versão no `Makefile` (`VERSION_STR`; `lint`/`version-check` derivam de `VERSION`).
- Estrutura de teste e harness determinístico **(já concluído — ver §11)**:
  - diretórios de fixtures `tests/fixtures/{url,html-to-text,display-list,malformed}/` + `README.md` de convenção golden;
  - harness header-only `tests/harness/capy_determinism.h` (PRNG semeada + relógio injetado) e `tests/harness/capy_test.h` (asserções + golden de bytes), com self-test em `make test-harness`/`make validate`.
- Stub de codec determinístico fica para a Fase C3 (depende da interface do adapter `capy-codec-image`).

**Contrato/ABI:** nenhuma mudança.
**Gates externos:** `make validate`.
**Critério de saída:** `make validate` verde; nenhuma referência de versão divergente; harness de golden test rodando ao menos um caso por superfície futura.

### Fase C1 — Core de URL (`0.1.0`)

**Objetivo:** parser e normalizador de URL determinísticos — base de toda
navegação e resolução de links.

**Entregáveis (layout proposto):**
- `src/url/url_parse.{c,h}` — parse de URL absoluta e relativa contra base.
- `src/url/url_normalize.c` — lowercase de scheme/host; path case preservado; não-ASCII via percent-encoding; normalização canônica de `.`/`..`.
- `src/url/origin.c` — tupla de origem `(scheme, host, port)`.
- `tests/test_url.c` + `tests/fixtures/url/`.

**Regras:**
- HTTPS-first: schemes não-HTTPS **parseiam**, mas o adapter de host rejeita o fetch.
- Rejeitar bytes de controle, espaço cru e `..` fora da normalização canônica.
- Limite de tamanho alinhado ao CapyOS `HTTP_MAX_URL = 2048`.
- Determinismo: mesma entrada + mesma base → mesma URL normalizada.

**Contrato/ABI:** primeira superfície real de `capy-browser-core` (parser de URL). Documentar em `compatibility.md` e na matriz CapyOS.
**Gates externos:** `make validate` + golden fixtures de URL.
**Critério de saída:** fixtures golden cobrindo absolutas, relativas, percent-encoding, IDN/percent, rejeição de controle/espaço/`..`, e idempotência de normalização.
**Mapeia para:** seguro, base para texto/imagens/download.

### Fase C2 — `CapyBrowse Text` (HTML→texto) (`0.2.0`) — alvo da Etapa 6

**Objetivo:** primeiro navegador **utilizável**: entrar num site e ler
conteúdo em texto, com links numerados navegáveis. Sem JS, sem recurso externo.

**Entregáveis:**
- `src/text/html_entities.c` — decode de entidades HTML comuns.
- `src/text/html_tokenizer.c` — tokenizer tolerante (reutilizado depois pelo parser DOM).
- `src/text/text_emit.c` — emite a saída do contrato CapyBrowse Text.
- `tests/test_text.c` + `tests/fixtures/html-to-text/`.

**Saída (conforme contrato):** título; blocos de texto normalizados; links
numerados com URL resolvida; warnings de parse; status de truncamento.

**Regras:**
- Nunca executa JS; nunca auto-carrega recurso externo.
- Falha fechado em URL inválida, redirect perigoso ou conteúdo acima do limite.
- Decodifica UTF-8 e entidades comuns; saída é texto UTF-8 sem CRLF, sem ANSI, sem bytes de controle fora de whitespace.
- Recuperação tolerante mas determinística.

**Limites (alpha):** entrada HTML ≤ 256 KiB; tempo de parse ≤ 2 s (relógio injetado nos testes).
**Contrato/ABI:** superfície HTML-to-text v1 (adicionada à ABI).
**Gates externos:** golden fixtures de HTML-to-text; rejeição de malformado/truncado; futuramente `make smoke-x64-vmware-capybrowse-text` (quando a Etapa 6 abrir).
**Dependências:** C1 (resolução de URL dos links).
**Critério de saída:** golden fixtures de título/blocos/links/warnings/truncamento; casos malformados recuperam de forma determinística; nenhum byte de controle na saída.
**Mapeia para:** "entrar em sites e navegar" (texto), seguro, exibir texto.

### Fase C3 — Adapter de codec de imagem (`0.2.x`)

**Objetivo:** pipeline de imagem **real** sem embutir codec — consumindo
`capy-codec-image` (CapyCodecs) — e aposentar o snapshot BMP.

**Entregáveis:**
- `src/adapter/host_adapter.h` — define os callbacks injetados, incluindo `decode_image(bytes) -> rgba32` roteado para `capy-codec-image`.
- `src/codec/image_adapter.{c,h}` — orquestra pedido de decode, trata falha (emite evento `image decode failure`, segue com placeholder).
- Marcar `src/codecs/` (BMP) como removível; remover quando o adapter substituir o teste legado (`tests/test_browser_codecs.c`).

**Regras:**
- CapyBrowser **não decodifica** imagem; só pede ao codec via callback.
- Falha de decode é não-fatal: placeholder + warning; a página ainda renderiza.
- Quando imagem é habilitada, o manifesto declara `depends=org.capyos.codecs.image-basic` (já presente no `Makefile`).

**Contrato/ABI:** adiciona o callback de codec ao host adapter; sem novo decoder próprio.
**Gates externos:** `make validate`; checagem de declaração de dependência de codec; auditoria "sem includes diretos do CapyOS".
**Dependências:** CapyCodecs publicando `capy-codec-image` consumível (host-only hoje, `0.0.6`).
**Critério de saída:** snapshot BMP retirado do caminho de build; decode roteado 100% via adapter; teste de falha de decode → placeholder determinístico.
**Mapeia para:** exibir imagens (preparação; render só na Etapa 7).

**Resultado do curto prazo:** um **navegador de texto seguro e determinístico**
(C2), com URL robusto (C1) e pipeline de imagem pronto para a fase gráfica (C3).
Ainda **sem** download e **sem** página gráfica — esses vêm no médio prazo.

---

## 5. MÉDIO PRAZO — navegador gráfico estático, download e base de anonimato

Objetivo do bloco: transformar o core de texto num **navegador gráfico
estático** (Etapa 7), adicionar **download de arquivos** e a **fundação do
modo anônimo**. Aqui entram as superfícies de contrato novas (§6).

### Fase M1 — Parser HTML tolerante → DOM-like (`0.3.0`)

**Objetivo:** representação DOM-like adequada a layout, a partir de HTML real.

**Entregáveis:**
- `src/html/dom.{c,h}` — árvore DOM-like determinística.
- `src/html/html_parse.c` — parser tolerante reaproveitando o tokenizer de C2.
- `tests/test_html.c` + fixtures de HTML válido e malformado.

**Regras:** nunca aborta em malformado; profundidade de layout limitada; mesma entrada → mesma árvore + mesmos warnings.
**Contrato/ABI:** superfície de parse estático (DOM-like) v1.
**Critério de saída:** golden de árvore + warnings; profundidade limitada testada; recuperação determinística.

### Fase M2 — Subset de CSS + cascade (`0.3.x`)

**Objetivo:** estilo estático suficiente para texto e blocos, com subset
**documentado** (sem JS, sem CSS dinâmico).

**Entregáveis:**
- `src/css/css_tokenize.c`, `src/css/css_parse.c`, `src/css/cascade.c`.
- Documentar o subset de CSS suportado em `compatibility.md`.
- `tests/test_css.c` + golden fixtures.

**Subset inicial sugerido:** box model básico, tipografia, cor, display
block/inline, margens/padding/border simples, largura/altura. (Flexbox/grid
ficam para o longo prazo.)
**Critério de saída:** cascade determinístico; subset documentado; golden de specificity/herança.

### Fase M3 — Layout estático + display-list (`0.4.0`) — alvo da Etapa 7

**Objetivo:** emitir a **display-list independente de compositor** que o
CapyOS desenha — primeira página **gráfica**.

**Entregáveis:**
- `src/layout/box.c`, `src/layout/layout.c` — layout estático com profundidade limitada.
- `src/displaylist/display_list.h` — **schema versionado** (`display_list_version`).
- `src/displaylist/display_list_emit.c`.
- `tests/test_layout.c`, `tests/test_displaylist.c` + golden fixtures.

**Tipos de nó iniciais (aditivos):** text runs; rectangles; image
placeholders (bytes via `capy-codec-image`); link bounds (com URL resolvida);
form controls básicos; scroll extent; accessibility labels quando disponível.

**Regras:** versionado, aditivo, determinístico (mesmo parse → mesma sequência de nós, mesmas coordenadas no layout declarado, mesmos a11y labels). Sem suposições específicas de compositor.
**Gates externos:** golden de display-list; futuramente `make smoke-x64-vmware-browser-https-static` e `make smoke-x64-vmware-browser-text-fallback` (preservar o modo texto como fallback).
**Dependências:** M1, M2, C3.
**Critério de saída:** golden estável de display-list para páginas com texto + imagem + links; ordenação determinística; modo texto preservado como fallback.
**Mapeia para:** exibir texto **e** imagens numa página gráfica.

### Fase M4 — Forms básicos + download + base do modo anônimo (`0.5.0`)

**Objetivo:** completar o "navegar e baixar arquivos" do curto prazo do
usuário (que na prática cai aqui, por depender de adapters) e plantar a base
de privacidade.

**Entregáveis:**
- Form controls estáticos e submit simples (GET/POST) via callback de fetch (sem JS).
- **Superfície de download** (ver §6.1): resolução/validação de URL, parse de `Content-Disposition`, sanitização determinística de nome de arquivo, limites de tamanho, streaming para um *download sink* fornecido pelo host (o CapyOS grava o arquivo).
- **Base de modo anônimo** (ver §6.2): sessão efêmera (sem cookies/cache persistentes via flags no adapter), User-Agent mínimo e estático, minimização/zeragem de Referer, sem requests de terceiros automáticos.
**Contrato/ABI:** adiciona superfícies de **download** e **flags de privacidade** ao host adapter (aditivo). Requer atualização cross-repo (§9).
**Gates externos:** golden de sanitização de nome/Content-Disposition; fail-closed em non-HTTPS e path traversal; coordenação com adapter CapyOS de fs/rede.
**Dependências:** adapters de rede + filesystem do CapyOS (Etapa 7), cookie/cache hooks do CapyOS.
**Critério de saída:** download determinístico e fail-closed (HTTPS-only, nome sanitizado, limite aplicado); modo efêmero sem persistência; submit de form simples coberto por fixture.
**Mapeia para:** "baixar arquivos", "navegar", base de "anônimo".

### Marco — Estabilização da ABI `capy-browser-core` v1 (`1.0.0`)

**Objetivo:** declarar a ABI v1 **estável** quando a integração da Etapa 7
estiver validada externamente.
**Critério de saída:** todas as superfícies (URL, HTML-to-text, parse
estático, display-list, modelo de erro, limites, download, flags de
privacidade) documentadas e cobertas por golden; gates externos do CapyOS
(smokes da Etapa 7) aprovados; matriz e auditoria cross-repo atualizadas.

---

## 6. Superfícies de contrato NOVAS (especificadas como planejadas)

As superfícies de **download** e **modo anônimo** já estão **especificadas
como planejadas e aditivas** em [`compatibility.md`](compatibility.md)
("Download surface" e "Private session surface"), com limites e códigos de
erro próprios. Elas permanecem **pendentes de ratificação cross-repo** (§9) —
matriz + contrato CapyOS + auditoria — antes de virarem autoritativas, e só
ficam ativas com a Etapa 7. JavaScript (§6.3) segue proibido até a Etapa 12.

### 6.1 Download de arquivos

- **Divisão de responsabilidade:** CapyBrowser resolve/valida a URL, lê
  metadados (MIME, `Content-Disposition`), deriva e **sanitiza** o nome de
  arquivo de forma determinística, aplica limite de tamanho e **faz streaming
  dos bytes para um *download sink* injetado**. O **CapyOS** grava em disco,
  faz o prompt de "salvar como" (com CapyUI) e aplica sandbox/quota.
- **Fail-closed:** rejeitar non-HTTPS, redirect perigoso, path traversal no
  nome, nomes vazios/controle, tamanho acima do limite.
- **Determinismo:** mesmo `(URL, headers, limites)` → mesmo nome derivado,
  mesmo veredito.
- **ABI:** novo callback `download_open/append/close` + novos códigos de erro
  (aditivos). Nunca abre socket nem escreve arquivo diretamente.

### 6.2 Modo anônimo

CapyBrowser cobre **só a camada de aplicação**; anonimato de **transporte**
(proxy/onion/DNS privado) é do CapyOS.

- **Parte CapyBrowser:** sessão efêmera (sem cookie/cache persistente — via
  flags no adapter); User-Agent mínimo, estático e sem leak de versão;
  Referer minimizado/zerado; sem auto-carregar terceiros; sem JS (já garante
  forte resistência a fingerprint); cabeçalhos de request mínimos e
  determinísticos; sem telemetria.
- **Parte CapyOS:** transporte (proxy/Tor-like se desejado), política de DNS,
  isolamento de armazenamento, certificado.
- **ABI:** flag de "ephemeral/private session" + contrato de User-Agent
  controlado pelo adapter (aditivo).

### 6.3 JavaScript (longo prazo, Etapa 12)

- Hoje **proibido** por contrato. Quando a Etapa 12 abrir, exige engine
  sandboxed, modelo de DOM mutável, reflow dinâmico e, provavelmente, **bump
  major** (altera garantias de determinismo). Plano de migração Etapa-gated
  obrigatório nos 5 docs (§9).

---

## 7. LONGO PRAZO — rumo a "sites modernos"

Bloco aspiracional e dependente da Etapa 12. Honestidade: paridade total com
a web moderna é um esforço de anos; o alvo realista é um **subset crescente**.

### Fase L1 — Performance, streaming e mais CSS (`1.x`, pós-Etapa 7)

- Parse/layout incremental e streaming render (página grande sem estourar tempo).
- Ampliar o subset de CSS (mais seletores, posicionamento, talvez fl/grid simples).
- Otimizações de memória dentro dos limites declarados.
- **Critério:** páginas estáticas maiores dentro de orçamento; subset de CSS expandido e documentado; sem regressão de determinismo.

### Fase L2 — Engine JavaScript sandboxed (`2.0.0`, Etapa 12)

- Engine JS isolada, com orçamento de tempo/memória e sem acesso a host fora dos callbacks.
- DOM mutável + APIs mínimas; sem rede fora do adapter.
- **Bump major de contrato** + plano de migração Etapa-gated + auditoria cross-repo.
- **Critério:** subset de JS determinístico-onde-possível, sandbox auditado, gates externos da Etapa 12.

### Fase L3 — DOM dinâmico, reflow e fetch via JS (`2.x`)

- Reflow/repaint dirigidos por mutação; `fetch`/`XHR` roteados pelo adapter de rede.
- Eventos básicos de input ligados ao plumbing do CapyOS.
- **Critério:** sites dinâmicos comuns (subset) renderizam de forma estável e sandboxed.

### Fase L4 — Anonimato avançado, mídia e compatibilidade (`2.x`–`3.x`)

- Integração com transporte anônimo do CapyOS (proxy/onion), resistência a fingerprint mais forte.
- Mais formatos de mídia via CapyCodecs (incl. vídeo/áudio quando as Etapas de codec abrirem).
- Trabalho contínuo de compatibilidade com sites reais.
- **Critério:** modo anônimo de transporte ponta-a-ponta; matriz de compatibilidade de sites medida e crescente.

---

## 8. Estratégia de testes e validação (todas as fases)

- **Golden fixtures por superfície:** URL, HTML-to-text, DOM, CSS, layout,
  display-list, download (nome/Content-Disposition), malformado/truncado.
- **Determinismo nos testes:** relógio injetado, seed de RNG fixa, stub de
  codec; nenhum teste depende de rede/clock/PRNG reais.
- **Fail-closed coverage:** cada caminho de rejeição tem fixture (URL inválida,
  redirect perigoso, acima do limite, decode falho, JS presente, path traversal).
- **Gates externos por área tocada** (recomendados, executados fora daqui):

| Área tocada | Gate externo |
|---|---|
| Qualquer mudança | `make validate` |
| Produzir artefatos de pacote | `make package` |
| URL parser/normalização/origem | golden de URL |
| HTML-to-text | golden de HTML-to-text |
| Parser estático/display-list | golden de display-list |
| Tolerância/recuperação | fixtures malformado/truncado |
| Render de imagem | checagem de `depends=` de codec |
| Mudança de layout de fonte | auditoria "sem includes do CapyOS" |
| Etapa 6 aberta | `make smoke-x64-vmware-capybrowse-text` (CapyOS) |
| Etapa 7 aberta | `make smoke-x64-vmware-browser-https-static` + `...-browser-text-fallback` (CapyOS) |

---

## 9. Coordenação cross-repo

- **CapyOS** — dono de rede/TLS, cache/cookies, sandbox, render/janela/input,
  ciclo de vida do app e dos gates de smoke. Consome o core via adapter
  (Etapas 6-7). Toda mudança de contrato exige atualizar a matriz e abrir
  auditoria nova.
- **CapyCodecs** — dono de `capy-codec-image`. CapyBrowser declara
  `depends=org.capyos.codecs.image-basic` quando imagem está habilitada.
- **CapyUI** — dona da desktop session, janela e launcher que expõem o
  navegador como app. CapyBrowser não depende de internals da CapyUI.
- **CapyAgent** — dono do formato de pacote e do **signer Ed25519 ainda
  pendente**. Enquanto não publicado, publicação assinada do CapyBrowser é
  **fail-closed** (só `--unsigned` em laboratório, nunca promovido).

**Mudança de contrato/ABI exige atualizar, no mesmo conjunto de mudanças:**
1. `compatibility.md` (este repo);
2. `../../CapyOS/docs/reference/integration/compatibility-matrix.md`;
3. `../../CapyOS/docs/reference/integration/browser-core-integration-contract.md`;
4. `../../CapyOS/docs/reference/integration/media-codec-integration-contract.md` (só se o acoplamento de codec mudar);
5. um novo `compatibility-audit-<data>.md` no lado do CapyOS.

Nunca empurrar bump breaking sem a auditoria e a linha de matriz no mesmo change.

---

## 10. Versionamento e disciplina de ABI

- **Semver `MAJOR.MINOR.PATCH`.** ABI aditiva dentro da major.
- **MINOR** para nova superfície aditiva (ex.: download, flags de privacidade).
- **MAJOR** reservado para mudança que quebra determinismo/semântica — o
  candidato concreto é **JS (L2 → `2.0.0`)**, com plano de migração Etapa-gated.
- **Proibido em qualquer momento (até a Etapa permitir):** executar JS antes
  da Etapa 12; auto-seguir redirect non-HTTPS; embutir codec local; remover
  erro/warning/nó de display-list já emitido; mudar normalização canônica de
  URL já validada; produzir saída não-determinística; crashar em malformado.

---

## 11. Itens de higiene imediatos (pré-requisito do C0)

Concluídos neste change:

- [feito] `SECURITY.md` — removida a referência fixa de versão ("0.0.3") para evitar drift futuro (agora aponta para `VERSION`).
- [feito] Skill `capybrowser-project-map` — alinhado a `0.0.6` / pin `0.8.0-alpha.261+20260529`.
- [feito] Versão centralizada no `Makefile` — `VERSION_STR := $(shell cat VERSION)`; `version-check` confere `README.md` contra `VERSION`; literal `0.0.5` removido de `lint`/`version-check`.
- [feito] `docs/README.md` referencia este roadmap.
- [feito] `Makefile` alinhado a `compatibility.md`: `make package STAGE=text|core` emite o nome canônico por etapa (`org.capyos.browser.text` na Etapa 6, `org.capyos.browser.core` na Etapa 7; default `core`), com guard fail-closed para `STAGE` inválido.
- [feito] Diretórios de fixtures por superfície (`tests/fixtures/{url,html-to-text,display-list,malformed}/`) com `tests/fixtures/README.md` definindo a convenção golden (`*.in`/`*.base`/`*.out`/`*.warn`, determinística e fail-closed).
- [feito] Harness determinístico header-only: `tests/harness/capy_determinism.h` (PRNG splitmix64 semeada + relógio monotônico injetado) e `tests/harness/capy_test.h` (asserções + comparação golden de bytes), com self-test `tests/test_harness.c` ligado a `make test-harness` / `make validate`.

Pendente (resto do C0 e adiante):

- Fixtures golden reais e testes por superfície: chegam junto com a superfície que os consome (URL em C1, HTML-to-text em C2, display-list em M3).
- Stub de codec determinístico: chega na Fase C3, quando a interface do adapter `capy-codec-image` existir (não inventar antes).

---

## 12. Definição de pronto (DoD) por fase

Uma fase só está "pronta" quando, **cumulativamente**:

1. Código host-testável, sem include/call do CapyOS.
2. Golden fixtures cobrindo caminhos felizes **e** de rejeição (fail-closed).
3. Determinismo verificado (mesma entrada → mesma saída/warnings/veredito).
4. Limites declarados em `compatibility.md` e re-checados contra o pin do CapyOS.
5. Mudança de contrato refletida na matriz/auditoria cross-repo (quando aplicável).
6. `make validate` (e `make package`, quando gera artefato) aprovado externamente.
7. Modo texto preservado como fallback (a partir da fase gráfica).
8. Roadmap e docs atualizados no mesmo change.
