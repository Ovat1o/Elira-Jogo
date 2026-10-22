# Distribuição e releases

O projeto usa GitHub Actions para compilar a raylib estaticamente e gerar quatro
executáveis nativos:

- `ELIRA-Windows-x64.exe`;
- `ELIRA-Linux-x64`;
- `ELIRA-macOS-Apple-Silicon`;
- `ELIRA-macOS-Intel`.

Também são gerados quatro pacotes completos prontos para uso:

- `ELIRA-Windows-x64.zip`;
- `ELIRA-Linux-x64.tar.gz`;
- `ELIRA-macOS-Apple-Silicon.tar.gz`;
- `ELIRA-macOS-Intel.tar.gz`.

Cada pacote contém o executável, a pasta `assets/`, instruções de uso e, no
macOS/Linux, um lançador. O jogador não precisa instalar compilador, Make,
Homebrew ou raylib. Os binários avulsos são úteis para testes, mas o jogo ainda
depende da pasta `assets/`; portanto, os pacotes completos são a distribuição
recomendada para usuários finais.

## Quando a automação roda

- Pull request para `main`: valida a compilação nas quatro plataformas.
- Push em `main`: valida e disponibiliza os pacotes temporariamente na execução.
- Tag iniciada por `v`: além de compilar, cria um GitHub Release público com os
  quatro executáveis, os quatro pacotes, notas automáticas e
  `SHA256SUMS.txt`.
- `workflow_dispatch`: permite testar manualmente os builds pela aba Actions.

## Como publicar uma versão

Garanta que as mudanças já estejam na branch `main` e que a execução de CI tenha
passado. Depois crie e envie uma tag semântica:

```bash
git checkout main
git pull --ff-only
git tag -a v1.0.0 -m "ELIRA v1.0.0"
git push origin v1.0.0
```

Acompanhe a execução **Build e Release** na aba Actions. Quando os quatro builds
terminarem, a versão aparecerá em **Releases**.

Não é necessário cadastrar token ou segredo: o job de publicação solicita apenas
`contents: write` para o `GITHUB_TOKEN` temporário da própria execução.

## Teste local com CMake

Se a raylib 6.0 estiver instalada no computador:

```bash
cmake -S . -B build -DELIRA_USE_SYSTEM_RAYLIB=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix dist/ELIRA-local
```

Sem `ELIRA_USE_SYSTEM_RAYLIB=ON`, o CMake baixa e compila a raylib 6.0, da mesma
forma utilizada pela pipeline.

## Limitações conhecidas

- Os pacotes de macOS ainda não são assinados nem notarizados. Para remover o
  aviso do Gatekeeper será necessário um certificado Apple Developer e segredos
  de assinatura configurados no repositório.
- O pacote Linux usa como base Ubuntu 22.04 x64 e pressupõe um ambiente desktop
  com drivers gráficos e bibliotecas de sistema usuais. Nenhum pacote de
  desenvolvimento é exigido do jogador.
