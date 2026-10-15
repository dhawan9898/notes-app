export const flattenIncludes = (c: string) => c;
export async function filesFor() { return {}; }
let busy = false;
export const isBusy = () => busy;
export function run(source: string, opts: { onStatus?: (m: string) => void }) {
  busy = true; opts.onStatus?.('Compiling…');
  const promise = (window as any).__run(source).then((r: any) => { busy = false; return r; }, (e: any) => { busy = false; throw e; });
  return { promise, cancel: () => {} };
}
export const renderResult = () => {}; export const renderError = () => {};
