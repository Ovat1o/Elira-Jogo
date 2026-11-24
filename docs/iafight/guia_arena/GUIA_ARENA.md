# Guia para desenhar uma ARENA do IA FIGHT (ELIRA)

Este guia é para quem vai desenhar um **cenário de luta** (fundo + chão) para o modo **IA FIGHT** do ELIRA, que vai ao **Tech Night da CESAR School em 24/11**.
As medidas abaixo vêm do código do jogo. Se você seguir o guia, a arte entra no jogo sem precisar de ajuste.

Arquivos desta pasta:

| arquivo | para que serve |
|---|---|
| `template_arena_pixel_320x180.png` | **gabarito para desenhar por cima**, no tamanho real da pixel art |
| `template_arena_pixel_320x180_ampliado.png` | o mesmo gabarito ampliado 4x, só para enxergar melhor |
| `template_arena_1280x720.png` | gabarito do tamanho da tela, com todas as medidas escritas |

Arenas de exemplo que já estão no jogo: **Arena do Núcleo** (sala de servidores) e **Laboratório 1990** (fitas magnéticas e terminais verdes).

---

## 1. Tamanho, formato e escala

- **Desenhe em 320 × 180 pixels.** Essa é a resolução da pixel art.
- O jogo amplia **4x** sem suavizar, com cada pixel virando um quadrado de 4×4. O resultado é **1280 × 720**, a tela do jogo.
- É a mesma escala dos lutadores: o Claude, por exemplo, usa 4 px de tela por pixel de arte.
- **Formato:** PNG de 32 bits (RGBA), sem perfil de cor estranho (sRGB normal).
- **Transparência:** só nas camadas que ficam por cima de outras: meio, plateia, chão e frente. A camada de fundo é **opaca**.
- **Exporte em 100 %**, ou seja, 320×180. Não amplie nem reduza ao exportar, porque o jogo já amplia.
- **Programas:** Aseprite, LibreSprite (grátis), Piskel (grátis, no navegador) ou Pixelorama (grátis).
- Desligue antisserrilhado (anti-aliasing) e pincéis com borda suave. Use **lápis de 1 px**.

> Toda medida neste guia aparece de duas formas: **na arte (320×180)** e **na tela (1280×720)**.
> Para converter: tela = arte × 4.

---

## 2. Medidas principais

Comece com o gabarito `template_arena_pixel_320x180.png` como camada de baixo. Desenhe por cima e esconda o gabarito na hora de exportar.

### Linha do chão (a mais importante)

| o quê | na arte (320×180) | na tela (1280×720) |
|---|---|---|
| **pés dos lutadores (linha do chão)** | **linha 155** | **y = 622** |
| começo do piso (onde a parede encontra o chão) | linha 148 | y = 592 |
| topo da mureta da plateia | linha 139 | y = 556 |
| plateia (cabeças e corpos) | linhas ~107 a 139 | y ≈ 430 a 556 |

- Os lutadores pisam na **linha 155**. O piso que você desenha começa na **linha 148** e vai até o fim da imagem (linha 179).
- Desenhe em perspectiva: a parte de cima do piso, das linhas 148 a 155, fica "lá no fundo" do ringue.
- Não desenhe nada que pareça um degrau ou buraco na linha 155. Os pés precisam parecer apoiados num chão plano.

### Lutadores

| o quê | na arte | na tela |
|---|---|---|
| altura em pé | **51 a 55 px** | **204 a 218 px** (Elira 217, Claude 204) |
| altura agachado | ~32 a 48 px | ~128 a 193 px |
| largura | 18 a 60 px (o Claude é largo: 60) | 70 a 240 px |
| topo da cabeça em pé | linha ~101 | y ≈ 404 |
| topo da cabeça no **pulo** | linha ~47 | y ≈ 188 |
| posição de início do round | colunas 105 e 215 | x = 420 e x = 860 |

- **Paredes:** o centro do lutador anda entre a **coluna 17** e a **coluna 302** (na tela, x = 70 a 1210).
- Encostado na parede, metade do corpo chega até a borda da imagem. Por isso **a arena inteira, de borda a borda, fica atrás dos lutadores**.
- Não existe câmera que anda para os lados: a tela é fixa e mostra a arena inteira o tempo todo.

### Zonas que o jogo cobre (HUD) e o telão

O jogo desenha estas coisas **por cima** da sua arte. Nessas áreas, deixe só fundo calmo (parede, teto, céu) e não ponha nada importante.

| zona | na arte (x / linhas) | na tela |
|---|---|---|
| barras de vida | colunas 16–140 e 179–303, linhas 5–15 | x 66–562 e 718–1214, y 20–60 |
| nomes dos lutadores | colunas 17–85 e 235–302, linhas 14–23 | y 58–92 |
| cronômetro (hexágono no centro) | colunas 147–172, linhas 1–25 | x 590–690, y 4–100 |
| rounds vencidos | colunas 125–145 e 174–195, linhas 14–23 | ao lado do cronômetro |
| contador de acertos | colunas 17–62 e 257–302, linhas 25–36 | y 100–146 |
| **telão "RINHA DE INTELIGÊNCIAS"** | **colunas 103–216, linhas 25–41** (cabos a partir da linha 11) | **x 414–866, y 100–166** |
| "ESPECIAL PRONTO" | colunas 17–75 e 245–302, linhas 154–163 | y 616–652 |
| barras de CARGA | colunas 17–93 e 227–302, linhas 164–174 | y 656–698 |

- O jogo escurece um pouco a faixa de cima (linhas 0–30) e a de baixo (linhas 155–179) para o placar ficar legível. Isso não é filtro: é só a placa atrás do placar.
- O **telão** é desenhado pelo jogo, com texto e sinal ao vivo. Na sua arte, você pode desenhar o teto ou a estrutura de onde ele "pendura". Não desenhe o telão em si.

### Margem de tremor

Quando sai um golpe forte, a tela treme até **12 px** (3 px de arte) para cada lado.
**Continue o desenho até a borda** da imagem em todas as camadas opacas, sem moldura e sem borda preta. O programador cuida do resto.

---

## 3. Camadas e o que vai em cada uma

Todas as camadas têm o **mesmo tamanho (320×180)** e ficam alinhadas no canto de cima à esquerda (0,0). Assim ninguém precisa posicionar nada: é só empilhar.

| ordem | arquivo | transparência | o que vai nela |
|---|---|---|---|
| 1 (atrás) | `arena_<nome>_fundo.png` | **opaco** | céu, parede do fundo, teto, coisas bem distantes. Cobre a imagem inteira. |
| 2 | `arena_<nome>_meio.png` | transparente | estruturas no meio do caminho: máquinas, estantes, colunas, janelas, e os **objetos grandes nas laterais** (até a linha ~160). |
| 3 | `arena_<nome>_plateia.png` | transparente | o público e a mureta na frente dele (linhas ~100 a 148). |
| 4 | `arena_<nome>_chao.png` | transparente em cima, **opaco das linhas 148 a 179** | o piso do ringue. |
| — | *(o jogo desenha os lutadores aqui)* | | |
| 5 (opcional) | `arena_<nome>_frente.png` | transparente | coisa que fica **na frente** dos lutadores: só nos cantos, nas colunas 0–15 e 304–319, e/ou nas linhas 170–179 perto das bordas. |

`<nome>` é o nome da arena em minúsculas, sem acento e sem espaço. Exemplos: `arena_biblioteca_fundo.png`, `arena_metro2048_chao.png`.

### Plateia (bônus opcional)

Nas arenas atuais, a plateia pula e levanta os braços quando sai um golpe forte. Para isso funcionar com a sua arte, entregue também `arena_<nome>_plateia_2.png`: a mesma plateia, com as **mesmas posições**, mas empolgada (braços para cima, corpo 1–3 px mais alto). O jogo alterna entre as duas.

### Piso

- O jogo desenha um **reflexo fraco dos lutadores** no piso, das linhas 155 a 179.
- Por isso, o piso deve ser **liso e escuro a médio**, com poucos detalhes fortes. Linhas de perspectiva suaves e ladrilhos discretos ficam ótimos. Grafite, tapete muito estampado ou desenho chamativo no meio não.

### Animação (opcional)

Luzes que piscam, telas e ventiladores podem ir em um arquivo separado, `arena_<nome>_anim_<coisa>.png`, com os quadros lado a lado, do mesmo tamanho. Anote no `LEIAME.txt` a posição (x, y na arte) e quantos quadros por segundo.
Se não der tempo, **não faça**: arena parada já serve.

---

## 4. Legibilidade: os lutadores precisam aparecer

O mais importante: **o jogador tem que enxergar os lutadores o tempo todo**.

Cores dos lutadores (evite essas cores em áreas grandes atrás deles):

| lutador | cores do corpo |
|---|---|
| **Elira** | cabelo **roxo**, detalhes **laranja**, armadura **branca** |
| **Claude** | **laranja/terracota** |
| **Codex** | **preto/grafite** com detalhes **verde neon** |
| **Antigravity** | **violeta/lilás** |
| HUD do jogo | linhas **ciano** |

Regras:

1. **Zona dos torsos (linhas 112 a 145; na tela, y 450 a 580):** fundo **calmo**, sem detalhe forte, sem listras e sem luz forte piscando. É onde ficam corpo, braços e golpes.
2. **Nem preto nem branco puro atrás dos lutadores.** O Codex é escuro e some no preto, e o branco "estoura" a Elira. Use tons médios-escuros, mais ou menos entre `#1A2238` e `#4A5070` (azul-acinzentado) ou entre `#2C2630` e `#5A4A40` (marrom-acinzentado).
3. **Contraste de valor:** o fundo atrás dos lutadores deve ser **um pouco mais claro que o Codex** e **mais escuro que a Elira**. Uma faixa de luz suave atrás dos corpos, como a que o Núcleo usa, ajuda muito.
4. **Cores fortes e saturadas só em detalhes pequenos**, como lâmpadas, LEDs e telinhas. Nada de parede roxa, laranja ou verde-neon atrás do ringue.
5. **Paleta:** até **32 cores** por arena, com uma cor dominante. Fica mais coeso com o resto do jogo.
6. **Nada de texto ou logo de terceiros:** nada de marca de empresa, logo de produto, personagem de outro jogo ou foto. Texto em geral, evite, porque o jogo já escreve o dele. Se quiser uma placa, use algo curto em português, inventado, e longe da zona dos torsos. Logo da CESAR ou do Tech Night só com autorização do Victor.
7. **SEM filtros de tela:** nada de scanline, vinheta (bordas escuras), efeito de pixelização por cima, aberração cromática ou color grading/LUT. O Victor recusa a arte se tiver qualquer um deles. Luz e sombra **desenhadas** no cenário pode e é bem-vinda.
8. **Combine com o tema:** o ELIRA é sobre IA, dados e viagem no tempo (1990, 2008, 2026, 2048, o Núcleo). Arenas que lembram uma dessas épocas encaixam melhor.

---

## 5. Checklist de entrega

Antes de mandar, confira cada item:

- [ ] `arena_<nome>_fundo.png`: 320×180, opaco
- [ ] `arena_<nome>_meio.png`: 320×180, fundo transparente
- [ ] `arena_<nome>_plateia.png`: 320×180, fundo transparente (e `_plateia_2.png`, se fez)
- [ ] `arena_<nome>_chao.png`: 320×180, opaco das linhas 148 a 179
- [ ] `arena_<nome>_frente.png`: só se fez (opcional)
- [ ] `arena_<nome>_previa.png`: **todas as camadas juntas, ampliadas 4x (1280×720)**, sem suavizar ("vizinho mais próximo" / nearest neighbor), **com o gabarito por cima a 50 %** para conferir as medidas
- [ ] `LEIAME.txt`: nome da arena, seu nome, época ou tema em uma frase e, se tiver animação, a posição e a velocidade
- [ ] Pés dos lutadores na **linha 155**; piso começa na **linha 148**
- [ ] Zona dos torsos (linhas 112–145) calma; nem preto nem branco puro atrás dos lutadores
- [ ] Nada importante embaixo do HUD ou do telão
- [ ] Desenho vai até a borda em todas as camadas opacas
- [ ] Sem antisserrilhado, sem filtro de tela, sem texto ou logo de terceiros
- [ ] Exportado em 100 % (320×180), PNG

Mande tudo numa pasta `arena_<nome>/`, zipada se preferir.

---

## 6. Mensagem pronta para mandar ao grupo

> Oi, pessoal! Vamos levar o modo de luta **IA FIGHT** do ELIRA para o **Tech Night da CESAR School em 24/11** e eu queria arenas novas desenhadas por vocês 🙌
>
> Cada arena é um cenário de luta em **pixel art de 320×180** (o jogo amplia 4x para 1280×720). Mandei em anexo o **GUIA_ARENA.md** com todas as medidas, além do **gabarito** `template_arena_pixel_320x180.png`: é só desenhar por cima dele.
>
> O essencial:
> • os lutadores pisam na **linha 155** e têm uns **52 px de altura**;
> • atrás dos corpos (linhas 112–145) o fundo tem que ser **calmo** e nem muito escuro nem muito claro (o Codex é preto e a Elira é roxa);
> • em cima tem a barra de vida, o cronômetro e o telão, e embaixo a barra de carga: lá vai só fundo simples;
> • separem em camadas: **fundo, meio, plateia e chão** (os nomes dos arquivos estão no guia);
> • **sem filtros** (scanline, vinheta, pixelização, color grading) e **sem logo ou texto de terceiros**.
>
> Tema livre, mas o que combinar com IA, dados e viagem no tempo (1990, 2008, 2026, 2048) encaixa melhor. Me mandem até **<DIA/HORA>** a pasta `arena_<nome>/` com as camadas, uma prévia ampliada e um LEIAME. Qualquer dúvida, me chamem. Valeu! 💜

---

## 7. Para quem vai programar (nota técnica, não precisa ler para desenhar)

Fonte das medidas: worktree `wt-modocombate`, branch `modocombate`.

- `IAF_GROUND_Y 622`, `IAF_WALL_L 70`, `IAF_WALL_R 1210`, `SCREEN_WIDTH/HEIGHT 1280×720` (`constantes.h`).
- `AR_FLOOR_Y = 592`, `AR_CROWD_RAIL_Y = 556` e telão `{420,106,440,54}` com moldura de 6 px e cabos a partir de y 46 (`iafight_arena.h`).
- Início do round em x 420/860 (`StartRound`).
- Pulo: 1080²/(2·2700) = 216 px de subida.
- `Hurt()`: Elira 70×196, Codex 84×200, Claude 232×204.
- Quadros IDLE: Elira 218 px de altura em escala 1, Claude 102 px × `CL_SCALE` 2 = 204 px.

Como uma arena entra no jogo (já implementado):
- A tela **ESCOLHA A ARENA** lê a tabela `ARENAS` em `src/iafight_arena.h`. Cada linha é um cartão.
- Para destravar uma arena: copie as camadas para `assets/sprites/iafight/` com os nomes deste guia e troque uma linha `AR_EM_BREVE` da tabela por `{"NOME NA TELA", "legenda curta", AR_CAMADAS("<nome>"), AR_PNG},`. Se quiser, ponha a camada de fundo na lista do `--verify-assets`, em `src/main.c`.
- Camada que falta é pulada (`meio`, `plateia_2` e `frente` são opcionais). Sem a camada de **fundo** no disco, o cartão continua travado (EM BREVE).
- O jogo desenha `fundo → meio → plateia (ou plateia_2 nos golpes fortes) → telão → chão`, depois os lutadores e por fim `frente`. Camadas de 320×180 são ampliadas 4x sem suavizar. Uma imagem única de 1280×720, como a Posto Vice 2026, também serve: `AR_IMAGEM("arquivo.png")`.
- `fundo` e `chao` são desenhados também um pouco maiores por baixo, para o tremor da câmera não mostrar a borda.
- O telão mostra `AO VIVO // <NOME NA TELA>`. O HUD continua desenhado pelo código.
- Linha 155 = y 620–623 na tela, porque 622 não é múltiplo de 4. A diferença de 2 px não aparece.
