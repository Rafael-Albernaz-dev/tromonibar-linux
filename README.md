# tromonibar-linux

Controlador rápido na barra superior do **GNOME 46 (Xorg)** com motor CLI em **C** para gerenciar múltiplos monitores com placa **NVIDIA** — incluindo compensação de overscan (`ViewPortOut`) para TV Panasonic via HDMI e rotação rápida do monitor HP.

## Arquitetura

- **`src/` (C11):** binário `tromonibar` compilado com `gcc` que consulta o `CurrentMetaMode` via `nvidia-settings`, calcula as coordenadas `+X+Y` sem deixar espaços mortos, aplica os perfis atomicamente e salva o último estado em `~/.config/tromonibar/state.conf`.
- **`extension/` (GNOME Shell 46 GJS):** indicador nativo na barra superior (`PanelMenu.Button`) que consulta e alterna os modos de forma assíncrona.
- **`autostart/`:** restaura automaticamente o último perfil salvo ao iniciar sessão X11/Xorg.

## Modos disponíveis

### TV Panasonic (`HDMI-0` / `DPY-1`)
- **Estendida (Esquerda):** `1920x1080 @ 60 Hz` com `ViewPortOut=1824x1026+48+27` (sem cortes nas bordas).
- **Espelhada (Principal):** espelha `DP-0` mantendo `165 Hz` no monitor principal e `60 Hz` sem cortes na TV.
- **Desligada (Off):** corta o sinal HDMI da TV para liberar mouse/janelas e permitir *standby*.

### Monitor HP (`DVI-D-0` / `DPY-0`)
- **Retrato (Direita / 270°):** `900x1440`
- **Retrato (Esquerda / 90°):** `900x1440`
- **Paisagem (Normal):** `1440x900`
- **Desligado (Off):** corta o sinal DVI.

## Instalação

```bash
make all test
make install
```

Para carregar a extensão recém-instalada no GNOME em **Xorg**:
1. Pressione `Alt + F2`, digite `r` e aperte `Enter` (ou faça logoff/login).
2. Ative a extensão:
   ```bash
   gnome-extensions enable tromonibar@tromonibar-linux
   ```

## Uso via CLI

```bash
tromonibar status [--json]
tromonibar tv <extend|mirror|off>
tromonibar hp <portrait-right|portrait-left|landscape|off>
tromonibar apply
```
