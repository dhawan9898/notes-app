// Shared, framework-free loaders for everything authored: lessons, labs, problems, paths, glossary.
// Used by scripts/validate.mjs and scripts/build-data.mjs (Astro reads the same files through its own content collections).
import fs from 'node:fs'; import path from 'node:path';
import * as yaml from 'js-yaml';

export const ROOT = path.resolve(new URL('../..', import.meta.url).pathname);
export const P = {
  notes: path.join(ROOT, 'src/content/notes'), labs: path.join(ROOT, 'src/content/labs'),
  practice: path.join(ROOT, 'authoring/practice'), examples: path.join(ROOT, 'authoring/examples'),
  data: path.join(ROOT, 'src/data'), generated: path.join(ROOT, 'src/data/generated'),
};
export const TRACKS = ['c', 'dsa', 'linux', 'networking'];
export const LEVELS = ['beginner', 'intermediate', 'advanced', 'expert'];

export const walk = (d) => (fs.existsSync(d) ? fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)])) : []);
export const rel = (f) => path.relative(ROOT, f);
export const stripNum = (s) => s.replace(/^\d+-/, '');

/** Splits "---\nyaml\n---\nbody". Throws a readable error for broken front matter. */
export function parseMd(file) {
  const text = fs.readFileSync(file, 'utf8');
  const m = text.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n?([\s\S]*)$/);
  if (!m) throw new Error(`${rel(file)}: missing front matter delimiters (---)`);
  let data;
  try { data = yaml.load(m[1]) ?? {}; } catch (e) { throw new Error(`${rel(file)}:${e.mark ? e.mark.line + 2 : '?'} ${e.reason || e.message}`); }
  return { data, body: m[2], text };
}

/** Lessons and labs share a shape; `base` is the collection root, id = path below it without the extension. */
function loadCollection(base, kind) {
  return walk(base).filter((f) => f.endsWith('.md')).sort().map((file) => {
    const { data, body } = parseMd(file);
    const parts = path.relative(base, file).replace(/\.md$/, '').split('/');
    const id = parts.join('/');
    const [track, moduleDir, name] = parts;
    return { kind, file, id, track, moduleDir, name, slug: stripNum(name), moduleSlug: stripNum(moduleDir), data, body };
  });
}
export const loadLessons = () => loadCollection(P.notes, 'lesson');
export const loadLabs = () => loadCollection(P.labs, 'lab');

export const readingMinutes = (body) => Math.max(1, Math.ceil(body.split(/\s+/).filter(Boolean).length / 220));

/** Modules derived from the lessons: one per (track, module title), ordered by moduleOrder. */
export function modulesOf(lessons) {
  const map = new Map();
  for (const l of lessons) {
    const k = `${l.track}|${l.data.module}`;
    if (!map.has(k)) map.set(k, { track: l.track, title: l.data.module, order: l.data.moduleOrder, slug: l.moduleSlug, lessons: [] });
    map.get(k).lessons.push(l);
  }
  return [...map.values()].sort((a, b) => TRACKS.indexOf(a.track) - TRACKS.indexOf(b.track) || a.order - b.order);
}

export function loadProblemFiles() {
  return walk(P.practice).filter((f) => f.endsWith('.md')).sort().map((file) => {
    const { data, body } = parseMd(file);
    const [track, moduleDir, name] = path.relative(P.practice, file).replace(/\.md$/, '').split('/');
    const [statement, ...rest] = body.split(/^## Explanation\s*$/m);
    return { file, track, moduleDir, name, slug: stripNum(name), data, statement: statement.trim(), explanation: rest.join('\n').trim() };
  });
}

export const loadJson = (name) => JSON.parse(fs.readFileSync(path.join(P.data, name), 'utf8'));
export const loadPaths = () => loadJson('paths.json');
export const loadGlossary = () => loadJson('glossary.json');
