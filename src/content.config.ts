import { defineCollection, z } from 'astro:content';
import { glob } from 'astro/loaders';

const notes = defineCollection({
  loader: glob({ pattern: '**/*.{md,mdx}', base: './src/content/notes' }),
  schema: z.object({
    title: z.string(),
    track: z.enum(['c', 'dsa', 'linux', 'networking']),
    module: z.string(),          // module title, e.g. "Layer 2"
    moduleOrder: z.number(),
    order: z.number(),           // order within module
    level: z.enum(['intermediate', 'advanced', 'expert']),
    summary: z.string(),
    sources: z.array(z.object({ label: z.string(), url: z.string().optional() })).default([]),
  }),
});

export const collections = { notes };
