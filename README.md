# Light Speed Player
# The fastest Amiga music player ever

## What is LSP?

LSP (Light Speed Player) is a very fast Amiga music player. It can play any MOD file and *outperforms* all existing Amiga music player in speed. MOD file should be converted to LSP music format using a command line tool called LSPConvert

Here is a speed comparaison of different Amiga music players. Numbers are given in "scanlines", measured on basic Amiga 500 OCS (PAL). Lower is better. As a typical MOD player has some peaks, we used 3 values: minium time per frame, average time, and peak time. 

| Player | Minimum | Average | Peak | Avg speedup factor |
|------|-----|------|---|---|
| ptplayer 5.3 | 1.6 | 4.6 | 13 | x1 (ref) |
| P61 | 0.5 | 1.52 | 4.6 | x3 |
| LSP | 0.25 | 0.63 | 1.8 | x7.3 |
| LSP insane | 0.2 | 0.4 | 1 | x13 |

*Minimal and peak time are easy to measure on any emulator (just take screenshot and count scanlines). Average time is a bit harder to get. As average MOD music is processing row commands 1 vblank over 4, I used this forula: avg = (peak+3*min)/4. If anyone want to do more robust benchmark about "average" time, please share your numbers*

## Who would use LSP?

```c
bool	LSPEncoder::ExportBank(const char* sfilename)
{
	bool ret = false;
	FILE* h;
	if (0 == fopen_s(&h, sfilename, "wb"))
	{
		printf("Writing LSP bank: %s\n", sfilename);
		w32(h, m_uniqueId);
		fwrite(m_lspBank, 1, m_lspBankSize, h);
		fclose(h);
		ret = true;
	}
	return ret;
}
```
