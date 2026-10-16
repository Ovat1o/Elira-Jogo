# Instruções de Compilação e Execução - ELIRA

Este documento explica como configurar o ambiente, compilar e executar o jogo **ELIRA**, seja no Windows (com Raylib instalada em qualquer pasta) ou em outros sistemas operacionais.

---

## 1. Pré-requisitos

Para compilar o jogo, você precisa de:
- **Compilador C (GCC)**: Recomendado o **w64devkit** que vem junto com o instalador oficial da Raylib, ou MinGW-w64 configurado no PATH.
- **GNU Make**: Utilitário de automação de compilação.
- **Raylib**: Biblioteca gráfica usada no desenvolvimento.

---

## 2. Configurando o Caminho da Raylib (`config.mk`)

Por padrão, o `Makefile` assume o caminho padrão de instalação da Raylib no Windows: `C:/raylib`.

Se a sua Raylib estiver instalada em outro diretório (por exemplo, `D:/raylib` ou `C:/Users/SeuUsuario/raylib`):

1. Na raiz do projeto, crie um arquivo chamado **`config.mk`** (ou copie a partir de `config.mk.example`).
   > *Nota: O arquivo `config.mk` já está no `.gitignore` e não será enviado para o repositório.*
2. Defina a variável `RAYLIB_DIR` apontando para o seu diretório da Raylib (use barras normais `/`):
   ```makefile
   RAYLIB_DIR = D:/caminho/para/raylib
   ```
3. Se você estiver usando Linux ou tiver o GCC e a Raylib instalados globalmente no sistema (ex: via MSYS2 / Pacman), basta adicionar:
   ```makefile
   CC = gcc
   ```

---

## 3. Comandos de Compilação

Abra o terminal na pasta do projeto e execute:

### Compilar o jogo:
```bash
make
```

### Compilar e rodar:
```bash
make run
```

### Limpar arquivos gerados:
```bash
make clean
```

### Compilar passando o caminho diretamente via terminal (sem `config.mk`):
```bash
make RAYLIB_DIR=D:/caminho/para/raylib
```

---

## 4. Configuração no Visual Studio Code (IntelliSense)

Se você utiliza o VSCode e notar linhas vermelhas indicando que `#include "raylib.h"` não foi encontrado:

1. Abra o arquivo [`.vscode/c_cpp_properties.json`](.vscode/c_cpp_properties.json).
2. Atualize os caminhos `"includePath"` e `"compilerPath"` conforme a sua instalação:
   ```json
   {
       "configurations": [
           {
               "name": "Win32",
               "includePath": [
                   "${workspaceFolder}/**",
                   "D:/caminho/para/raylib/raylib/src"
               ],
               "compilerPath": "D:/caminho/para/raylib/w64devkit/bin/gcc.exe",
               "cStandard": "c99",
               "intelliSenseMode": "windows-gcc-x64"
           }
       ],
       "version": 4
   }
   ```

