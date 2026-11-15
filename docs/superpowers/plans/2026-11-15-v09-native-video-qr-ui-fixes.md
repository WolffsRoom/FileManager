# FileManager v0.9 Native Video, QR, and UI Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Corrigir a reprodução MP4 com vídeo e áudio reais no Vita, tornar downloads por QR/GitHub confiáveis e concluir os ajustes de busca, FTP, seleção e configurações solicitados para a v0.9.

**Architecture:** O player passa a usar o módulo RGBA do AvPlayer adotado pelo NetStream e superfícies PAF não proprietárias da memória do decoder. A saída de música ganha pausa/restauração explícita; o download usa libcurl/OpenSSL com staging atômico; o QR move buffers grandes para o heap. Estados visuais e de entrada ficam associados à página ativa, evitando que transições e reciclagem de células reapliquem geometria ou cor incorreta.

**Tech Stack:** C/C++, VitaSDK, ScePaf, SceAvPlayer RGBA, SceAudioOut, libcurl, OpenSSL, quirc, ftpvita, PowerShell regression checks, Docker VitaSDK.

**Spec:** `docs/superpowers/specs/2026-11-15-v09-native-video-qr-ui-fixes-design.md`

## Global Constraints

- Manter versão `00.90` e nome visível File Manager 0.9.
- Preservar handles válidos `0x81xxxxxx`; zero e padrão de erro `0x80xxxxxx` são inválidos.
- O frame pertence ao decoder; a superfície PAF somente referencia a memória e deve ser retirada antes de fechar o AvPlayer.
- MP3 e vídeo não podem possuir saídas ativas ao mesmo tempo; restaurar somente o estado anterior.
- Downloads finais ficam em `ux0:Downloads`, usam `.part`, renomeação atômica, redirecionamentos limitados e limpeza no erro/cancelamento.
- Um toque em Círculo fecha a busca ou navega, nunca ambos.
- Nunca mostrar o IP do cliente FTP na interface.
- Não alterar arquivos locais não relacionados nem substituir VPKs anteriores.

## Review Focus

- Fechar o vídeo antes do primeiro frame ou durante troca de velocidade não pode deixar thread, textura ou porta de áudio viva.
- Firmware que rejeite velocidade 150 deve voltar a 1x e continuar reproduzindo sem seek simulado.
- GitHub pode redirecionar entre domínios e omitir tamanho; o nome final ainda deve ser `WolffsStore.vpk` e o progresso deve permanecer válido.
- QR parcial ou desfocado pode produzir grids inválidos; o scanner deve continuar sem stack overflow, leitura fora do bitmap ou callback após fechamento.
- Pesquisa aberta durante fechamento do teclado não pode alterar a geometria de uma página anterior durante animação 3D.

---

### Task 1: Player RGBA funcional e ciclo de vida seguro

**Files:**
- Modify: `src/video_player.h`
- Modify: `src/video_player.cpp`
- Modify: `src/preview_menu.cpp`
- Modify: `CMakeLists.txt`
- Add: `module/libSceAvPlayerPSVitaRGBA8888.suprx`
- Modify: `tests/v09_video_player.ps1`
- Create: `tests/v09_video_rgba.ps1`

**Interfaces:**
- Produces: `VideoPlayer::State { Closed, Preparing, Playing, Paused, Ended, Failed }`.
- Produces: `static void PlayerEvent(void *, int32_t eventId, int32_t sourceId, void *)` e `void SetStateFromEvent(int32_t)`.
- Produces: `bool Open(const char *)`, `void Close()`, `void Update()`, `Frame()`, `HasFirstFrame()`, `Failed()`, `Duration()`, `CurrentTime()` preservando a interface já consumida por `PreviewMenu`.
- Consumes: `module/libSceAvPlayerPSVitaRGBA8888.suprx` empacotado como `app0:module/libSceAvPlayerPSVitaRGBA8888.suprx`.

- [ ] **Step 1: Escrever `tests/v09_video_rgba.ps1` exigindo módulo empacotado, `autoStart = false`, callback de eventos, espera por READY, `sceAvPlayerStart`, `sceAvPlayerGetVideoDataEx`, `ImageMode_RGBA8888`, pitch em bytes e ausência do wrapper PAF YUV que gerava quadro preto.**

- [ ] **Step 2: Atualizar `tests/v09_video_player.ps1` para o novo contrato RGBA e executar ambos; confirmar RED porque o player atual usa `sceAvPlayerGetVideoData` com `ImageMode_YUV420P2`.**

- [ ] **Step 3: Adicionar o módulo de referência, declarar somente as extensões AvPlayer necessárias e empacotá-lo no VPK.**

- [ ] **Step 4: Implementar inicialização por eventos com handle bitwise válido, habilitar um stream de vídeo e um de áudio quando READY e iniciar explicitamente o player.**

- [ ] **Step 5: Substituir superfícies YUV/CPU fallback por cache de superfícies RGBA não proprietárias, usando `width`, `height`, `pitch * 4` e proporção preservada.**

- [ ] **Step 6: Tornar `CloseDecoder()` idempotente: desanexar textura do plano, sincronizar display, parar áudio, parar/fechar AvPlayer e só então liberar wrappers e callbacks.**

- [ ] **Step 7: Executar os dois testes da task, `tests/video_3d_navigation_20261108.ps1` e `tests/preview_actions.ps1`; confirmar GREEN.**

- [ ] **Step 8: Commitar com `fix: render MP4 through native RGBA player`.**

### Task 2: Áudio exclusivo, velocidade e exportação do vídeo

**Files:**
- Modify: `src/core/music.h`
- Modify: `src/core/music.c`
- Modify: `src/video_player.h`
- Modify: `src/video_player.cpp`
- Modify: `src/preview_menu.h`
- Modify: `src/preview_menu.cpp`
- Modify: `resource/locale/pt-br.xml`
- Modify: `resource/locale/en.xml`
- Modify: `resource/locale/es.xml`
- Modify: `resource/locale/fr.xml`
- Modify: `resource/locale/de.xml`
- Create: `tests/v09_video_audio_options.ps1`

**Interfaces:**
- Produces: `int musicIsOpen(void)`, `int musicIsPaused(void)`, `void musicPause(void)`, `void musicResume(void)`.
- Produces: `int VideoPlayer::SetSpeed(int32_t percent)`, `int32_t Speed() const` com percentuais 100, 150 e 200.
- Consumes: `int mediaExport(const char *path, int isVideo, const MediaExportParam *param)` já existente.
- Produces: opções de vídeo ordenadas `Velocidade: <valor>` e `Exportar`, separadas das opções de imagem.

- [ ] **Step 1: Escrever teste exigindo pausa explícita do MP3 antes de abrir áudio de vídeo, restauração condicional, `sceAvPlayerSetTrickSpeed`, ciclo 100/150/200 e exportação com `isVideo = 1`.**

- [ ] **Step 2: Executar o teste e confirmar RED porque o vídeo atual não controla a música e `OnOptions()` retorna para vídeo.**

- [ ] **Step 3: Implementar estado explícito da música sem substituir o comportamento existente de `musicTogglePause()`.**

- [ ] **Step 4: Registrar o estado anterior no início do vídeo, pausar a música antes da porta AvPlayer e restaurar apenas se ela estava tocando.**

- [ ] **Step 5: Implementar velocidade; em erro para 150, registrar, restaurar 100 e retornar falha não fatal para a interface.**

- [ ] **Step 6: Criar menu de opções do vídeo e ligar Exportar ao job/progresso de mídia existente sem fechar o preview.**

- [ ] **Step 7: Executar `tests/v09_video_audio_options.ps1`, `tests/music_interaction.ps1`, `tests/media_ui_features.ps1` e os testes da Task 1; confirmar GREEN.**

- [ ] **Step 8: Commitar com `feat: add video speed export and audio ownership`.**

### Task 3: QR robusto e download GitHub via libcurl/OpenSSL

**Files:**
- Modify: `src/qr_menu.h`
- Modify: `src/qr_menu.cpp`
- Modify: `src/core/download.h`
- Modify: `src/core/download.c`
- Modify: `src/browser_menu.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/v09_qr_download.ps1`
- Create: `tests/v09_qr_crash_github.ps1`

**Interfaces:**
- Produces: buffers `quirc_code *m_code` e `quirc_data *m_data` alocados no heap durante a vida de `QrMenu`.
- Preserves: `int inspectDownloadUrl(const char *, DownloadInfo *)`.
- Preserves: `int downloadUrlEx(const char *, const char *, DownloadProgress, DownloadCancel)`.
- Consumes: libcurl linked with `curl`, `ssl`, `crypto`, `pthread`, following at most 8 redirects.

- [ ] **Step 1: Escrever teste exigindo buffers quirc no heap, validação de dimensão/grid/payload, parada da câmera antes do callback e download curl com `FOLLOWLOCATION`, `MAXREDIRS`, status HTTP, headers e `.part`.**

- [ ] **Step 2: Atualizar `tests/v09_qr_download.ps1` para não exigir SceHttp e executar ambos; confirmar RED pelas estruturas locais de stack e pelo TLS nativo.**

- [ ] **Step 3: Alocar/desalocar os work buffers no construtor/destrutor de `QrMenu`; rejeitar count/index/size/payload inválidos antes de `quirc_decode`.**

- [ ] **Step 4: Garantir que `Finish()` marque fechamento, pare câmera e retire o callback de frame antes de enfileirar o resultado.**

- [ ] **Step 5: Portar para `download.c` a inicialização curl/OpenSSL comprovada da Wolff's Store, mantendo as assinaturas públicas e parsing seguro de nome/headers.**

- [ ] **Step 6: Implementar callback de escrita/progresso/cancelamento, validar status 2xx, aceitar tamanho desconhecido e remover somente o arquivo parcial no erro.**

- [ ] **Step 7: Executar os testes da task e `tests/navigation_stability.ps1`; confirmar GREEN.**

- [ ] **Step 8: Commitar com `fix: stabilize QR scan and GitHub downloads`.**

### Task 4: Pesquisa, comandos e seleção imediata

**Files:**
- Modify: `src/browser_menu.h`
- Modify: `src/browser_menu.cpp`
- Modify: `resource/file_manager_plugin.xml`
- Modify: `resource/locale/pt-br.xml`
- Modify: `resource/locale/en.xml`
- Modify: `resource/locale/es.xml`
- Modify: `resource/locale/fr.xml`
- Modify: `resource/locale/de.xml`
- Modify: `tests/navigation3d_pfs_music_regressions.ps1`
- Modify: `tests/selection_toolbar_native_20261108.ps1`
- Create: `tests/v09_search_selection_controls.ps1`

**Interfaces:**
- Produces: `bool BrowserMenu::CloseSearch()` que retorna se consumiu a ação.
- Produces: `bool BrowserMenu::AllSelected(const BrowserPage *) const` e `void UpdateSelectionToolbar(BrowserPage *)`.
- Produces: `m_selectionAllButton` persistente para atualizar `Selec. Tudo`/`Desel. Tudo`.
- Changes: Start -> `ToggleSearch()`, Triangle/lower-right -> `OnOptions()`.

- [ ] **Step 1: Escrever teste para Círculo consumido pela busca, geometria por página, mapeamento Start/Triangle, toolbar 960, rótulos curtos e toggle total.**

- [ ] **Step 2: Atualizar testes legados que ainda exigem Start = opções/Triangle = pesquisa e executar; confirmar RED pelo contrato antigo.**

- [ ] **Step 3: Fazer `OnBack()` encerrar busca e retornar antes de agendar `PopPage()`, finalizando edição e filtro numa única função idempotente.**

- [ ] **Step 4: Guardar/restaurar estado de busca por página ou normalizá-lo antes de transições, sem aplicar `SetPos/SetSize` ao ListView em animação.**

- [ ] **Step 5: Trocar os keycodes e callbacks de Start/Triangle preservando botão de canto.**

- [ ] **Step 6: Implementar Select All/Desel. Tudo pelo estado agregado, repintar a célula real recebida pelo callback e todas as células instanciadas, sem colorir o nome.**

- [ ] **Step 7: Ajustar RCO para toolbar de 960 px e atualizar traduções compactas compatíveis com os botões.**

- [ ] **Step 8: Executar testes da task, `tests/navigation_stability.ps1`, `tests/v06_touch_selection_search.ps1` e `tests/followup_selection_3d_search_20261107.ps1`; confirmar GREEN.**

- [ ] **Step 9: Commitar com `fix: stabilize search controls and selection toolbar`.**

### Task 5: Hostname privado no FTP

**Files:**
- Modify: `src/ftp.h`
- Modify: `src/ftp.cpp`
- Modify: `src/ftp_menu.cpp`
- Modify: `resource/locale/pt-br.xml`
- Modify: `resource/locale/en.xml`
- Modify: `resource/locale/es.xml`
- Modify: `resource/locale/fr.xml`
- Modify: `resource/locale/de.xml`
- Create: `tests/v09_ftp_hostname.ps1`

**Interfaces:**
- Produces: lookup reverso limitado com `sceNetResolverCreate()`, `sceNetResolverStartAton()` e `sceNetResolverDestroy()` que grava hostname em `ftp::Status.client` fora do callback de UI.
- Produces: fallback localizado `msg_ftp_client_private` sem endereço numérico.
- Consumes: IP recebido somente internamente do log ftpvita para resolver o nome; ele nunca é copiado para texto visível.

- [ ] **Step 1: Escrever teste exigindo resolução reversa, fallback privado e ausência da cópia direta `s_status.client[n] = ip[n]`.**

- [ ] **Step 2: Executar o teste e confirmar RED porque `OnLog()` copia o IP para o status.**

- [ ] **Step 3: Implementar resolução assíncrona com `sceNetResolverStartAton()`, timeout de 2 segundos, uma tentativa e destruição garantida do resolver, protegendo a atualização do status caso o cliente desconecte durante a consulta.**

- [ ] **Step 4: Mostrar hostname quando disponível e texto genérico quando não; manter endereço do próprio servidor FTP inalterado.**

- [ ] **Step 5: Executar `tests/v09_ftp_hostname.ps1`, `tests/browser_indicators.ps1` e `tests/usb_select.ps1`; confirmar GREEN.**

- [ ] **Step 6: Commitar com `fix: hide FTP client addresses behind hostnames`.**

### Task 6: Settings neutro, duas fontes e texto de opacidade

**Files:**
- Modify: `src/settings.h`
- Modify: `src/settings.cpp`
- Modify: `src/settings_menu.h`
- Modify: `src/settings_menu.cpp`
- Modify: `resource/file_manager_plugin.xml`
- Modify: `resource/locale/pt-br.xml`
- Modify: `resource/locale/en.xml`
- Modify: `resource/locale/es.xml`
- Modify: `resource/locale/fr.xml`
- Modify: `resource/locale/de.xml`
- Create: `tests/v09_settings_font_opacity.ps1`

**Interfaces:**
- Produces: `fontSize` normalizado para 0 (`Pequeno`, 22 px) ou 1 (`Maior`, 28 px).
- Produces: migração de config `2 -> 1`; novo padrão `0`.
- Produces: widgets separados `text_font_preview_lead_page_settings` e `text_font_preview_body_page_settings`.

- [ ] **Step 1: Escrever teste exigindo duas fontes, padrão 0, migração 2->1, ausência de Grande, preview reposicionado/completo, seleção visual escondida e `Opacidade dos menus`.**

- [ ] **Step 2: Executar o teste e confirmar RED porque há três tamanhos, padrão Normal e a seleção do template não é zerada.**

- [ ] **Step 3: Normalizar load/save/índice e alterar `FileFontSize()` para somente 22/28 sem acesso fora do array.**

- [ ] **Step 4: Alterar SettingsMenu para duas linhas, opções mais altas e preview central inferior; usar widget separado para `Lorem Ipsum` enfatizado e corpo solicitado.**

- [ ] **Step 5: Esconder sempre `plane_list_item_selection` em células de Settings criadas/recicladas.**

- [ ] **Step 6: Atualizar rótulo de opacidade e equivalentes dos cinco idiomas, mantendo chave de configuração compatível.**

- [ ] **Step 7: Executar teste da task, `tests/ui_regressions.ps1`, `tests/v06_ui_refinements.ps1` e `tests/media_ui_features.ps1`; confirmar GREEN.**

- [ ] **Step 8: Commitar com `fix: simplify font settings and menu opacity copy`.**

### Task 7: Integração, build e VPK v0.9

**Files:**
- Modify: `tests/v09_release.ps1`
- Modify: `docs/v0.9-hardware-test.md`
- Produce: `scripts/build/vpk/v0.9/FileManager-v0.9-<timestamp>.vpk`

**Interfaces:**
- Consumes: entregas das Tasks 1–6.
- Produces: VPK v0.9 verificável, SHA-256 e roteiro de hardware atualizado.

- [ ] **Step 1: Adicionar os seis novos testes ao agregador `tests/v09_release.ps1`; executar e confirmar todos passam.**

- [ ] **Step 2: Executar todos os `tests/*.ps1`; para teste legado incompatível, atualizar somente quando o requisito aprovado o substituiu e registrar qualquer falha não relacionada.**

- [ ] **Step 3: Executar `git diff --check` e revisar o diff somente dos arquivos desta entrega, preservando untracked existentes.**

- [ ] **Step 4: Compilar com `bash scripts/build.sh Release v0.9`; exigir saída zero e VPK timestampado sem sobrescrever anteriores.**

- [ ] **Step 5: Inspecionar o VPK e confirmar `eboot.bin`, RCO, módulos existentes e `module/libSceAvPlayerPSVitaRGBA8888.suprx`.**

- [ ] **Step 6: Calcular SHA-256 e atualizar `docs/v0.9-hardware-test.md` com MP4 visível/audível, velocidade, exportação, MP3 restaurado, QR fornecido, busca, seleção, FTP e Settings.**

- [ ] **Step 7: Fazer revisão final completa, corrigir achados importantes por RED->GREEN e repetir suíte/build afetados.**

- [ ] **Step 8: Commitar verificação e documentação com `docs: verify v0.9 native media fixes`.**

- [ ] **Step 9: Enviar a branch `test` e entregar caminho, tamanho e SHA-256 do novo VPK.**
