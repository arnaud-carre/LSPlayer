/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

// Shrinkler API entry point. Only used to "estimate" the .lsmusic file size once packed with Schrinkler
// ( "-pack" command line option )

#include <assert.h>
#include "LSPTypes.h"
#include "external/Shrinkler/Pack.h"

#define NUM_RELOC_CONTEXTS 256

int	ShrinklerCompressEstimate(u8* data, int dataSize)
{
	vector<unsigned> pack_buffer;

#ifdef NDEBUG
	const int p = 9;					// -9 option
#else
	const int p = 1;					// -2 option
#endif
	printf("Estimating Amiga Shrinkler packing size... (preset -%d)\n", p);

	RangeCoder *range_coder = new RangeCoder(LZEncoder::NUM_CONTEXTS + NUM_RELOC_CONTEXTS, pack_buffer);

	// Crunch the data
	range_coder->reset();

	PackParams params;
	params.iterations = 1 * p;
	params.length_margin = 1 * p;
	params.skip_length = 1000 * p;
	params.match_patience = 100 * p;
	params.max_same_length = 10 * p;

	RefEdgeFactory edge_factory(100000);

	packData(data, dataSize, 0, &params, range_coder, &edge_factory, false);
	range_coder->finish();
	int packedSize = int(pack_buffer.size()) * 4;
	delete range_coder;
	return packedSize;
}
