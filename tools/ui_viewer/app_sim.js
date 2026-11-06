// Simulation of src/browser_menu.cpp and friends over a fake file system, so the
// screens can be looked at and driven from a PC. Keep the rules in step with the C++:
// what is on each option menu, what each key does, which page opens.
'use strict';

const GB = 1024 * 1024 * 1024, MB = 1024 * 1024;

// ------------------------------------------------------------------ fake file system

const FS = {
  'ux0:': ['app/', 'data/', 'tai/', 'pspemu/', 'video/', 'Fotos/', 'jogo.vpk:41943040', 'leia-me.txt:2048'],
  'ux0:app': ['PCSE00000/', 'VITASHELL/', 'FILEMGR01/'],
  'ux0:data': ['FileManager/', 'savegames/', 'nota.txt:512'],
  'ux0:data/FileManager': ['config.txt:20', 'perf.log:300'],
  'ux0:Fotos': ['praia.png:2400000', 'gato.jpg:1800000', 'clip.mp4:52000000'],
  'ux0:video': [],
  'ux0:tai': ['config.txt:900', 'kubridge.skprx:20000'],
  'ur0:tai': ['config.txt:600', 'henkaku.suprx:40000'],
};
const DEVICES = [
  { name: 'ux0:', label: 'msg_dev_ux0', icon: 'tex_ui_root', free: 12.3 * GB, cap: 29.7 * GB },
  { name: 'uma0:', label: 'msg_dev_uma0', icon: 'tex_ui_root', free: 55 * GB, cap: 59.6 * GB },
  { name: 'ur0:', label: 'msg_dev_ur0', icon: 'tex_ui_root', free: 2.6 * GB, cap: 3.1 * GB },
];
const SYSTEM = ['vs0:', 'os0:', 'sa0:', 'tm0:', 'ud0:', 'pd0:'];
const SHORTCUTS = [['ux0:Downloads', 'msg_shortcut_downloads', 'tex_ui_download'], ['ux0:picture', 'msg_shortcut_pictures', 'tex_ui_image'], ['ux0:music', 'msg_shortcut_music', 'tex_ui_music'], ['ux0:pspemu', 'msg_shortcut_psp', 'tex_new_open_folder'], ['ux0:temp', 'msg_shortcut_trash', 'tex_new_trash'], ['ux0:app', 'msg_shortcut_apps', 'tex_new_open_folder'], ['ux0:data', 'msg_shortcut_data', 'tex_new_open_folder'], ['ur0:tai', 'msg_shortcut_plugins', 'tex_new_open_folder']];

function kindOf(name) {
  const e = (name.match(/\.[^.]+$/) || [''])[0].toLowerCase();
  if (['.png', '.jpg', '.jpeg', '.bmp', '.gif'].includes(e)) return 'image';
  if (['.mp4', '.mkv', '.avi'].includes(e)) return 'video';
  if (['.mp3', '.ogg', '.at9', '.wav'].includes(e)) return 'music';
  if (['.txt', '.ini', '.xml', '.log', '.cfg', '.json'].includes(e)) return 'text';
  if (e === '.vpk') return 'vpk';
  if (e === '.sfo') return 'sfo';
  if (['.zip', '.rar', '.7z', '.psarc'].includes(e)) return 'package';
  if (['.suprx', '.skprx', '.self', '.bin'].includes(e)) return 'plugin';
  return 'other';
}
const ICON = { folder: 'tex_new_folder', image: 'tex_ui_image', video: 'tex_file_icon_video', music: 'tex_ui_audio', text: 'tex_ui_text', package: 'tex_ui_archive', vpk: 'tex_ui_vpk', sfo: 'tex_ui_sfo', plugin: 'tex_ui_file', other: 'tex_ui_file' };

function listDir(path) {
  if (path === '') {
    const out = DEVICES.map((d) => ({ root: true, name: d.name, label: d.label, icon: d.icon, free: d.free, cap: d.cap, target: d.name, isDir: true }));
    for (const b of SIM.bookmarks) out.push({ root: true, name: b, labelText: b.split(/[\/:]/).filter(Boolean).pop(), icon: 'tex_file_icon_folder', target: b, isDir: true, bookmark: true });
    for (const [p, l, ic] of SHORTCUTS) out.push({ root: true, name: p, label: l, icon: ic, target: p, isDir: true });
    out.push({ root: true, name: SYSTEM.join(' '), label: 'msg_system_partitions', icon: 'tex_new_lock', target: '::system', isDir: true });
    return out;
  }
  if (path === '::system') return SYSTEM.map((n) => ({ root: true, name: n, label: 'msg_dev_system', icon: 'tex_ui_settings', target: n, isDir: true }));
  const items = (FS[path] || []).map((s) => {
    const [n, size] = s.split(':');
    const isDir = n.endsWith('/');
    const name = isDir ? n.slice(0, -1) : n;
    return { name, isDir, size: +size || 0, kind: isDir ? 'folder' : kindOf(name), marked: false };
  });
  items.sort((a, b) => (a.isDir !== b.isDir ? (a.isDir ? -1 : 1) : a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' })));
  return items;
}

function join(path, name) { return /[:\/]$/.test(path) ? path + name : path + '/' + name; }
function fmtSize(b) {
  if (b < 1024) return b + ' B';
  const u = ['B', 'KB', 'MB', 'GB', 'TB']; let i = 0, s = 1;
  while (b / s >= 1024 && i < 4) { s *= 1024; i++; }
  return (Math.round(b * 10 / s) / 10).toFixed(1) + ' ' + u[i];
}
const isRootStyle = (p) => p === '' || p === '::system';
const previewable = (e) => e.kind === 'image' || (e.kind === 'video' && /\.mp4$/i.test(e.name));

// ------------------------------------------------------------------ state

const THEMES = [
  ['msg_theme_default', [1, 1, 1], [1, 1, 1]], ['msg_theme_midnight', [0.10, 0.10, 0.13], [0.55, 0.75, 1]],
  ['msg_theme_graphite', [0.34, 0.36, 0.40], [0.85, 0.88, 0.92]], ['msg_theme_ocean', [0.05, 0.42, 0.55], [0.45, 0.95, 1]],
  ['msg_theme_purple', [0.33, 0.16, 0.58], [0.82, 0.68, 1]], ['msg_theme_emerald', [0.05, 0.40, 0.27], [0.55, 1, 0.72]],
  ['msg_theme_wine', [0.46, 0.08, 0.18], [1, 0.62, 0.68]], ['msg_theme_pink', [0.68, 0.22, 0.48], [1, 0.75, 0.88]],
  ['msg_theme_custom', [1, 1, 1], [1, 1, 1]],
];

const SIM = {
  pages: [], theme: 0, view: 0, clipboard: [], clipMove: false, bookmarks: [],
  overlay: null,   // { type: 'options' | 'settings' | 'preview' | 'input' | 'dialog', ... }
  browser: null,
};

function log(msg) {
  const el = document.getElementById('log');
  el.textContent = msg + '\n' + el.textContent.slice(0, 3000);
}

function pushPage(path) {
  SIM.pages.push({ path, entries: listDir(path), focused: 0, scroll: 0 });
}
function topPage() { return SIM.pages[SIM.pages.length - 1]; }
const twoColumns = () => false;

// ------------------------------------------------------------------ actions (mirror of BrowserMenu)

function optionActions() {
  const p = topPage();
  const atRoot = isRootStyle(p.path);
  const target = p.entries[p.focused];
  const marked = p.entries.some((e) => e.marked);
  const a = [];
  if (!atRoot) {
    if (target || marked) a.push('copy', 'move', 'delete');
    if (SIM.clipboard.length) a.push('paste');
    if (target && !marked) a.push('rename');
    a.push('new_folder');
    if (target && !marked) a.push('info');
    a.push('more');
  } else {
    if (target && target.target !== '::system' && target.target !== '::ftp') a.push('info');
    if (target && target.bookmark) a.push('remove_shortcut');
    a.push('ftp', 'settings');
  }
  return a;
}
function moreActions() {
  const p = topPage();
  const a = [];
  if (p.entries.some((e) => e.marked)) a.push('clear_selection');
  const target = p.entries[p.focused];
  if (target && target.isDir) a.push('add_shortcut');
  a.push('ftp', 'settings');
  return a;
}
const LABEL = {
  copy: 'msg_opt_copy', move: 'msg_opt_move', delete: 'msg_opt_delete', paste: 'msg_opt_paste', rename: 'msg_opt_rename',
  new_folder: 'msg_opt_new_folder', info: 'msg_opt_info', settings: 'msg_opt_settings', more: 'msg_opt_more',
  clear_selection: 'msg_opt_clear_selection', add_shortcut: 'msg_opt_add_shortcut', remove_shortcut: 'msg_opt_remove_shortcut', ftp: 'msg_ftp_server',
};

function openOptions(actions) {
  SIM.overlay = { type: 'options', actions, focused: 0 };
}

function doAction(act) {
  const p = topPage();
  const t = p.entries[p.focused];
  switch (act) {
    case 'more': openOptions(moreActions()); return;
    case 'settings': SIM.overlay = { type: 'settings', focused: 0 }; return;
    case 'ftp': SIM.overlay = { type: 'ftp', t0: performance.now(), focus: 0 }; return;
    case 'copy': case 'move':
      SIM.clipboard = p.entries.filter((e) => e.marked).map((e) => join(p.path, e.name));
      if (!SIM.clipboard.length && t) SIM.clipboard = [join(p.path, t.name)];
      SIM.clipMove = act === 'move'; log('clipboard: ' + SIM.clipboard.join(', ')); break;
    case 'paste': log((SIM.clipMove ? 'move ' : 'copy ') + SIM.clipboard.join(', ') + ' -> ' + p.path); SIM.clipboard = []; break;
    case 'delete': SIM.overlay = { type: 'dialog', title: pafString('msg_confirm_delete_title'), text: pafString('msg_confirm_delete'), button: 'yesno' }; return;
    case 'rename': SIM.overlay = { type: 'input', title: pafString('msg_rename_title'), value: t.name }; return;
    case 'new_folder': SIM.overlay = { type: 'input', title: pafString('msg_new_folder_title'), value: '' }; return;
    case 'info': SIM.overlay = { type: 'dialog', title: pafString('msg_info_title'), text: (t.root ? t.target : join(p.path, t.name)) + '\n' + pafString('msg_info_size') + ': ' + fmtSize(t.root ? t.cap || 0 : t.size), button: 'ok' }; return;
    case 'clear_selection': p.entries.forEach((e) => { e.marked = false; }); break;
    case 'add_shortcut': if (t) { SIM.bookmarks.push(join(p.path, t.name)); log('atalho: ' + join(p.path, t.name)); } break;
    case 'remove_shortcut': SIM.bookmarks = SIM.bookmarks.filter((b) => b !== t.target); SIM.pages[0].entries = listDir(''); break;
  }
  SIM.overlay = null;
}

function enterFocused() {
  const p = topPage();
  const e = p.entries[p.focused];
  if (!e) return;
  if (e.target === '::ftp') { doAction('ftp'); return; }
  if (e.isDir) { pushPage(e.root ? e.target : join(p.path, e.name)); return; }
  if (previewable(e)) {
    const list = p.entries.filter(previewable);
    SIM.overlay = { type: 'preview', list, index: list.indexOf(e), path: p.path };
    return;
  }
  doAction('info');
}

// ------------------------------------------------------------------ input

function key(k) {
  const o = SIM.overlay;
  if (o) return keyOverlay(o, k);
  const p = topPage();
  switch (k) {
    case 'up': p.focused = Math.max(0, p.focused - 1); break;
    case 'down': p.focused = Math.min(p.entries.length - 1, p.focused + 1); break;
    case 'cross': enterFocused(); break;
    case 'right': { const e = p.entries[p.focused]; if (e && e.isDir) enterFocused(); break; }
    case 'circle': case 'left': if (SIM.pages.length > 1) SIM.pages.pop(); break;
    case 'triangle': openOptions(optionActions()); break;
    case 'square': { const e = p.entries[p.focused]; if (e && !isRootStyle(p.path)) e.marked = !e.marked; break; }
    case 'select': doAction('ftp'); break;
  }
}

function keyOverlay(o, k) {
  if (o.type === 'options') {
    if (k === 'up') o.focused = Math.max(0, o.focused - 1);
    else if (k === 'down') o.focused = Math.min(o.actions.length - 1, o.focused + 1);
    else if (k === 'cross') doAction(o.actions[o.focused]);
    else if (k === 'circle' || k === 'triangle') SIM.overlay = null;
  } else if (o.type === 'settings') {
    const rows = THEMES.length;
    if (k === 'up') o.focused = Math.max(0, o.focused - 1);
    else if (k === 'down') o.focused = Math.min(rows - 1, o.focused + 1);
    else if (k === 'cross') SIM.theme = o.focused;
    else if (k === 'circle') SIM.overlay = null;
  } else if (o.type === 'ftp') {
    if (k === 'circle') { SIM.overlay = null; SIM.ftpRunning = true; }
    else if (k === 'left' || k === 'right') o.focus = o.focus ? 0 : 1;
    else if (k === 'cross') { SIM.ftpRunning = !o.focus; SIM.overlay = null; }
  } else if (o.type === 'preview') {
    if (k === 'circle') SIM.overlay = null;
    else if (k === 'l') o.index = (o.index + o.list.length - 1) % o.list.length;
    else if (k === 'r') o.index = (o.index + 1) % o.list.length;
  } else {
    if (k === 'cross' || k === 'circle') SIM.overlay = null;
  }
}

// ------------------------------------------------------------------ screens

function applyTheme(bgPlane) {
  const th = THEMES[SIM.theme];
  bgPlane.texture = SIM.theme === 0 || SIM.theme === 8 ? 'tex_common_bg' : 'tex_bg_neutral';
  bgPlane.color = [th[1][0], th[1][1], th[1][2], 1];
}

function drawList(ctx, page, rect, alpha, active) {
  const rootStyle = isRootStyle(page.path);
  const rowH = rootStyle ? 96 : 80;
  const visible = Math.floor(rect.h / rowH);
  if (page.focused < page.scroll) page.scroll = page.focused;
  if (page.focused >= page.scroll + visible) page.scroll = page.focused - visible + 1;
  ctx.save();
  ctx.beginPath(); ctx.rect(rect.x, rect.y, rect.w, rect.h); ctx.clip();
  for (let i = page.scroll; i < Math.min(page.entries.length, page.scroll + visible + 1); i++) {
    const e = page.entries[i];
    const cell = { x: rect.x, y: rect.y + (i - page.scroll) * rowH, w: rect.w, h: rowH };
    const [item] = instantiate(rootStyle ? 'template_list_item_device' : 'template_list_item_file', null);
    item.size = [rect.w, rowH];
    const btn = item.find('image_button_list_item');
    btn.size = [rect.w < 960 ? rect.w - 16 : 840, rowH];
    btn.focused = active && i === page.focused;
    btn.image = PAF.images.get(PAF.ids.get(e.icon || ICON[e.kind]) ? PAF.ids.get(e.icon || ICON[e.kind]).a.src : '');
    if (rootStyle) {
      item.find('text_device_name').text = e.labelText || pafString(e.label);
      let info = e.name;
      if (e.cap) info += '  -  ' + pafString('msg_free_of').replace('%s', fmtSize(e.free)).replace('%s', fmtSize(e.cap));
      item.find('text_device_info').text = info;
      const bar = item.find('plane_device_bar');
      if (e.target === '::ftp' && SIM.ftpRunning) item.find('text_device_info').text = 'ftp://192.168.0.42:1337';
      if (!e.cap || rect.w < 960) bar.hidden = true;
      else {
        const used = 1 - e.free / e.cap;
        const fill = item.find('plane_device_bar_fill');
        fill.size = [220 * used, 8];
        fill.color = used > 0.9 ? [1, 0.35, 0.3, 1] : THEMES[SIM.theme][2].concat([1]);
      }
      if (rect.w < 960) { item.find('text_device_name').size = [rect.w - 150, 38]; item.find('text_device_info').size = [rect.w - 150, 28]; }
    } else {
      btn.text = e.name;
      item.find('text_list_item_info').text = e.isDir ? '' : fmtSize(e.size);
      item.find('plane_list_item_check').hidden = !e.marked;
    }
    drawWidget(ctx, item, { x: cell.x, y: cell.y, w: cell.w, h: cell.h }, alpha);
  }
  ctx.restore();
}

function drawBrowser(ctx) {
  const root = buildTree('page_browser');
  applyTheme(root.find('plane_base_bg_page_browser'));
  const p = topPage();
  const title = root.find('text_top');
  title.text = p.path === '' ? pafString('msg_app_title') : (p.path === '::system' ? pafString('msg_system_partitions') : p.path);
  root.find('busyindicator_loader_page_browser').spinning = false;
  const empty = root.find('text_empty_page_browser');
  empty.text = pafString('msg_empty_folder');
  empty.hidden = p.entries.length > 0;
  root.find('button_back_page_browser').hidden = SIM.pages.length < 2;
  const listRoot = root.find('plane_browser_root_page_browser');
  listRoot.listDraw = null;
  drawWidget(ctx, root, SCREEN, 1);

  // the list views are created per page at runtime, like PushPage does
  const lists = twoColumns()
    ? [[SIM.pages[SIM.pages.length - 2], 480, -240, false], [p, 480, 240, true]]
    : [[p, 960, 0, true]];
  for (const [page, w, x, active] of lists) {
    const [lv] = instantiate('template_list_view_generic', null);
    lv.size = [w, 448]; lv.pos = [x, -272];
    const r = lv.layout(SCREEN);
    drawList(ctx, page, r, active ? 1 : 0.55, active);
  }
  // corner buttons are drawn above the lists, as on the console
  for (const id of ['button_back_page_browser', 'button_options_page_browser']) {
    const b = root.find(id);
    drawWidget(ctx, Object.assign(b, { children: [] }), SCREEN, 1);
  }
}

function drawOptions(ctx, o) {
  const [base] = instantiate('template_option_menu_base', null);
  const plane = base.find('plane_option_menu_base');
  const n = o.actions.length;
  const labels = o.actions.map((a) => pafString(LABEL[a]));
  ctx.font = '600 28px "Segoe UI", system-ui, sans-serif';
  const len = Math.max(202, Math.max(...labels.map((l) => ctx.measureText(l).width)) + 40);
  // same arithmetic as OptionMenu::OnSizeAdjust
  plane.size = [len + 12, 12 + 60 * n];
  plane.pos = [264 + (202 - plane.size[0]) / 2, 43];
  base.find('text_option_menu_ruler').hidden = true;
  const box = base.find('box_option_menu');
  box.children = o.actions.map((a, i) => {
    const tid = n === 1 ? 'template_option_menu_button_single' : (i === 0 ? 'template_option_menu_button_top' : (i === n - 1 ? 'template_option_menu_button_bottom' : 'template_option_menu_button_middle'));
    const [b] = instantiate(tid, box);
    b.size = [len, 60]; b.text = labels[i]; b.focused = i === o.focused;
    b.anchor = [0, 2]; b.align = [0, 2]; b.pos = [0, -6 - 60 * i];
    return b;
  });
  box.size = plane.size;
  drawWidget(ctx, base, SCREEN, 1);
}

function drawSettings(ctx, o) {
  const root = buildTree('page_settings');
  applyTheme(root.find('plane_base_bg_page_settings'));
  root.find('text_top').text = pafString('msg_opt_settings');
  drawWidget(ctx, root, SCREEN, 1);
  const lv = root.find('list_view_page_settings');
  const rows = THEMES.map((t, i) => [t[0], 'msg_section_theme', SIM.theme === i]);
  const page = { path: '::settings', entries: rows, focused: o.focused, scroll: o.scroll || 0 };
  const r = lv.rect;
  const visible = Math.floor(r.h / 80);
  if (o.focused < page.scroll) page.scroll = o.focused;
  if (o.focused >= page.scroll + visible) page.scroll = o.focused - visible + 1;
  o.scroll = page.scroll;
  ctx.save(); ctx.beginPath(); ctx.rect(r.x, r.y, r.w, r.h); ctx.clip();
  for (let i = page.scroll; i < Math.min(rows.length, page.scroll + visible + 1); i++) {
    const [item] = instantiate('template_list_item_file', null);
    const btn = item.find('image_button_list_item');
    btn.text = pafString(rows[i][0]); btn.focused = i === o.focused;
    btn.image = PAF.images.get(PAF.ids.get('tex_file_icon_image').a.src);
    item.find('text_list_item_info').text = pafString(rows[i][1]);
    item.find('plane_list_item_check').hidden = !rows[i][2];
    drawWidget(ctx, item, { x: r.x, y: r.y + (i - page.scroll) * 80, w: r.w, h: 80 }, 1);
  }
  ctx.restore();
  for (const b of [root.find('button_back_page_settings')]) drawWidget(ctx, b, SCREEN, 1);
}

function drawPreview(ctx, o) {
  const root = buildTree('page_preview');
  const e = o.list[o.index];
  root.find('text_title_page_preview').text = e.name;
  root.find('text_error_page_preview').text = pafString('msg_preview_error');
  root.find('text_error_page_preview').hidden = true;
  root.find('busyindicator_page_preview').spinning = false;
  // a stand-in picture so the layout can be judged
  const cv = document.createElement('canvas'); cv.width = 320; cv.height = 180;
  const x = cv.getContext('2d');
  const g = x.createLinearGradient(0, 0, 320, 180); g.addColorStop(0, '#2b6cb0'); g.addColorStop(1, '#f6ad55');
  x.fillStyle = g; x.fillRect(0, 0, 320, 180);
  x.fillStyle = '#fff'; x.font = '24px sans-serif'; x.fillText(e.kind === 'video' ? '▶ vídeo' : 'imagem', 110, 96);
  const plane = root.find('plane_image_page_preview');
  plane.texture = null; plane.style = null; plane.surface = cv; plane.size = [960 * 0.95, 544 * 0.95 * 180 / 320 > 544 ? 544 : 960 * 180 / 320];
  drawWidget(ctx, root, SCREEN, 1);
}

function drawInput(ctx, o) {
  const root = buildTree('page_text_input');
  root.find('text_title_page_text_input').text = o.title;
  root.find('text_box_page_text_input').text = o.value;
  drawWidget(ctx, root, SCREEN, 1);
}

// Mirror of FtpMenu::Update, with a transfer that advances over time.
function drawFtp(ctx, o) {
  const root = buildTree('page_ftp');
  const t = (performance.now() - o.t0) / 1000;
  const fmt = (id, a, b) => pafString(id).replace('%s', a).replace('%s', b || '');
  root.find('text_title_page_ftp').text = pafString('msg_ftp_server');
  root.find('text_address_page_ftp').text = fmt('msg_ftp_address', '192.168.0.42', '1337');
  const total = 41943040, done = Math.min(total, Math.floor((t % 8) / 8 * total));
  const waiting = t < 2;
  root.find('text_connection_page_ftp').text = pafString(waiting ? 'msg_ftp_waiting' : 'msg_ftp_client');
  root.find('text_activity_page_ftp').text = waiting ? pafString('msg_ftp_running') : fmt('msg_ftp_sending', 'jogo.vpk');
  root.find('plane_bar_page_ftp').hidden = waiting;
  root.find('plane_bar_fill_page_ftp').size = [560 * done / total, 10];
  root.find('text_progress_page_ftp').text = waiting ? '' : fmt('msg_ftp_progress_of', fmtSize(done), fmtSize(total)) + '  (' + Math.floor(done * 100 / total) + '%)';
  const ok = root.find('button_ok_page_ftp'), cancel = root.find('button_cancel_page_ftp');
  ok.text = pafString('msg_ok'); cancel.text = pafString('msg_cancel');
  ok.focused = !o.focus; cancel.focused = !!o.focus;
  drawWidget(ctx, root, SCREEN, 1);
}

function drawDialog(ctx, o) {
  // Stand-in for the system's CommonGuiDialog: its look comes from the firmware at runtime.
  ctx.fillStyle = 'rgba(0,0,0,0.6)'; ctx.fillRect(0, 0, 960, 544);
  const r = { x: 180, y: 150, w: 600, h: 244 };
  ctx.fillStyle = '#e9ebef'; ctx.beginPath(); ctx.roundRect(r.x, r.y, r.w, r.h, 10); ctx.fill();
  ctx.fillStyle = '#1d2129'; ctx.font = '600 26px "Segoe UI", sans-serif'; ctx.fillText(o.title || '', r.x + 24, r.y + 44);
  ctx.font = '22px "Segoe UI", sans-serif';
  String(o.text).split('\n').forEach((l, i) => ctx.fillText(l, r.x + 24, r.y + 90 + i * 30));
  ctx.font = '600 22px "Segoe UI", sans-serif'; ctx.fillStyle = '#2a5bd7';
  const btn = o.button === 'yesno' ? 'Sim     Não' : (o.button === 'ok' ? 'OK' : pafString(o.button));
  ctx.fillText(btn, r.x + r.w - 24 - ctx.measureText(btn).width, r.y + r.h - 24);
  ctx.fillStyle = '#888'; ctx.font = '13px sans-serif'; ctx.fillText('(diálogo do sistema - aparência aproximada)', r.x + 24, r.y + r.h - 26);
}

function frame() {
  const ctx = document.getElementById('screen').getContext('2d');
  ctx.clearRect(0, 0, 960, 544);
  const o = SIM.overlay;
  try {
    if (o && o.type === 'settings') drawSettings(ctx, o);
    else if (o && o.type === 'preview') drawPreview(ctx, o);
    else {
      drawBrowser(ctx);
      if (o && o.type === 'options') drawOptions(ctx, o);
      if (o && o.type === 'input') drawInput(ctx, o);
      if (o && o.type === 'dialog') drawDialog(ctx, o);
      if (o && o.type === 'ftp') drawFtp(ctx, o);
    }
  } catch (err) {
    ctx.fillStyle = '#f66'; ctx.font = '16px monospace'; ctx.fillText(String(err), 10, 20);
    console.error(err);
  }
  requestAnimationFrame(frame);
}

const KEYS = {
  Enter: 'cross', Escape: 'circle', Backspace: 'circle', t: 'triangle', T: 'triangle', ' ': 'square',
  s: 'select', S: 'select', ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right', q: 'l', e: 'r',
};

// URL hash drives the simulation for screenshots: #keys=down,cross,triangle&lang=en
function runScript() {
  const h = new URLSearchParams(location.hash.slice(1));
  if (h.get('lang')) { PAF.lang = h.get('lang'); document.getElementById('lang').value = PAF.lang; }
  if (h.get('theme')) SIM.theme = +h.get('theme');
  if (h.get('view')) SIM.view = +h.get('view');
  if (h.get('bounds')) PAF.showBounds = true;
  (h.get('keys') || '').split(',').filter(Boolean).forEach(key);
}

(async () => {
  try {
    await pafLoad();
  } catch (e) {
    document.getElementById('log').textContent = String(e);
    return;
  }
  pushPage('');
  runScript();
  window.addEventListener('hashchange', () => { SIM.pages = []; SIM.overlay = null; pushPage(''); runScript(); });
  document.addEventListener('keydown', (ev) => { if (KEYS[ev.key]) { key(KEYS[ev.key]); ev.preventDefault(); } });
  document.getElementById('lang').onchange = (ev) => { PAF.lang = ev.target.value; };
  document.getElementById('bounds').onchange = (ev) => { PAF.showBounds = ev.target.checked; };
  frame();
})();
