import GObject from 'gi://GObject';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const TV_MODES = [
    {id: 'extend', label: 'TV: Estendida (Esquerda)'},
    {id: 'mirror', label: 'TV: Espelhada (Principal)'},
    {id: 'off', label: 'TV: Desligada (Off)'},
];

const HP_MODES = [
    {id: 'portrait-right', label: 'HP: Retrato (Direita / 270°)'},
    {id: 'portrait-left', label: 'HP: Retrato (Esquerda / 90°)'},
    {id: 'landscape', label: 'HP: Paisagem (Normal)'},
    {id: 'off', label: 'HP: Desligado (Off)'},
];

function getBinaryPath() {
    return GLib.build_filenamev([GLib.get_home_dir(), '.local', 'bin', 'tromonibar']);
}

function runCommandAsync(args) {
    return new Promise((resolve, reject) => {
        try {
            const proc = Gio.Subprocess.new(
                args,
                Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_PIPE
            );
            proc.communicate_utf8_async(null, null, (source, res) => {
                try {
                    const [, stdout, stderr] = source.communicate_utf8_finish(res);
                    if (source.get_successful()) {
                        resolve(stdout.trim());
                    } else {
                        reject(new Error(stderr.trim() || `Exit ${source.get_exit_status()}`));
                    }
                } catch (e) {
                    reject(e);
                }
            });
        } catch (e) {
            reject(e);
        }
    });
}

const TromonibarIndicator = GObject.registerClass(
class TromonibarIndicator extends PanelMenu.Button {
    _init() {
        super._init(0.0, 'Tromonibar');

        this._icon = new St.Icon({
            icon_name: 'video-display-symbolic',
            style_class: 'system-status-icon',
        });
        this.add_child(this._icon);

        this._tvItems = new Map();
        this._hpItems = new Map();
        this._busy = false;
        this._destroyed = false;

        const tvHeader = new PopupMenu.PopupMenuItem('TV Panasonic (HDMI-0)', {
            reactive: false,
            can_focus: false,
        });
        tvHeader.label.add_style_class_name('popup-subtitle-menu-item');
        this.menu.addMenuItem(tvHeader);

        for (const mode of TV_MODES) {
            const item = new PopupMenu.PopupMenuItem(mode.label);
            item.connect('activate', () => this._applyMode(['tv', mode.id]));
            this.menu.addMenuItem(item);
            this._tvItems.set(mode.id, item);
        }

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        const hpHeader = new PopupMenu.PopupMenuItem('Monitor HP (DVI-D-0)', {
            reactive: false,
            can_focus: false,
        });
        hpHeader.label.add_style_class_name('popup-subtitle-menu-item');
        this.menu.addMenuItem(hpHeader);

        for (const mode of HP_MODES) {
            const item = new PopupMenu.PopupMenuItem(mode.label);
            item.connect('activate', () => this._applyMode(['hp', mode.id]));
            this.menu.addMenuItem(item);
            this._hpItems.set(mode.id, item);
        }

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        const reapplyItem = new PopupMenu.PopupMenuItem('Reaplicar perfil salvo');
        reapplyItem.connect('activate', () => this._applyMode(['apply']));
        this.menu.addMenuItem(reapplyItem);

        this.menu.connect('open-state-changed', (_menu, isOpen) => {
            if (isOpen) {
                this._refreshStatus();
            }
        });

        this._refreshStatus();
    }

    async _refreshStatus() {
        if (this._destroyed) {
            return;
        }
        try {
            const out = await runCommandAsync([getBinaryPath(), 'status', '--json']);
            if (this._destroyed) {
                return;
            }
            const state = JSON.parse(out);
            for (const [id, item] of this._tvItems.entries()) {
                item.setOrnament(
                    state.tv === id ? PopupMenu.Ornament.CHECK : PopupMenu.Ornament.NONE
                );
            }
            for (const [id, item] of this._hpItems.entries()) {
                item.setOrnament(
                    state.hp === id ? PopupMenu.Ornament.CHECK : PopupMenu.Ornament.NONE
                );
            }
        } catch (e) {
            logError(e, 'Tromonibar: failed to query status');
        }
    }

    async _applyMode(subArgs) {
        if (this._busy || this._destroyed) {
            return;
        }
        this._busy = true;
        this._icon.opacity = 140;
        try {
            await runCommandAsync([getBinaryPath(), ...subArgs]);
            await this._refreshStatus();
        } catch (e) {
            logError(e, 'Tromonibar: failed to apply display mode');
            Main.notify('Tromonibar', `Erro ao aplicar modo: ${e.message}`);
        } finally {
            this._busy = false;
            if (!this._destroyed) {
                this._icon.opacity = 255;
            }
        }
    }

    destroy() {
        this._destroyed = true;
        super.destroy();
    }
});

export default class TromonibarExtension extends Extension {
    enable() {
        this._indicator = new TromonibarIndicator();
        Main.panel.addToStatusArea(this.uuid, this._indicator);
    }

    disable() {
        if (this._indicator) {
            this._indicator.destroy();
            this._indicator = null;
        }
    }
}
