# File Manager v0.6-beta — Settings, busca de texto e seleção

## Intenção

Preparar a próxima versão de teste com melhor legibilidade, consistência visual e feedback de interação, preservando a interface nativa ScePaf do PS Vita e os fluxos atuais do File Manager.

## Objetivos

- Aumentar discretamente o espaço entre os botões de energia e a lista principal de Settings.
- Centralizar a camada colorida sobre a camada sem cor dos ícones em Themes.
- Substituir os toggles pequenos por switches maiores que reproduzem os 17 frames PNG de cada transição, 25 ms por frame; usar widget nativo apenas se a animação por frames não for tecnicamente segura no Vita.
- Mostrar, abaixo de Small, Normal e Large, uma amostra de texto renderizada no tamanho selecionado.
- Exibir Controls como lista vertical rolável, com os assets de `resource/ui/UI Sony PSP` convertidos para o formato suportado pelo projeto e normalizados em uma mesma caixa visual menor.
- No visualizador de texto, usar Triângulo para abrir/fechar pesquisa incremental e Start para abrir o menu `(...)` com Edit line, Insert line, Delete line e Save.
- Manter todo arquivo, pasta ou item marcado com o mesmo realce colorido do cursor mesmo depois que o foco se mover.
- Classificar `.md` exclusivamente como texto e remover qualquer associação com imagem de disco/ISO.
- Atualizar metadados e documentação para `v0.6-beta` / `00.60` e gerar um VPK beta versionado sem sobrescrever pacotes anteriores.

## Comportamento e layout

- A interface continuará usando os templates, dimensões, transições e barras nativas existentes; o mockup é apenas uma referência de intenção.
- Settings continuará em uma coluna. Controls também usará uma coluna com rolagem e o mesmo foco nativo das demais listas.
- O preview de fonte ficará abaixo das três opções e mudará imediatamente quando a seleção mudar.
- O switch permanecerá no lado direito da linha. A animação sempre começa no estado atual e termina no novo estado, sem loop.
- A pesquisa de texto filtra ou localiza linhas sem modificar o conteúdo do arquivo. Fechar a pesquisa restaura a lista completa e mantém uma posição válida.
- O realce persistente de marcação e o realce de foco podem coincidir; uma linha marcada não volta ao estado neutro quando perde o foco.

## Abordagens consideradas

### Switch

1. **Frames PNG fornecidos — escolhida.** Reutiliza os 17 quadros por direção já preparados, oferece o visual solicitado e evita depender de WebP em execução.
2. Spritesheet com recorte por UV. Reduz objetos de textura, mas depende de uma API de recorte menos comprovada neste projeto.
3. Widget nativo. É a alternativa de segurança caso a sequência de frames não seja estável no hardware, mas não reproduz fielmente o recurso fornecido.

### Controls

1. **Lista vertical rolável — escolhida.** Mantém o padrão ScePaf, o espaço dos rótulos e uma navegação previsível.
2. Duas colunas. Mostra mais itens, mas reduz legibilidade e complica foco e rolagem.

## Restrições e não objetivos

- Não reestruturar telas não relacionadas nem trocar o framework ScePaf.
- Não apagar, substituir ou mover VPKs históricos.
- Não criar GitHub Release; o resultado permanece na branch `test`.
- Não usar WebP diretamente no Vita sem suporte já comprovado.
- Não alterar o significado das ações existentes do editor de texto, apenas seu atalho para Start.

## Validação

- Testes de regressão devem cobrir geometria de Settings, centralização de Themes, sequência dos switches, preview de fonte, lista vertical de Controls, atalhos e pesquisa do visualizador de texto, realce persistente e classificação de `.md`.
- A suíte completa deve passar antes da build.
- O VPK deve ser validado como ZIP, ter SHA-256 registrado e preservar os pacotes existentes.
- Aparência, animação e controles ainda exigem validação final no PS Vita físico.
