# Distribuição e releases

O projeto usa GitHub Actions para compilar a raylib estaticamente e gerar
instaladores que incluem o executável e todos os recursos do jogo:

- `ELIRA-Windows-x64-Setup.exe`;
- `ELIRA-Linux-x64.deb`;
- `ELIRA-macOS-Apple-Silicon.pkg`;
- `ELIRA-macOS-Intel.pkg`.

A pipeline verifica a presença da fonte `GeistPixel-Square.ttf`, do retrato da
Elira e dos sprites de disquete, CD e SSD antes de publicar. Se algum desses
arquivos não entrar no instalador, o build falha.

No Windows, o instalador cria a pasta do jogo e um atalho na área de trabalho.
No macOS, o PKG instala `ELIRA.app` em Aplicativos, com os recursos dentro do
bundle. No Linux, o pacote DEB instala o jogo, registra uma entrada no menu de
aplicativos e declara as bibliotecas de sistema necessárias para Ubuntu/Debian
64 bits.

## Quando a automação roda

- Pull request para `main`: valida a compilação nas quatro plataformas.
- Push em `main`: valida e disponibiliza os instaladores temporariamente na execução.
- Tag iniciada por `v`: além de compilar, cria um GitHub Release público com os
  quatro instaladores, notas automáticas e `SHA256SUMS.txt`.
- `workflow_dispatch`: permite testar manualmente os builds pela aba Actions.

## Como publicar uma versão

Garanta que as mudanças já estejam na branch `main` e que a execução de CI tenha
passado. Depois crie e envie uma tag semântica:

```bash
git checkout main
git pull --ff-only
git tag -a v1.0.1 -m "ELIRA v1.0.1"
git push origin v1.0.1
```

Acompanhe a execução **Build e Release** na aba Actions. Quando os quatro builds
terminarem, a versão aparecerá em **Releases**. Se a Release da tag já existir,
uma nova execução substitui os instaladores pelos arquivos recém-gerados.

Não é necessário cadastrar token ou segredo: o job de publicação solicita apenas
`contents: write` para o `GITHUB_TOKEN` temporário da própria execução.

## Teste local com CMake

Se a raylib 6.0 estiver instalada no computador:

```bash
cmake -S . -B build -DELIRA_USE_SYSTEM_RAYLIB=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cpack --config build/CPackConfig.cmake -C Release -B dist
```

Sem `ELIRA_USE_SYSTEM_RAYLIB=ON`, o CMake baixa e compila a raylib 6.0, da mesma
forma utilizada pela pipeline.

## Limitações conhecidas

- Os aplicativos de macOS ainda não são assinados nem notarizados. Para remover o
  aviso do Gatekeeper será necessário um certificado Apple Developer e segredos
  de assinatura configurados no repositório.
- O instalador Linux usa o formato DEB e tem como base Ubuntu 22.04 x64. Ele é
  destinado a Ubuntu, Debian e distribuições compatíveis com pacotes DEB.
