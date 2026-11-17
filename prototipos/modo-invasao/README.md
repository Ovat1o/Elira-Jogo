# Modo Invasão — protótipo v2

Segundo modo de jogo do ELIRA: ação top-down em salas, com um andar por época.
**É um protótipo standalone. Ainda não está integrado ao jogo principal:** não mexe em
`main.c`, `funcoes.c`, `funcoes.h`, `constantes.h`, no menu nem nos assets do escape room.

## Conteúdo
- 4 estágios (1990, 2008, 2026, 2048), cada um com o chefe da sua era:
  O Coletor (privacidade), O Filtro (falsos positivos), O Algoritmo (viés), O Oráculo (alucinação).
- Núcleo final contra A.R.1.3.L., com uma fase por era.
- 5 corações, 10 melhorias em chip e escolha de 1 entre 2 após cada chefe.
- Painel de narrativa com retrato e fala da Elira ao pegar artefatos e melhorias.

## Compilar e rodar (Windows)
Na raiz do repositório, depois de `scripts\setup-windows.ps1`:

    .deps\w64devkit\bin\make.exe -C prototipos/modo-invasao
    prototipos\modo-invasao\invasao.exe

Controles: WASD move, SETAS atiram, ESC pausa, R reinicia o estágio, 1-5 pulam de estágio (teste).
`invasao.exe --shots <pasta>` gera prévias em PNG e sai.

## Arquivos
- `invasao.c`: código do protótipo (raylib; cenário, inimigos e chefes desenhados com primitivas).
- `elira_sheet.png`, `elira_fx.png`, `elira_rostos.png`: sprite da Elira (4 direções), efeitos e retratos.
  São recursos novos do protótipo. Os assets existentes em `assets/` só são lidos, nunca alterados.

## Em aberto (revisar com o grupo)
- O que acontece ao morrer (hoje volta ao início do estágio).
- Falas da Elira e o artefato de 2048 ("Núcleo de Redundância") ainda precisam de revisão.
- Confirmar se o PDF de requisitos permite sprites PNG novos.
- Integração: branch `feature/modo-invasao`, botão no menu e código com prefixo `Inv*`.
