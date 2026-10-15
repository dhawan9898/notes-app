// Pure helpers for handing commands from lessons and labs to the Linux terminal page.
const MAX_COMMANDS = 30, MAX_LENGTH = 400;

/** The commands in a code block that shows a terminal session: lines starting with "$ " (output lines are skipped). */
export function commandsFromBlock(text: string): string[] {
  const out: string[] = [];
  for (const raw of text.split('\n')) {
    const m = raw.match(/^\$ (.+)$/);
    if (m) out.push(m[1].replace(/\s+#\s.*$/, '').trim());           // a trailing "# comment" is not part of the command
  }
  return out.filter(Boolean);
}

/** Query string for /practice/terminal/ carrying commands (each one a c= parameter). */
export const queueQuery = (commands: string[]) => '?' + commands.slice(0, MAX_COMMANDS).map((c) => `c=${encodeURIComponent(c)}`).join('&');

/** Commands from the page URL, bounded: the terminal only ever runs inside a throw-away virtual machine, but a link should not paste a novel. */
export function commandsFromQuery(search: string): string[] {
  return new URLSearchParams(search).getAll('c').map((c) => c.replace(/[\r\n]+/g, ' ').trim()).filter((c) => c && c.length <= MAX_LENGTH).slice(0, MAX_COMMANDS);
}

/** Bytes to send to the terminal for one key of the on-screen key row. */
export const KEY_CODES: Record<string, string> = { Esc: '\x1b', Tab: '\t', Up: '\x1b[A', Down: '\x1b[B', Right: '\x1b[C', Left: '\x1b[D', 'Ctrl-C': '\x03', 'Ctrl-D': '\x04', Home: '\x1b[H', End: '\x1b[F' };

/** Control-key combination for a single typed character ("c" -> ETX), or null when it has none. */
export function ctrlOf(ch: string): string | null {
  if (ch.length !== 1) return null;
  const c = ch.toUpperCase().charCodeAt(0);
  return c >= 64 && c <= 95 ? String.fromCharCode(c - 64) : null;
}
