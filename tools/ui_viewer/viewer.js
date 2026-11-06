// PAF layout and drawing, enough to see File Manager's screens on a PC.
// Layout rules (confirmed against ScePaf-RE widget.h and the firmware's own RCOs):
//   origin at the parent's center, Y up
//   anchor = reference point on the parent, align = reference point on the child
//   x: 0 center, 1 left, 2 right   y: 0 center, 1 bottom, 2 top
'use strict';

const PAF = {
  app: null, common: null, strings: null,
  ids: new Map(),        // id -> element (styles, templates, pages, textures)
  images: new Map(),     // png name -> HTMLImageElement
  lang: 'pt-br',
  showBounds: false,
};

async function pafLoad() {
  const get = (f) => fetch('cache/' + f, { cache: 'no-store' }).then((r) => {
    if (!r.ok) throw new Error(f + ': run tools/ui_viewer/export.py first');
    return r.json();
  });
  [PAF.app, PAF.common, PAF.strings] = await Promise.all([get('app.json'), get('common.json'), get('strings.json')]);
  const index = (n) => {
    if (n.a && n.a.id && typeof n.a.id === 'string' && n.tag !== 'locale') {
      // our resource wins over the common one when both define an id
      if (!PAF.ids.has(n.a.id) || n === PAF.app) PAF.ids.set(n.a.id, n);
    }
    n.c.forEach(index);
  };
  index(PAF.app);
  const appIds = new Map(PAF.ids);
  index(PAF.common);
  appIds.forEach((v, k) => PAF.ids.set(k, v));

  const pngs = new Set();
  const collect = (n) => { if (n.tag === 'texture' && n.a.src) pngs.add(n.a.src); n.c.forEach(collect); };
  collect(PAF.app); collect(PAF.common);
  await Promise.all([...pngs].map((p) => new Promise((ok) => {
    const img = new Image();
    img.onload = () => { PAF.images.set(p, img); ok(); };
    img.onerror = () => ok();
    img.src = 'cache/tex/' + p;
  })));
}

function pafString(id) {
  const t = PAF.strings[PAF.lang] || PAF.strings.en || {};
  return t[id] !== undefined ? t[id] : id;
}

function pafTexture(id) {
  const t = PAF.ids.get(id);
  if (!t || t.tag !== 'texture') return null;
  const img = PAF.images.get(t.a.src);
  return img ? { img, a: t.a } : null;
}

// Child of a style by its id (planeobj / textobj).
function styleObj(style, id) {
  if (!style || !id) return null;
  return style.c.find((c) => c.a.id === id) || null;
}

function rgba(c, alphaMul = 1) {
  const v = c || [1, 1, 1, 1];
  return `rgba(${Math.round(v[0] * 255)},${Math.round(v[1] * 255)},${Math.round(v[2] * 255)},${(v[3] === undefined ? 1 : v[3]) * alphaMul})`;
}

// ------------------------------------------------------------------ layout

class W {
  constructor(node, parent) {
    this.node = node;
    this.tag = node.tag;
    this.id = node.a.id;
    this.parent = parent;
    this.children = [];
    const lh = node.c.find((c) => c.tag === 'layout_hint');
    const h = lh ? lh.a : {};
    this.size = h.size ? [h.size[0], h.size[1]] : null;
    this.pos = h.pos ? [h.pos[0], h.pos[1]] : [0, 0];
    this.anchor = h.anchor || [0, 0];
    this.align = h.align || [0, 0];
    this.alpha = h.alpha !== undefined ? h.alpha : 1;
    this.style = node.a.style ? PAF.ids.get(node.a.style) : null;
    this.texture = node.a.texture || null;
    this.text = null;
    this.color = null;      // runtime SetColor
    this.hidden = false;
    this.focused = false;
    this.marked = false;
    this.listDraw = null;   // custom drawing for list views (cells come from the simulation)
    for (const c of node.c) {
      if (['layout_hint', 'focus_hint', 'layout_box'].includes(c.tag)) continue;
      this.children.push(new W(c, this));
    }
    this.box = node.c.find((c) => c.tag === 'layout_box');
  }

  find(id) {
    if (this.id === id) return this;
    for (const c of this.children) { const r = c.find(id); if (r) return r; }
    return null;
  }

  // Screen rectangle {x, y, w, h} given the parent's.
  layout(pr, override) {
    const size = this.size && (this.size[0] || this.size[1]) ? this.size : [pr.w, pr.h];
    const w = size[0] || pr.w, h = size[1] || pr.h;
    const ref = (axis, v, len) => (v === 0 ? 0 : (v === 1 ? -len / 2 : len / 2)) * (axis ? 1 : 1);
    // anchor point on parent (center origin, y up)
    const ax = ref(0, this.anchor[0], pr.w), ay = ref(1, this.anchor[1], pr.h);
    // align point on child relative to its center
    const cx = ref(0, this.align[0], w), cy = ref(1, this.align[1], h);
    const p = override || this.pos;
    const centerX = ax + p[0] - cx, centerY = ay + p[1] - cy; // relative to parent center, y up
    const pcx = pr.x + pr.w / 2, pcy = pr.y + pr.h / 2;
    this.rect = { x: pcx + centerX - w / 2, y: pcy - centerY - h / 2, w, h };
    return this.rect;
  }
}

function buildTree(id) {
  const node = PAF.ids.get(id);
  if (!node) throw new Error('missing ' + id);
  const root = new W(node, null);
  // template_ref expands in place
  const expand = (w) => {
    w.children = w.children.flatMap((c) => {
      if (c.tag === 'template_ref') {
        const t = PAF.ids.get(c.node.a.template);
        return t ? t.c.map((n) => { const x = new W(n, w); expand(x); return x; }) : [];
      }
      expand(c);
      return [c];
    });
  };
  expand(root);
  return root;
}

function instantiate(templateId, parent) {
  const t = PAF.ids.get(templateId);
  return t.c.map((n) => new W(n, parent));
}

// ------------------------------------------------------------------ drawing

function drawImage9(ctx, tex, r, alphaMul, tint) {
  const { img, a } = tex;
  ctx.save();
  ctx.globalAlpha *= alphaMul;
  const mh = a.margin_h || [0, 0], mv = a.margin_v || [0, 0];
  const mode = a.div_mode || '';
  let src = img;
  if (tint && (tint[0] !== 1 || tint[1] !== 1 || tint[2] !== 1)) src = tinted(img, tint);
  if (mode === '3x3' || mode === '3x1') {
    // nine-slice with the texture's own margins; corners keep their pixel size
    const sx = [0, img.width * (a.div_ratio_h ? a.div_ratio_h[0] : 0.49)];
    const L = Math.min(Math.max(mh[0], img.width * 0.2), img.width / 2), R = L;
    const T = mode === '3x3' ? Math.min(Math.max(mv[0], img.height * 0.2), img.height / 2) : 0, B = T;
    const sxs = [0, L, img.width - R, img.width], sys = mode === '3x3' ? [0, T, img.height - B, img.height] : [0, 0, img.height, img.height];
    const dxs = [r.x, r.x + Math.min(L, r.w / 2), r.x + r.w - Math.min(R, r.w / 2), r.x + r.w];
    const dys = mode === '3x3' ? [r.y, r.y + Math.min(T, r.h / 2), r.y + r.h - Math.min(B, r.h / 2), r.y + r.h] : [r.y, r.y, r.y + r.h, r.y + r.h];
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) {
      const sw = sxs[i + 1] - sxs[i], sh = sys[j + 1] - sys[j], dw = dxs[i + 1] - dxs[i], dh = dys[j + 1] - dys[j];
      if (sw > 0 && sh > 0 && dw > 0 && dh > 0) ctx.drawImage(src, sxs[i], sys[j], sw, sh, dxs[i], dys[j], dw, dh);
    }
  } else {
    ctx.drawImage(src, r.x, r.y, r.w, r.h);
  }
  ctx.restore();
}

const tintCache = new Map();
function tinted(img, c) {
  const key = img.src + c.join(',');
  if (tintCache.has(key)) return tintCache.get(key);
  const cv = document.createElement('canvas');
  cv.width = img.width; cv.height = img.height;
  const x = cv.getContext('2d');
  x.drawImage(img, 0, 0);
  x.globalCompositeOperation = 'multiply';
  x.fillStyle = rgba([c[0], c[1], c[2], 1]);
  x.fillRect(0, 0, cv.width, cv.height);
  x.globalCompositeOperation = 'destination-in';
  x.drawImage(img, 0, 0);
  tintCache.set(key, cv);
  return cv;
}

function drawText(ctx, text, r, tobj, opt = {}) {
  if (!text) return;
  const a = (tobj && tobj.a) || {};
  const size = a.font_size || 28;
  ctx.save();
  ctx.font = `${a.bold ? '600 ' : ''}${size}px "Segoe UI", system-ui, sans-serif`;
  ctx.fillStyle = rgba(opt.color || a.font_color || [1, 1, 1, 1], opt.alpha || 1);
  ctx.textBaseline = 'middle';
  const alignX = opt.alignX !== undefined ? opt.alignX : (a.align_x || 0);
  let x = r.x + (opt.padL || 0);
  const w = r.w - (opt.padL || 0) - (opt.padR || 0);
  ctx.textAlign = 'left';
  let t = String(text);
  if (ctx.measureText(t).width > w) {
    while (t.length > 1 && ctx.measureText(t + '…').width > w) t = t.slice(0, -1);
    t += '…';
  }
  const tw = ctx.measureText(t).width;
  // as seen on the console: 0 left, 1 center, 2 right
  if (alignX === 2) x = x + w - tw;
  else if (alignX === 1 || alignX === 3 || opt.center) x = x + (w - tw) / 2;
  ctx.fillText(t, x, r.y + r.h / 2);
  ctx.restore();
}

function drawPlaneObj(ctx, style, objId, r, alphaMul, override) {
  const o = styleObj(style, objId);
  if (!o) return;
  const color = (override && override.color) || o.a.color || [1, 1, 1, 1];
  const texId = (override && override.texture) || o.a.texture0;
  let rr = r;
  if (o.a.size && !(override && override.fill)) {
    const p = o.a.pos || [0, 0];
    rr = { x: r.x + r.w / 2 - o.a.size[0] / 2 + p[0], y: r.y + r.h / 2 - o.a.size[1] / 2 - p[1], w: o.a.size[0], h: o.a.size[1] };
  }
  const tex = texId ? pafTexture(texId) : null;
  if (tex) drawImage9(ctx, tex, rr, alphaMul * (color[3] === undefined ? 1 : color[3]), color);
  else if (!texId) {
    ctx.fillStyle = rgba(color, alphaMul);
    ctx.fillRect(rr.x, rr.y, rr.w, rr.h);
  }
}

function drawWidget(ctx, w, pr, alpha) {
  if (w.hidden) return;
  const r = w.layout(pr, w.posOverride);
  const a = alpha * w.alpha * (w.color && w.color[3] !== undefined ? w.color[3] : 1);
  const st = w.style;
  const sa = st ? st.a : {};
  const styleAlpha = sa.color && sa.color[3] !== undefined ? sa.color[3] : 1;

  switch (w.tag) {
    case 'plane': {
      const tex = w.texture ? pafTexture(w.texture) : null;
      if (st && st.tag === 'style_plane' && !w.texture) {
        // A style_plane only draws through its planeobj; a bare color draws nothing.
        if (sa.planeobj) {
          ctx.fillStyle = rgba(w.color || sa.color, alpha * w.alpha);
          ctx.fillRect(r.x, r.y, r.w, r.h);
        } else if (PAF.showBounds) {
          ctx.strokeStyle = '#ff4d4d'; ctx.strokeRect(r.x, r.y, r.w, r.h);
        }
      } else if (tex) {
        drawImage9(ctx, tex, r, a, w.color ? w.color.slice(0, 3) : null);
      } else if (w.surface) {
        ctx.save(); ctx.globalAlpha = a; ctx.drawImage(w.surface, r.x, r.y, r.w, r.h); ctx.restore();
      } else if (!st) {
        // a plane with neither style nor texture is white
        ctx.fillStyle = rgba(w.color || [1, 1, 1, 1], a);
        ctx.fillRect(r.x, r.y, r.w, r.h);
      }
      break;
    }
    case 'text':
    case 'text_box': {
      if (w.tag === 'text_box' && st) drawPlaneObj(ctx, st, st.a.planeobj, r, a, { fill: true });
      const tobj = st ? styleObj(st, st.a.textobj) : null;
      drawText(ctx, w.text, r, tobj, { alpha: a, color: w.color, padL: w.tag === 'text_box' ? 16 : 0 });
      break;
    }
    case 'button':
    case 'image_button':
    case 'corner_button': {
      if (!st) break;
      const isCorner = w.tag === 'corner_button';
      if (isCorner) {
        // corner buttons sit in a screen corner whatever their layout says
        const left = sa.layout_type === 2 || sa.layout_type === 0;
        const bg = pafTexture(styleObj(st, sa.bg_obj) ? styleObj(st, sa.bg_obj).a.texture0 : null);
        const bw = bg ? bg.img.width : 120, bh = bg ? bg.img.height : 120;
        const rr = { x: left ? 0 : 960 - bw, y: 544 - bh, w: bw, h: bh };
        w.rect = rr;
        ctx.save();
        if (!left) { ctx.translate(960, 0); ctx.scale(-1, 1); rr.x = 0; }
        drawPlaneObj(ctx, st, sa.bg_obj, rr, a, { fill: true });
        ctx.restore();
        const rr2 = { x: left ? 0 : 960 - bw, y: 544 - bh, w: bw, h: bh };
        drawPlaneObj(ctx, st, sa.corner_obj, rr2, a);
        if (w.focused) drawPlaneObj(ctx, st, sa.glow_obj, rr2, a, { fill: true });
        break;
      }
      drawPlaneObj(ctx, st, sa.bg_obj, r, a * styleAlpha, { fill: true });
      if (w.focused && sa.glow_obj) drawPlaneObj(ctx, st, sa.glow_obj, r, a, { fill: true });
      let padL = sa.left_margin || 0;
      if (w.tag === 'image_button' && w.image) {
        const io = styleObj(st, sa.image_obj);
        const isz = io && io.a.size ? io.a.size : [70, 70];
        const margin = sa.image_margin || 10;
        const ih = Math.min(isz[1], r.h);
        const iw = Math.min(isz[0], ih * w.image.width / w.image.height);
        ctx.save(); ctx.globalAlpha = a;
        ctx.drawImage(w.image, r.x + margin + (isz[0] - iw) / 2, r.y + (r.h - ih) / 2, iw, ih);
        ctx.restore();
        padL = margin * 2 + isz[0];
      }
      const lo = styleObj(st, sa.label_obj);
      drawText(ctx, w.text, r, lo, { alpha: a, padL, padR: sa.right_margin || 0, center: w.tag === 'button' });
      break;
    }
    case 'busyindicator': {
      if (!w.spinning) break;
      const t = performance.now() / 120;
      for (let i = 0; i < 8; i++) {
        const ang = i / 8 * Math.PI * 2;
        ctx.fillStyle = rgba([0.78, 0.8, 0.85, ((i + Math.floor(t)) % 8) / 8]);
        ctx.beginPath();
        ctx.arc(r.x + r.w / 2 + Math.cos(ang) * 24, r.y + r.h / 2 + Math.sin(ang) * 24, 6, 0, 7);
        ctx.fill();
      }
      break;
    }
    case 'list_view':
      if (w.listDraw) w.listDraw(ctx, r, a);
      break;
    default:
      break;
  }

  if (PAF.showBounds && w.tag !== 'list_view') {
    ctx.strokeStyle = 'rgba(255,220,0,0.35)';
    ctx.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
  }
  if (w.tag !== 'corner_button') for (const c of w.children) drawWidget(ctx, c, r, a);
}

// Stacks children of a box (layout_box layout_type 1 = vertical, 2 = horizontal).
function layoutBox(box, items) {
  let y = 0;
  for (const it of items) { it.posOverride = [0, -y]; it.anchor = [1, 2]; it.align = [1, 2]; y += it.size[1]; }
  return y;
}

const SCREEN = { x: 0, y: 0, w: 960, h: 544 };
