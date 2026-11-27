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

Nos modos Estendida e Espelhada, o Tromonibar usa 720p se a NVIDIA listar esse modo e 1080p estiver indisponível ou não ativar o layout. O desktop lógico e a compensação de overscan são preservados. Se o driver não ativar o modo da TV, o menu exibe um erro.

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

`status --json` expõe `hdmi_connected` e `hdmi_active`. A primeira indica detecção
da TV pelo XRandR. A segunda indica um modo de vídeo em uso. Compare as duas
ao diagnosticar a TV abaixo de GNOME/Xorg.

## Persistência e Comportamento

- **Estado salvo:** cada alteração grava automaticamente o perfil em `~/.config/tromonibar/state.conf`.
- **Autostart no Xorg:** `tromonibar-apply.desktop` aguarda `3s` após o login (`X-GNOME-Autostart-Delay=3`) e executa `tromonibar apply` para restaurar o último estado.
- **Saída ausente:** ao reaplicar o perfil, o Tromonibar ignora a TV (`HDMI-0`) ou o monitor HP (`DVI-D-0`) se estiverem desconectados. Se o perfil salvo pede a TV e ela está desconectada, `apply` restaura os demais monitores, preserva o perfil e retorna código 2. A ativação explícita da TV requer que HDMI-0 esteja conectada e anuncie 1080p ou 720p.
- **Barra superior fixa no monitor principal:** o `DPY-2` (`DP-0`) é posicionado primeiro na `CurrentMetaMode` e `XineramaInfoOrder=DP-0` é reforçado após cada troca, evitando que a barra do GNOME mude de tela ao espelhar ou desativar monitores.

