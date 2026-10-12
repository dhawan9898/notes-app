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
    level: z.enum(['beginner', 'intermediate', 'advanced', 'expert']),
    summary: z.string(),
    prerequisites: z.array(z.string()).default([]),   // titles of notes (or plain phrases) the reader should know first
    sources: z.array(z.object({ label: z.string(), url: z.string().optional() })).default([]),
  }),
});

export const collections = { notes };
