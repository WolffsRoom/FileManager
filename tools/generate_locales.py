#!/usr/bin/env python3
"""Generate File Manager RCO locales from matching FMVita/VitaShell strings.

Unmatched File Manager-specific strings intentionally fall back to English.
Run from the repository root and apply the emitted patch.
"""
from pathlib import Path
import html
import re

SOURCE = Path("resource/locale/en.xml")
TARGETS = {
    "es": (Path("legacy/l10n/spanish.txt"), "Español"),
    "fr": (Path("legacy/l10n/french.txt"), "Français"),
    "de": (Path("legacy/l10n/german.txt"), "Deutsch"),
}

KEYS = {
    "msg_ok": "OK", "msg_cancel": "CANCEL", "msg_opt_copy": "COPY",
    "msg_opt_move": "MOVE", "msg_opt_paste": "PASTE", "msg_opt_delete": "DELETE",
    "msg_opt_rename": "RENAME", "msg_opt_new_folder": "NEW_FOLDER",
    "msg_opt_info": "PROPERTIES", "msg_opt_more": "MORE", "msg_undo": "UNDO_ACTION",
    "msg_copying": "COPYING", "msg_moving": "MOVING", "msg_deleting": "DELETING",
    "msg_installing": "INSTALLING", "msg_wait": "PLEASE_WAIT",
    "msg_rename_title": "RENAME", "msg_new_folder_title": "NEW_FOLDER",
    "msg_info_title": "PROPERTIES", "msg_info_size": "PROPERTY_SIZE",
    "msg_info_folders": "FOLDER", "msg_info_files": "FILE_",
    "msg_set_language": "VITASHELL_SETTINGS_LANGUAGE", "msg_set_select": "VITASHELL_SETTINGS_SELECT_BUTTON",
    "msg_view_list": "VIEW_MODE_LIST", "msg_view_two_columns": "VIEW_MODE_2COLUMN",
    "msg_sort": "SORT_BY", "msg_sort_name": "BY_NAME", "msg_sort_date": "BY_DATE",
    "msg_opt_select": "MARK_ALL", "msg_opt_clear_selection": "UNMARK_ALL",
    "msg_select_all": "MARK_ALL", "msg_ftp_server": "FTP_SERVER_TITLE",
    "msg_qr_code": "VITASHELL_SETTINGS_SELECT_BUTTON_QR", "msg_qr_scanning": "QR_SCANNING",
    "msg_btn_cross": "CONFIRM_YES_BTN", "msg_btn_circle": "NAV_BACK",
    "msg_cmd_open": "NAV_OPEN", "msg_cmd_back": "NAV_BACK", "msg_cmd_options": "MORE",
    "msg_cmd_mark": "MARK_ALL", "msg_cmd_settings": "SEARCH",
    "msg_music_title": "TITLE", "msg_music_prev": "NAV_BACK", "msg_music_play": "NAV_OPEN",
    "msg_music_export": "EXPORT_MEDIA", "msg_preview_mirror": "MIRROR_BTN",
    "msg_text_edit_line": "EDIT_LINE", "msg_text_insert_line": "INSERT_EMPTY_LINE",
    "msg_text_delete_line": "DELETE", "msg_text_save": "SAVE_BTN",
    "msg_install_title": "INSTALLING", "msg_install_button": "INSTALLING",
    "msg_install_confirm": "INSTALL_QUESTION", "msg_wifi_off": "WIFI_ERROR",
}

def legacy_strings(path: Path):
    out = {}
    raw = path.read_bytes()
    try:
        decoded = raw.decode("utf-8")
    except UnicodeDecodeError:
        decoded = raw.decode("cp1252")
    for line in decoded.splitlines():
        m = re.match(r'^([A-Z0-9_]+)\s*=\s*"(.*)"\s*$', line)
        if m:
            value = m.group(2).replace(r"\n", "&#10;").replace(r"\\", "&#10;").replace("\\", " ")
            out[m.group(1)] = value
    return out

entries = re.findall(r'<string src="(.*?)" id="(.*?)"\s*/>', SOURCE.read_text(encoding="utf-8"))
print("*** Begin Patch")
for code, (legacy_path, display_name) in TARGETS.items():
    old = legacy_strings(legacy_path)
    print(f"*** Add File: resource/locale/{code}.xml")
    print("+<?xml version=\"1.0\" encoding=\"utf-8\"?>")
    print("+<stringset>")
    for source, ident in entries:
        value = old.get(KEYS.get(ident, ""), html.unescape(source).replace("\n", "&#10;"))
        if ident == f"msg_lang_{code}":
            value = display_name
        value = html.escape(value, quote=True).replace("&amp;#10;", "&#10;")
        value = "".join(ch if ord(ch) < 128 else f"&#x{ord(ch):X};" for ch in value)
        print(f'+  <string src="{value}" id="{ident}" />')
    print("+</stringset>")
print("*** End Patch")
