# v0.6-beta Preview, Selection and Touch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Corrigir toque, reciclagem visual, seleção, busca e previews, acrescentando transparência configurável e um player de música mais nativo.

**Architecture:** O template de linha ganha camadas independentes para seleção e switch; estados de foco deixam de controlar a marca persistente. Imagem e música tornam-se páginas overlay cuja opacidade vem de `settings::Config`. Seek e transparência usam `ProgressBarTouch`, seguindo o widget encontrado no RCO oficial do app Música.

**Tech Stack:** C++/C para VitaSDK, ScePaf, XML RCO, PowerShell regressions, Docker VitaSDK.

**Spec:** `docs/superpowers/specs/2026-11-09-v06-preview-selection-touch-design.md`

## Global Constraints

- Continuar na branch `test` e preservar todos os arquivos não rastreados do usuário.
- Não alterar o brilho físico do Vita.
- Vídeo permanece fora do ajuste de transparência.
- Cada alteração funcional começa com teste falhando e termina com build VitaSDK real.

## Review Focus

- Células recém-recicladas não podem piscar checkbox antes do preenchimento.
- Foco físico, toque e marca persistente não podem sobrescrever o estado visual uns dos outros.
- Opacidade mínima ainda deve manter metadados e controles legíveis.
- Seek deve funcionar para MP3 e OGG, inclusive nos extremos 0% e 100%.
- Menu `(...)` não pode destruir ou minimizar o player dentro do callback PAF em andamento.

---

### Task 1: Linhas, switch, ícone de tema e busca

**Files:**
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/settings_menu.cpp`
- Modify: `src/browser_menu.cpp`
- Modify: `src/browser_menu.h`
- Test: `tests/v06_touch_selection_search.ps1`

**Interfaces:**
- Produces: `BrowserMenu::ApplyRowHighlight(...)` controla uma camada dedicada; o template inicia o switch transparente.

- [ ] Escrever regressões para área tocável do switch, alinhamento exato do preview de tema, textura inicial transparente e realinhamento da busca após a mudança de geometria.
- [ ] Rodar o teste e confirmar falha pelos comportamentos ausentes.
- [ ] Ampliar apenas a área tocável das linhas de Settings e centralizar a camada de tema pelas dimensões reais do ícone.
- [ ] Fazer o template nascer sem checkbox visível e adicionar uma camada azul fixa atrás do botão da linha.
- [ ] Atualizar criação, marcação, desmarcação, Select All e reciclagem para mostrar/ocultar a camada persistente.
- [ ] Reordenar abertura da busca para redimensionar primeiro e focar o primeiro item depois.
- [ ] Rodar regressões relacionadas e fazer commit.

### Task 2: Transparência e detalhes configuráveis

**Files:**
- Modify: `src/settings.h`
- Modify: `src/settings.cpp`
- Modify: `src/settings_menu.h`
- Modify: `src/settings_menu.cpp`
- Modify: `src/preview_menu.cpp`
- Modify: `src/music_menu.cpp`
- Modify: `src/browser_menu.cpp`
- Modify: `resource/file_manager_plugin.xml`
- Modify: `resource/locale/{pt,en,es,fr,de}.xml`
- Test: `tests/v06_preview_transparency.ps1`

**Interfaces:**
- Produces: `Config::previewOpacity`, persistido como percentual inteiro seguro; `settings::PreviewOpacity()` retorna alpha normalizado; `Config::showDetails` controla a segunda linha dos arquivos.

- [ ] Escrever regressões de default, limites, persistência, slider nativo e aplicação somente a imagem/música.
- [ ] Confirmar RED.
- [ ] Adicionar configuração com default legível e clamp na carga.
- [ ] Adicionar a entrada Transparência em Settings e uma página com `ProgressBarTouch`, rótulo percentual e callbacks de mudança.
- [ ] Adicionar “Mostrar detalhes” como switch persistente, ligado por padrão.
- [ ] Formatar arquivos como `TIPO · DD/MM/AAAA · HH:MM`, sem detalhes em pastas, respeitando `showDetails`.
- [ ] Trocar fundos opacos de imagem e música por camadas overlay que recebem `PreviewOpacity()`.
- [ ] Confirmar GREEN e regressões de Settings/preview; fazer commit.

### Task 3: Player de música nativo, opções e seek

**Files:**
- Modify: `src/core/music.h`
- Modify: `src/core/music.c`
- Modify: `src/music_menu.h`
- Modify: `src/music_menu.cpp`
- Modify: `resource/file_manager_plugin.xml`
- Modify: `src/resource.h`
- Test: `tests/music_interaction.ps1`
- Test: `tests/v06_music_native_player.ps1`

**Interfaces:**
- Produces: `musicSeek(double seconds)` converte tempo para offset bruto; `MusicMenu::OnOptions()` expõe Exportar, Minimizar e Abrir no Música.

- [ ] Criar testes para conversão tempo/offset, `ProgressBarTouch` e menu de três ações; confirmar RED.
- [ ] Corrigir `musicSeek` usando duração e tamanho real do arquivo, com clamp dos extremos.
- [ ] Substituir leitura manual de coordenadas pelo callback do slider nativo e manter atualização de tempo durante arraste.
- [ ] Reorganizar capa, metadados, progresso e controles segundo a referência oficial já inspecionada.
- [ ] Remover os três botões de ação do corpo e ligá-los ao corner button `(...)`, deferindo ações destrutivas.
- [ ] Rodar testes musicais e fazer commit.

### Task 4: Verificação e pacote beta

**Files:**
- Modify: `docs/v0.6-beta-settings-text-search-selection.md`
- Create: `scripts/build/vpk/v0.6-beta/FileManager-v0.6-beta-<timestamp>.vpk`

- [ ] Rodar cada teste PowerShell isoladamente e exigir suíte completa verde.
- [ ] Compilar `Release` com VitaSDK Docker e arquivar sem substituir VPKs anteriores.
- [ ] Validar ZIP, `param.sfo`, tamanho e SHA-256.
- [ ] Revisar o diff completo, registrar limitações de teste físico e fazer commit final.
- [ ] Enviar `test` e entregar o caminho do VPK.
