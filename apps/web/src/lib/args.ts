export function parseArgs(raw: string): string[] {
  return raw
    .split('\n')
    .map((part) => part.trim())
    .filter((part) => part.length > 0);
}
