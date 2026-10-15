// Vite turns `import x from 'file?url'` into the URL the built file will have.
declare module '*?url' { const url: string; export default url; }
