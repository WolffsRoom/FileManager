# File Manager v0.6-beta — preview, seleção e toque

## Intenção

Corrigir as interações reportadas no hardware e aproximar os previews da linguagem visual nativa do Vita, preservando o navegador visível sob as telas de imagem e música.

## Comportamento aprovado

- Toda a área visual do switch, inclusive o próprio controle, ativa a opção tocada.
- A camada colorida do ícone de Themes coincide com a camada sem cor.
- Células recicladas nunca exibem temporariamente o quadrado de seleção.
- Imagem e Música são overlays sobre o navegador. Settings oferece um slider persistente para a intensidade da transparência desses overlays; ele não altera o brilho físico da tela.
- O player de música segue a composição do app Música oficial: capa à esquerda, metadados, progresso tocável e controles principais à direita. Exportar, Minimizar e Abrir no Música ficam no menu `(...)` inferior direito.
- O seek converte o tempo escolhido para a posição real esperada pelos decodificadores MP3/OGG.
- Ao abrir a busca, a geometria da lista muda antes de ela ser alinhada ao primeiro item.
- A seleção usa uma camada azul fixa própria, independente da animação de foco, e nenhum checkbox é mostrado.
- Arquivos podem exibir uma segunda linha `TIPO · DD/MM/AAAA · HH:MM`, derivada da extensão e do horário de modificação já carregado. Pastas não exibem detalhes. A opção persistente “Mostrar detalhes”, ligada por padrão, controla essa linha.

## Referência investigada

O app Música oficial do Vita foi inspecionado de forma somente leitura pelo FTP em `vs0:/app/NPXS10009/musicbrowser_plugin.rco`. Ele usa `progressbar_touch`, com slider e callbacks de arraste. O File Manager reutilizará o mesmo tipo de widget tanto para o seek quanto para o ajuste de transparência.

## Abordagens

- Escolhida: widgets nativos PAF e camadas visuais independentes. Mantém toque, animações e foco próximos aos apps do sistema.
- Rejeitada: continuar lendo coordenadas globais de toque manualmente. É frágil diante de mudanças de layout e já causou o seek sem resposta.
- Rejeitada: simular transparência apenas escurecendo a imagem. Não preserva de fato a UI anterior e mistura conteúdo com configuração visual.

## Restrições

- O valor de transparência deve permanecer dentro de um intervalo legível e ser salvo no `settings::Config`.
- Vídeo continua usando o preview atual; a transparência solicitada aplica-se às telas de imagem e música.
- A navegação por controles físicos e toque deve continuar funcional.
- Alterações devem ter regressões automatizadas e uma build VitaSDK real; aparência final requer confirmação no console.
