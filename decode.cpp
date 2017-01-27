// SOURCE:
// http://www.tinyted.net/eddie/decode.html

struct VAGState
{
	VAGState()		{ Reset(); }

	void	Reset()	{ s[0] = s[1] = 0; }

	double	s[2];
};

void DecodeVAGBlock(const unsigned char in[16], short out[28], VAGState* state);

const double filter[5][2] =
{
	{ 0.0, 0.0 },
	{ 60.0 / 64.0,  0.0 },
	{ 115.0 / 64.0, -52.0 / 64.0 },
	{ 98.0 / 64.0, -55.0 / 64.0 },
	{ 122.0 / 64.0, -60.0 / 64.0 }
};

const unsigned char	wavhdr[44] =
{
	'R','I','F','F',
	0,0,0,0,			// length of file - 8
	'W','A','V','E',

	'f','m','t',' ',
	0x10,0,0,0,
	1,0,2,0,
	0,0,0,0,			// sample rate
	0,0xF4,1,0,
	4,0,0x10,0,

	'd','a','t','a',
	0,0,0,0,			// length of file - 0x2C
};

int main(int argc, char *argv[])
{
	int		rate = 32000;

	argc--;
	argv++;

	while (argc && (argv[0][0] == '-'))
	{
		char	parm = argv[0][1];

		if (parm == 'r')
		{
			if (argv[0][2])
			{
				rate = atoi(argv[0] + 2);
			}
			else
			{
				argc--;
				argv++;
				rate = atoi(argv[0]);
			}

			printf("Sample rate : %d\n", rate);
		}

		argc--;
		argv++;
	}

	char	src[256];
	char	dst[256];

	if (argc == 1)
	{
		sprintf(src, "%s.VB", argv[0]);
		sprintf(dst, "%s.WAV", argv[0]);
	}
	else if (argc == 2)
	{
		strcpy(src, argv[0]);
		strcpy(dst, argv[1]);
	}
	else
	{
		printf("Usage: Decode [-r <rate>] <in>|<root> [<out>]\n");
		return -1;
	}
	
	FILE*	vag = fopen(src, "rb");
	if (!vag)
	{
		printf("Can't open source file %s\n", src);
		return -1;
	}

	// find number of blocks
	fseek(vag, 0, SEEK_END);
	int	length = ftell(vag) / 16;
	fseek(vag, 0, SEEK_SET);

	length /= 1024;

	FILE*	wav = fopen(dst, "wb");
	if (!wav)
	{
		printf("Can't open destination file %s\n", dst);
		return -1;
	}

	printf("Length : %d blocks\n", length);

	unsigned char	hdr[44];
	int*			ihdr = (int*)hdr;

	memcpy(hdr, wavhdr, 44);
	ihdr[1] = length * 28 * 512 * 2 * 2 + 0x24;
	ihdr[10] = length * 28 * 512 * 2 * 2;
	ihdr[6] = rate;

	fwrite(hdr, 1, 44, wav);

	VAGState		st[2];
	unsigned char	bl[512 * 16], br[512 * 16];
	short			out[512 * 28 * 2];

	for (int blk = 0; blk < length; blk++)
	{
		fread(bl, 16, 512, vag);
		fread(br, 16, 512, vag);
	
		for (int ii = 0; ii < 512; ii++)
		{
			short	obuf[28];

			// decode left block
			DecodeVAGBlock(bl + ii * 16, obuf, &st[0]);
			for (int jj = 0; jj < 28; jj++)		out[(ii * 28 + jj) * 2] = obuf[jj];

			// decode right block
			DecodeVAGBlock(br + ii * 16, obuf, &st[1]);
			for (int jj = 0; jj < 28; jj++)		out[(ii * 28 + jj) * 2 + 1] = obuf[jj];
		}

		// write output
		fwrite(out, 2, 512 * 28 * 2, wav);
	}

	fclose(wav);
	fclose(vag);

	return 0;
}

int highnibble(int a)		{ return (a >> 4) & 15; }
int lownibble(int a)		{ return a & 15; }

short quantize(double sample)
{
	int a = int(sample + 0.5);
	
	if (a < -32768) return -32768; 
	if (a > 32767)	return 32767;

	return short(a);
}

void DecodeVAGBlock(const unsigned char in[16], short out[28], VAGState* state)
{
	double	s[2];

	s[0] = state->s[0];
	s[1] = state->s[1];

	int		predictor	= highnibble(in[0]);
	int		shift		= lownibble(in[0]);
	int		flags		= in[1];

	if (predictor > 4)
	{
		// this should never happen in a valid VAG block
		printf("Predictor %d!\n", predictor);
		predictor = 0;
	}

	for (int ii = 0; ii < 14; ii++)
	{
		int	byte = in[ii + 2];

		out[ii * 2]		= short(lownibble(byte) << 12)	>> shift;
		out[ii * 2 + 1]	= short(highnibble(byte) << 12)	>> shift;
	}

	for (int ii = 0; ii < 28; ii++)
	{
		double filt = out[ii] + s[0] * filter[predictor][0] + s[1] * filter[predictor][1];
		s[1] = s[0];
		s[0] = filt;

		out[ii] = quantize(filt);
	}

	state->s[0] = s[0];
	state->s[1] = s[1];
}
