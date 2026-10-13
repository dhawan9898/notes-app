// Renders the Android launcher icons from a simple vector glyph (run once; output is committed with the Android project).
import sharp from 'sharp'; import fs from 'node:fs'; import path from 'node:path';
const RES = new URL('../android/app/src/main/res/', import.meta.url).pathname;
const glyph = (size, pad) => `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 512 512"><g transform="translate(${256 - 256 * pad} ${256 - 256 * pad}) scale(${pad})"><rect x="120" y="96" width="272" height="320" rx="44" fill="#ffffff" fill-opacity="0.16" stroke="#ffffff" stroke-opacity="0.85" stroke-width="10"/><path d="M178 184h156M178 256h156M178 328h96" stroke="#ffffff" stroke-width="24" stroke-linecap="round"/><circle cx="352" cy="328" r="10" fill="#6cb0ff"/></g></svg>`;
const bg = (size, round) => `<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}"><defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#2b6bff"/><stop offset="1" stop-color="#5b2bd6"/></linearGradient></defs><rect width="${size}" height="${size}" rx="${round ? size / 2 : size * 0.22}" fill="url(#g)"/></svg>`;
const dens = { mdpi: 1, hdpi: 1.5, xhdpi: 2, xxhdpi: 3, xxxhdpi: 4 };
for (const [d, k] of Object.entries(dens)) {
  const dir = path.join(RES, `mipmap-${d}`);
  const s = Math.round(48 * k), f = Math.round(108 * k);
  for (const round of [false, true]) {
    const base = await sharp(Buffer.from(bg(s, round))).png().toBuffer();
    const g = await sharp(Buffer.from(glyph(s, 0.78))).png().toBuffer();
    await sharp(base).composite([{ input: g }]).png().toFile(path.join(dir, round ? 'ic_launcher_round.png' : 'ic_launcher.png'));
  }
  await sharp(Buffer.from(glyph(f, 0.62))).png().toFile(path.join(dir, 'ic_launcher_foreground.png'));
}
fs.writeFileSync(path.join(RES, 'values/ic_launcher_background.xml'), '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <color name="ic_launcher_background">#2F4FE0</color>\n</resources>\n');
// the adaptive icon should use the PNG foreground, not the template's vector drawable
for (const f of ['ic_launcher.xml', 'ic_launcher_round.xml']) {
  fs.writeFileSync(path.join(RES, 'mipmap-anydpi-v26', f), '<?xml version="1.0" encoding="utf-8"?>\n<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n    <background android:drawable="@color/ic_launcher_background"/>\n    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>\n</adaptive-icon>\n');
}
console.log('icons written');
