import { defineCollection, z } from 'astro:content';
import { glob } from 'astro/loaders';

// The authoritative rules live in scripts/validate.mjs (it also checks cross-references); this schema gives the pages their types.
const track = z.enum(['c', 'dsa', 'linux', 'networking']);
const level = z.enum(['beginner', 'intermediate', 'advanced', 'expert']);
const shared = {
  title: z.string(), track, module: z.string(), moduleOrder: z.number(), order: z.number(), level, summary: z.string(),
  prerequisites: z.array(z.string()).default([]),
  sources: z.array(z.object({ label: z.string(), url: z.string().optional() })).default([]),
};

const notes = defineCollection({ loader: glob({ pattern: '**/*.md', base: './src/content/notes' }), schema: z.object(shared) });
const labs = defineCollection({ loader: glob({ pattern: '**/*.md', base: './src/content/labs' }), schema: z.object({ ...shared, minutes: z.number(), env: z.enum(['browser', 'machine']), terminal: z.array(z.string()).default([]) }) });

export const collections = { notes, labs };
