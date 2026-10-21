# Instruções de Compilação e Execução - ELIRA

Este documento explica a organização atual do código-fonte e como configurar o ambiente, compilar e executar o jogo **ELIRA** no Windows, Linux ou macOS.

---

## 1. Estrutura Atual do Projeto (`Elira-Jogo`)

O código em C do projeto está modularizado dentro da pasta `src/`, que já inclui também os cabeçalhos necessários da biblioteca **Raylib**:

```text
Elira-Jogo/
├── Makefile                  # Script de automação da compilação modular
├── config.mk.example         # Exemplo de configuração local opcional
├── assets/                   # Recursos visuais, fontes e áudio do jogo
└── src/
    ├── main.c                # Função main() e loop principal de execução
    ├── funcoes.c             # Implementação por extenso de todas as funções do jogo
    ├── constantes.h          # Todas as diretivas #define e constantes do jogo
    ├── funcoes.h             # Estruturas (struct/enum), globais e protótipos das funções
    ├── raylib.h              # Cabeçalho principal da biblioteca Raylib
    ├── raymath.h             # Cabeçalho de funções matemáticas da Raylib
    └── rlgl.h                # Cabeçalho da camada OpenGL (rlgl) da Raylib
```

---

## 2. Pré-requisitos

Como os 3 arquivos de cabeçalho da Raylib (`raylib.h`, `raymath.h` e `rlgl.h`) já estão incluídos na pasta `src/`, para compilar o projeto você precisa apenas de:

- **Compilador C (GCC)**: Recomendado o **w64devkit** (que acompanha o instalador oficial da Raylib no Windows) ou MinGW-w64 / GCC configurado no `PATH`.
- **GNU Make** (`make` ou `mingw32-make`): Utilitário de automação de build.
- **Biblioteca Raylib compilada (`libraylib.a`)**: Para a etapa de linkagem (`-lraylib`).

---

## 3. Configurando o Caminho da Raylib (`config.mk`)

Por padrão, no Windows, o `Makefile` busca o compilador e a biblioteca `libraylib.a` na instalação padrão em `C:/raylib` (ou diretamente em `src/`, caso `libraylib.a` tenha sido copiada para lá).

Se a sua instalação da Raylib estiver em outro diretório (por exemplo, `D:/raylib`):

1. Na raiz da pasta `Elira-Jogo`, crie um arquivo chamado **`config.mk`** (você pode copiar a partir de `config.mk.example`).
   > *Nota: O arquivo `config.mk` está listado no `.gitignore` para não sobrescrever a configuração de outros integrantes.*
2. Defina a variável `RAYLIB_DIR` apontando para a sua pasta da Raylib (utilize barras normais `/`):
   ```makefile
   RAYLIB_DIR = D:/caminho/para/raylib
   ```
3. Caso utilize um compilador específico instalado no sistema, você também pode definir:
   ```makefile
   CC = gcc
   ```

---

## 4. Comandos de Compilação

Abra o terminal dentro da pasta **`Elira-Jogo`** e execute:

### Compilar o jogo:
Compila `src/main.c` e `src/funcoes.c` em arquivos-objeto (`src/main.o` e `src/funcoes.o`) usando os cabeçalhos de `src/` e gera o executável final (`ELIRA.exe` no Windows ou `ELIRA` no Linux/macOS):
```bash
make
```

### Compilar e executar imediatamente:
```bash
make run
```

### Limpar arquivos gerados (`.o` e executável):
```bash
make clean
```

### Compilar passando o caminho diretamente via terminal (sem `config.mk`):
```bash
make RAYLIB_DIR=D:/caminho/para/raylib
```

---

## 5. Compilação Manual Direta via GCC (Alternativa sem Make)

Caso deseje compilar manualmente pelo terminal sem usar o `make`, execute na pasta `Elira-Jogo`:

**No Windows (PowerShell / CMD):**
```powershell
gcc -Isrc -std=c99 -Wall -Wextra src/main.c src/funcoes.c -o ELIRA.exe -Lsrc -LC:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm
```

**No Linux:**
```bash
gcc -Isrc -std=c99 -Wall -Wextra src/main.c src/funcoes.c -o ELIRA -Lsrc -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

**No macOS (com Raylib via Homebrew):**
```bash
gcc -Isrc -std=c99 -Wall -Wextra src/main.c src/funcoes.c -o ELIRA $(brew --prefix raylib)/lib/libraylib.a -framework Cocoa -framework IOKit -framework OpenGL -framework CoreVideo
```

---

## 6. Configuração no Visual Studio Code (IntelliSense)

Como os cabeçalhos `raylib.h`, `raymath.h`, `rlgl.h`, `constantes.h` e `funcoes.h` já estão todos localizados dentro da pasta `src/`, o arquivo [`.vscode/c_cpp_properties.json`](.vscode/c_cpp_properties.json) já encontra todos os `.h` automaticamente através de `"${workspaceFolder}/**"`:

```json
{
    "configurations": [
        {
            "name": "Win32",
            "includePath": [
                "${workspaceFolder}/**"
            ],
            "defines": [
                "_DEBUG",
                "UNICODE",
                "_UNICODE"
            ],
            "cStandard": "c11",
            "cppStandard": "gnu++14",
            "intelliSenseMode": "windows-gcc-x86"
        }
    ],
    "version": 4
}
```
