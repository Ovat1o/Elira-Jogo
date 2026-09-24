# A.R.I.3.L. - Demo 1990

Primeira fatia jogavel de **A.R.I.3.L.: Escape Run Temporal**, baseada no documento
"Ideia do jogo - Provisoria".

## O que esta demo inclui

- introducao narrativa com Elira e A.R.1.3.L;
- uma sala point-and-click ambientada em 1990;
- cinco objetos investigaveis e um diario educativo;
- sala detalhada com janela espacial, estrutura da nave, painel tecnico,
  gravador, mobiliario e objetos de epoca;
- terminal com enigma e sistema de tres vidas;
- cronometro da partida;
- recompensa em forma de artefato de memoria (disquete);
- tela de resultado ligada a rota do final otimo;
- tela de derrota e opcoes para reiniciar.
- tela de identificacao para o perfil do jogador, mantendo Elira como protagonista.

## Como jogar

1. Execute `make run` no terminal do projeto ou abra `ARI3L.exe`.
2. Use o mouse para clicar nos objetos da sala.
3. Pressione `D` para abrir ou fechar o diario.
4. No terminal, digite o codigo com o teclado ou com o painel numerico.
5. `Esc` fecha o diario ou volta do terminal para a sala.

## Como compilar

```powershell
make clean
make
make run
```

O Makefile utiliza a instalacao da raylib em `C:/raylib`.
