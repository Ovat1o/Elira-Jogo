#ifndef CONSTANTES_H
#define CONSTANTES_H

/* Dimensoes da tela e configuracoes principais */
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define ACCESS_CODE "1950"
#define PLAYER_NAME_MAX 20

/* ========================================================================= */
/* FONTE GERAL DO JOGO                                                       */
/* ========================================================================= */
/* COMO TROCAR A FONTE:
     1. Coloque o arquivo da fonte (.ttf ou .otf) na pasta assets/fonts/
     2. Altere GAME_FONT_FILE abaixo para o nome do arquivo
     3. Recompile (make clean && make run)
   Use GAME_FONT_FILE "" para usar a fonte padrao (pixelada) embutida na raylib.
   Exemplos:
     #define GAME_FONT_FILE "GeistPixel-Square.ttf"   -> Geist Pixel (Square)
     #define GAME_FONT_FILE "MinhaFonte.otf"          -> qualquer outra fonte
     #define GAME_FONT_FILE ""                        -> fonte padrao da raylib
   Cada tamanho e rasterizado sob demanda (cache por tamanho em pixels). Se o
   arquivo nao for encontrado, o jogo volta automaticamente para a fonte padrao. */
#define GAME_FONT_DIR      "assets/fonts/"
#ifndef GAME_FONT_FILE
#define GAME_FONT_FILE     "GeistPixel-Square.ttf"
#endif
#define GAME_FONT_PATH     GAME_FONT_DIR GAME_FONT_FILE
#define GAME_FONT_MAX_SIZE 192

/* Filtro da textura da fonte: 1 = pixel art (POINT, bordas nitidas),
   0 = fonte suave/vetorial (BILINEAR, bordas suavizadas) */
#define GAME_FONT_PIXEL_ART 1

/* Metricas: por padrao a escala e o deslocamento vertical sao calibrados
   AUTOMATICAMENTE lendo a altura da letra 'H' da fonte escolhida, para que ela
   ocupe a mesma caixa da fonte padrao da raylib (preservando todos os layouts).
   Para ajustar manualmente, mude GAME_FONT_MANUAL_METRICS para 1 e edite os
   valores abaixo (ex.: Geist Pixel = 1.3f e 0.19f). */
#define GAME_FONT_MANUAL_METRICS 0
#define GAME_FONT_SCALE          1.3f
#define GAME_FONT_OFFSET_Y       0.19f

/* ========================================================================= */
/* PARAMETROS DA ABERTURA CINEMATOGRAFICA (AJUSTAVEIS)                       */
/* ========================================================================= */
#define CRAWL_SPEED             34.0f   /* Velocidade de subida acelerada para leitura agradavel */
#define CRAWL_PERSPECTIVE_K     1.15f   /* Inclinacao 3D em perspectiva estilo Star Wars */
#define CRAWL_FONT_SIZE_TITLE   64.0f   /* Titulo ELIRA imponente */
#define CRAWL_FONT_SIZE_BODY    30.0f   /* Tamanho base do texto narrativo bem legivel */
#define CRAWL_FONT_SIZE_YEAR    44.0f   /* Ano final '1990' em destaque */
#define CRAWL_LINE_SPACING      44.0f   /* Espacamento vertical entre linhas normais */
#define CRAWL_PARAGRAPH_SPACING 34.0f   /* Espacamento extra entre paragrafos */
#define CRAWL_MIN_SCALE         0.34f   /* Escala minima no horizonte (profundidade Star Wars) */
#define CRAWL_MAX_SCALE         1.15f   /* Escala maxima na base da tela */
#define CRAWL_HORIZON_Y         100.0f  /* Posicao Y do horizonte de fuga */
#define CRAWL_BOTTOM_Y          675.0f  /* Posicao Y da base de entrada */

/* ========================================================================= */
/* CONFRONTO FINAL (2048) - SALA DE DIALOGO ELIRA x ARIEL                    */
/* ========================================================================= */
#define FINAL_TYPE_SPEED 48.0f /* bytes por segundo no efeito maquina de escrever */

/* ========================================================================= */
/* SALA 2026 - LABORATORIO DE SINTESE (VIES ALGORITMICO)                     */
/* ========================================================================= */
#define R2026_MATERIAL_COUNT     4       /* Ferro, Cobalto, Neodimio, Niquel */
#define R2026_CRITERIA_COUNT     5       /* Adequacao, Estabilidade, Disponib., Custo, Historico */
#define R2026_CORRECT_MATERIAL   2       /* indice do Neodimio (unica Terra Rara) */
#define R2026_PADLOCK_CODE       "118"   /* registros de Terras Raras no treinamento */
#define R2026_CODE_LEN           3       /* digitos do cadeado do painel de pesos */
#define R2026_WEIGHT_STEP        5       /* passo (%) dos botoes -/+ do painel */
#define R2026_AUTO_EXEC_SECONDS  180.0f  /* prazo ate Ariel executar a propria recomendacao */
#define R2026_AUTO_EXEC_WARNING  30.0f   /* abaixo disso o HUD entra em alerta */
#define R2026_SCAN_SECONDS       1.2f    /* varredura de uma amostra no analisador */
#define R2026_PROCESS_SECONDS    3.6f    /* sintese final na camara */
/* Pesos (%) na ordem: Adequacao tecnica, Estabilidade, Disponibilidade, Custo, Historico de uso */
#define R2026_WEIGHTS_BIASED     {5, 0, 40, 35, 20}   /* configuracao da diretoria (enviesada) */
#define R2026_WEIGHTS_NORM       {50, 25, 15, 10, 0}  /* norma do comite tecnico (Anexo B) */

/* ========================================================================= */
/* SALA 2048 - TERMINAL DA A.R.1.3.L (prefixo T2046_ mantido no codigo)  */
/* ========================================================================= */
#define T2046_INFLUENCE_MAX          100    /* influencia que da a vitoria a Ariel */
#define T2046_INFLUENCE_HIGH         50     /* sabotagens mais frequentes; abrir protocolo vira sugestao */
#define T2046_INFLUENCE_MID          25     /* muda a cor do medidor */
#define T2046_COST_FOLLOW_WARNING    15     /* seguir aviso de Ariel */
#define T2046_COST_ACCEPT_SUGGESTION 15     /* aceitar "Voce quis dizer...?" */
#define T2046_COST_READ_FAKE_REPORT  10     /* 1a leitura de relatorio_ariel.txt */
#define T2046_COST_FALSE_PRINCIPLE   15     /* anotar principio falso */
#define T2046_COST_ACTIVE_SABOTAGE   5      /* por sabotagem ativa nao descoberta, a cada ciclo */
#define T2046_BONUS_DISCOVERY        10     /* descoberta de sabotagem (reduz a influencia) */
#define T2046_INTERVAL_NORMAL        5      /* acoes entre sabotagens */
#define T2046_INTERVAL_SHORT         3      /* idem, com influencia >= 50 */
#define T2046_PRINCIPLES_TOTAL       3      /* principios corretos a recuperar */
#define T2046_BAR_SECONDS            1.2f   /* duracao da barra de reconstrucao (segundos) */
#define T2046_FINAL_PHRASE           "EU DECIDO"
#define T2046_LOG_MAX                12     /* linhas guardadas no log do terminal */
#define T2046_LOG_LEN                200    /* tamanho maximo de cada linha do log */
#define T2046_INPUT_MAX              15     /* caracteres aceitos no campo de texto */

/* Macro auxiliar para remocao do fundo do retrato (flood fill) */
#define PORTRAIT_PUSH(X, Y) do { int _i = (Y) * w + (X); \
    if (px[_i].a != 0 && IsPortraitBackground(px[_i])) { px[_i].a = 0; stack[sp++] = _i; } } while (0)

/* ========================================================================= */
/* COLECIONAVEIS OCULTOS E ROTA DOURADA                                       */
/* ========================================================================= */
/* Abas do diario guia: uma por sala, RECOMPENSAS e OCULTOS */
#define JOURNAL_TAB_1990    0
#define JOURNAL_TAB_2008    1
#define JOURNAL_TAB_2026    2
#define JOURNAL_TAB_2048    3
#define JOURNAL_TAB_REWARDS 4
#define JOURNAL_TAB_HIDDEN  5
#define JOURNAL_TAB_COUNT   6
#define FINAL_GOLDEN_CHOICE 3  /* indice da 4a opcao (dourada) em cada escolha do confronto */
#define ENDING_NAME_RUIM   "Final Ruim"
#define ENDING_NAME_PADRAO "Final Padrão"
#define ENDING_NAME_OTIMO  "Rota Dourada"

/* ========================================================================= */
/* MODO IA FIGHT (src/iafight.c) - LUTA 1x1 EM VISAO LATERAL                 */
/* ========================================================================= */
#ifndef IAF_ARENAS_DESENHADAS
#define IAF_ARENAS_DESENHADAS 0 /* 1 = arenas desenhadas no codigo (Nucleo e Laboratorio 1990) voltam a tela de arenas */
#endif
#define IAF_ELIRA_ATLAS     "assets/sprites/iafight/elira_luta.png" /* atlas de luta da Elira */
#define IAF_CLAUDE_ATLAS    "assets/sprites/iafight/claude_luta.png" /* atlas de luta do Claude */
#define IAF_CODEX_ATLAS     "assets/sprites/iafight/codex_luta.png"  /* atlas de luta do Codex */
#define IAF_ANTI_ATLAS      "assets/sprites/iafight/antigravity_luta.png" /* atlas de luta do Antigravity */
#define IAF_GROUND_Y        622.0f   /* linha do chao da arena (pes dos lutadores) */
#define IAF_WALL_L          70.0f    /* limite esquerdo da arena */
#define IAF_WALL_R          1210.0f  /* limite direito da arena */
#define IAF_GRAVITY         2700.0f  /* gravidade (px/s^2) */
#define IAF_JUMP_SPEED      1080.0f  /* velocidade inicial do pulo (px/s) */
#define IAF_HP_MAX          1000.0f  /* vida de cada lutador */
#define IAF_METER_MAX       100.0f   /* barra CARGA cheia */
#define IAF_SPECIAL_COST    50.0f    /* CARGA gasta por especial */
#define IAF_AUDIO_DIR       "assets/som/iafight/" /* efeitos, vozes e musicas do modo */
#define IAF_MUSIC_VOLUME    0.5f     /* musica do modo no volume padrao do jogo (0,45) */
#define IAF_ARENA_MUSIC_GAIN 1.4f    /* trilha propria da arena (Posto Vice 2026) um pouco mais alta que a musica_luta */
#define IAF_SELECT_MUSIC_GAIN 0.6f   /* musica das telas de selecao (lutador e arena) mais baixa */
#define IAF_VOL_LVL_MAX      10      /* volume do IA FIGHT ajustavel no menu do modo: niveis 0..10 */
#define IAF_VOL_LVL_PADRAO   5       /* nivel inicial; e o volume de sempre (nivel 10 = o dobro, ate o maximo do audio) */
#define IAF_SFX_VOLUME      0.45f    /* efeitos e vozes no volume padrao do jogo (era 0,8: alto demais no evento) */
#define IAF_ROUND_TIME      60.0f    /* cronometro do round (s); curto para o ritmo de evento */
#define IAF_INTRO_ROUND     0.7f     /* abertura: "ROUND n" fica ate este instante (s) */
#define IAF_INTRO_TOTAL     1.2f     /* abertura: "LUTEM!" ate a luta comecar (s) */
#define IAF_SHOT_STARTUP    0.20f    /* tiro de plasma da Elira: preparo (s) */
#define IAF_SHOT_ACTIVE     0.06f    /* disparo (s) */
#define IAF_SHOT_RECOVERY   0.28f    /* recuperacao (s) */
#define IAF_SHOT_COOLDOWN   0.45f    /* recarga entre tiros (s); so 1 tiro dela na tela */
#define IAF_MAX_PROJ        12       /* projeteis simultaneos */
#define IAF_MAX_PARTS       700      /* particulas/textos de efeito simultaneos */
#define IAF_LUNGE_LIGHT     10.0f    /* avanco do corpo no golpe fraco (px) */
#define IAF_LUNGE_HEAVY     18.0f    /* avanco do corpo no golpe forte (px) */
#define IAF_LUNGE_LOW       14.0f    /* avanco do corpo na rasteira (px) */
#define IAF_HITSTOP_LIGHT   0.05f    /* pausa de impacto: golpe fraco, baixo, aereo e tiro (s) */
#define IAF_HITSTOP_HEAVY   0.09f    /* pausa de impacto: golpe forte (s) */
#define IAF_HITSTOP_SPECIAL 0.14f    /* pausa de impacto: especial (s) */
#define IAF_HIT_SHAKE_PX    3.0f     /* tremor de quem apanha durante a pausa (px) */
#define IAF_ACTIVE_VISUAL   0.10f    /* o quadro de impacto fica pelo menos isto na tela (s) */
#define IAF_SHOT_RECOIL     3.0f     /* recuo da Elira ao disparar o tiro de plasma (px) */
#define IAF_MUZZLE_Y        108.0f   /* altura da boca do canhao da Elira acima dos pes (px) */
#define IAF_MUZZLE_X        125.0f   /* boca do canhao da Elira a frente da ancora (px) */
#define IAF_ELIRA_PIX       2.5f     /* px de tela por pixel da arte da Elira (multiplo de 0,5) */
#define IAF_ELIRA_ACTIVE_VISUAL 0.14f /* golpes da Elira: quadro de impacto fica ao menos isto (s) */
#define IAF_ELIRA_LUNGE_MUL 1.4f     /* a arte mexe pouco o corpo: avanco maior nos golpes dela */
#define IAF_LUNGE_FWDH      34.0f    /* avanco do corpo no soco longo (frente + forte) (px) */
#define IAF_UPPER_CANCEL    0.08f    /* cima e forte com ate este intervalo: o pulo vira uppercut (s) */
#define IAF_UPPER_LAUNCH    820.0f   /* velocidade para cima de quem leva o uppercut (px/s) */
#define IAF_DASH_TAP        0.25f    /* janela do toque duplo para frente que faz o dash (s) */
#define IAF_DASH_TIME       0.20f    /* duracao do dash (s) */
#define IAF_DASH_DIST       180.0f   /* distancia da investida do dash (px) */
#define IAF_ANTI_SLAM_VY    650.0f   /* ground slam do Antigravity: velocidade minima de mergulho (px/s) */
#define IAF_BUFFER          0.15f    /* botao apertado antes da hora fica guardado ate o lutador poder agir (s) */
#define IAF_BUFFER_SPECIAL  0.10f    /* o especial gasta carga: guardado por menos tempo (s) */
#define IAF_CHAIN_WINDOW    0.15f    /* combo fraco > fraco > forte: janela do cancelamento depois do acerto (s) */
#define IAF_CHAIN_SCALE2    0.90f    /* dano do 2o golpe do combo */
#define IAF_CHAIN_SCALE3    0.80f    /* dano do 3o golpe do combo */
#define IAF_CHAIN_STUN_PAD  0.06f    /* combo pela janela: atordoamento minimo = preparo do golpe seguinte + isto (s) */
/* reducao de dano em combo longo (contador de acertos seguidos do HUD): do IAF_COMBO_SCALE_FROM-esimo acerto em
   diante cada golpe tira IAF_COMBO_SCALE_STEP a menos que o anterior, acumulando (4o 85%, 5o 72%, 6o 61%, 7o 52%,
   8o 44%), sem passar do piso IAF_COMBO_SCALE_MIN (9o em diante 40%). Soma com IAF_CHAIN_SCALE2/3. */
#define IAF_COMBO_SCALE_FROM 4
#define IAF_COMBO_SCALE_STEP 0.15f
#define IAF_COMBO_SCALE_MIN  0.40f
#define IAF_GRAB_TOGETHER   0.05f    /* fraco e forte com ate este intervalo contam como juntos (agarrao) (s) */
#define IAF_GRAB_RANGE      30.0f    /* alcance do agarrao alem dos corpos encostados (px) */
#define IAF_GRAB_HOLD       0.25f    /* segura o oponente; nesse tempo ele escapa com fraco + forte (s) */
#define IAF_GRAB_RECOVER    0.35f    /* arremesso e volta a guarda (s) */
#define IAF_GRAB_DMG        80.0f    /* dano do arremesso */
#define IAF_GRAB_PUSH       520.0f   /* velocidade do arremesso para tras (px/s) */
#define IAF_GRAB_VY         600.0f   /* velocidade para cima do arremesso (px/s) */

#endif /* CONSTANTES_H */
