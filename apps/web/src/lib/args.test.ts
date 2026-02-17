import { describe, expect, it } from 'vitest';

import { parseArgs } from './args';

describe('parseArgs', () => {
  it('splits newline-delimited values', () => {
    expect(parseArgs('one\ntwo\nthree')).toEqual(['one', 'two', 'three']);
  });

  it('trims whitespace and removes empty entries', () => {
    expect(parseArgs('  -y\n\n  @pkg/name  \n   \n.')).toEqual(['-y', '@pkg/name', '.']);
  });
});
