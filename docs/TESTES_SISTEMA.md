# Relatório de Testes de Sistema e Qualidade (QA / FDS) — Projeto ELIRA

- **Responsável pela Execução:** Beatriz de Melo Diniz (`@BiaD-Cam`)
- **Repositório:** [Ovat1o/Elira-Jogo](https://github.com/Ovat1o/Elira-Jogo)
- **Data da Execução:** 26/11/2026
- **Total de Histórias Testadas:** 16 UHs

## Casos de Teste Executados por História de Usuário (Jira)

| ID Teste | Card Jira | História de Usuário | Módulo (`src/`) | Cenário Executado | Resultado | Evidência (Screencast) |
| :--- | :--- | :--- | :--- | :--- | :---: | :--- |
| **CT-01** | **PI2-64** | UH01: Menu Principal e Navegação | `src/main.c` | Navegação entre os 3 modos (*Iniciar Missão*, *Modo Invasão* e *IA Fight*) sem travamentos. | Aprovado | Anexado no Card Jira |
| **CT-02** | **PI2-67** | UH03: Diário de Viagem e Pistas | `src/funcoes.c` | Abertura do diário durante a campanha e exibição correta das pistas registradas. | Aprovado | Anexado no Card Jira |
| **CT-03** | **PI2-69** | UH05: Tutorial e Regras de Jogo | `src/funcoes.c` | Exibição das instruções iniciais e comandos antes da primeira estação temporal. | Aprovado | Anexado no Card Jira |
| **CT-04** | **PI2-71** | UH06: Introdução Narrativa da Missão | `src/funcoes.c` | Carregamento da cena introdutória de 2048 e contextualização da missão de Elira. | Aprovado | Anexado no Card Jira |
| **CT-05** | **PI2-72** | (EXTRA) UH07: Navegação entre Estações Temporais | `src/funcoes.c` | Transição sequencial entre as eras (1990, 2008, 2026 e 2048) após concluir cada desafio. | Aprovado | Anexado no Card Jira |
| **CT-06** | **PI2-73** | (EXTRA) UH08: Ganho e Perda de Pontos Éticos | `src/funcoes.c` | Atualização dinâmica da pontuação ética conforme acertos e erros nos diálogos. | Aprovado | Anexado no Card Jira |
| **CT-07** | **PI2-74** | UH09: Desafio de Viés Algorítmico | `src/funcoes.c` | Validação das alternativas lógicas no puzzle de viés algorítmico. | Aprovado | Anexado no Card Jira |
| **CT-08** | **PI2-75** | UH10: Desafio de Alucinação | `src/funcoes.c` | Validação das escolhas lógicas no desafio de alucinação (erro vs. acerto). | Aprovado | Anexado no Card Jira |
| **CT-09** | **PI2-76** | UH11: Sistema de Vidas do Jogador | `src/funcoes.c` | Redução da barra de 5 vidas ao errar respostas e acionamento do *Game Over* ao zerar. | Aprovado | Anexado no Card Jira |
| **CT-10** | **PI2-77** | UH12: Resultado após fases | `src/funcoes.c` | Exibição da tela de resumo e feedback imediato ao finalizar cada estação temporal. | Aprovado | Anexado no Card Jira |
| **CT-11** | **PI2-78** | UH13: Coleção de Artefatos de Memória | `src/funcoes.c` | Coleta e contabilização dos artefatos temporais (Disquete, CD, SSD) no inventário. | Aprovado | Anexado no Card Jira |
| **CT-12** | **PI2-79** | UH14: Confronto Final com a IA | `src/funcoes.c` | Execução das 3 perguntas finais ($Q_1, Q_2, D$) diante do núcleo de A.R.I.E.L. em 2048. | Aprovado | Anexado no Card Jira |
| **CT-13** | **PI2-80** | UH15: Desfecho - Final Ruim | `src/funcoes.c` | Acionamento do desfecho trágico quando vidas zeram ou há falha crítica na compreensão ($\neg V \lor \neg T$). | Aprovado | Anexado no Card Jira |
| **CT-14** | **PI2-81** | UH16: Desfecho - Final Padrão | `src/funcoes.c` | Acionamento dos desfechos intermediários (*Trégua Frágil* / *Silêncio* / *Piloto Automático*). | Aprovado | Anexado no Card Jira |
| **CT-15** | **PI2-82** | UH17: Desfecho - Final Ótimo | `src/funcoes.c` | Execução da combinação canônica ($Q_1=1 \land Q_2=1 \land D=2$) gerando *De Volta às Mãos Humanas*. | Aprovado | Anexado no Card Jira |
| **CT-16** | **PI2-84** | UH19: Salvamento em Arquivo Local | `src/funcoes.c` | Persistência dos dados de pontuação e progresso em arquivo local após conclusão da partida. | Aprovado | Anexado no Card Jira |
