#include "vagdec.h"
#pragma warning(disable : 4996)  

#include <vector>
#include <algorithm>

class VBStream
{
public:
	static const size_t VB_BLOCK_SIZE = 512 * 16 * 2;

	VBStream( FILE* file, int sample_rate, int stereo, int size )
		: stream( file ), fileSize( size )
	{
		memset( &virtualHeader, 0, sizeof(virtualHeader) );
		virtualHeader.VAG[0] = 'V';
		virtualHeader.VAG[1] = 'A';
		virtualHeader.VAG[2] = 'G';
		virtualHeader.field_3 = 'p';

		virtualHeader.sample_rate = _byteswap_ulong(sample_rate);
		virtualHeader.stereo = _byteswap_ulong(stereo);
		virtualHeader.size = _byteswap_ulong(size);

		blockBuffer = stereo != 0 ? (U8*)AIL_mem_alloc_lock( VB_BLOCK_SIZE ) : nullptr;
	}

	VBStream( const VBStream& ) = delete;
	VBStream& operator = ( const VBStream& rhs ) = delete;

	VBStream( VBStream&& rhs ) = default;
	VBStream& operator = ( VBStream&& rhs ) = default;

	~VBStream()
	{
		if ( blockBuffer != nullptr )
		{
			AIL_mem_free_lock( blockBuffer );
			blockBuffer = nullptr;
		}
	}

	bool UsesInterleave() const { return blockBuffer != nullptr; }

	FILE* GetStream() const { return stream; }

	U32 ReadFile( void FAR* Buffer, U32 Bytes )
	{
		if ( virtualCursor == 0 )
		{
			VAG_HEADER* header = (VAG_HEADER*)Buffer;
			*header = virtualHeader;

			size_t bytesRead = sizeof(VAG_HEADER);
			if ( !UsesInterleave() )
			{
				bytesRead += fread( &header[1], 1, Bytes-sizeof(*header), stream );
			}
			else
			{
				ReadBlockAndInterleave();

				memcpy( &header[1], blockBuffer, Bytes-sizeof(*header) );
				bytesRead += Bytes-sizeof(*header);
			}
			virtualCursor += bytesRead;
			return bytesRead;
		}

		if ( currentBlockCursor + Bytes >= VB_BLOCK_SIZE )
		{
			size_t firstRead = VB_BLOCK_SIZE - currentBlockCursor;

			memcpy( Buffer, blockBuffer+currentBlockCursor, firstRead );
			ReadBlockAndInterleave();
			memcpy( (U8*)Buffer+firstRead, blockBuffer, Bytes-firstRead );

			currentBlockCursor += Bytes-firstRead;
		}
		else
		{
			memcpy( Buffer, blockBuffer+currentBlockCursor, Bytes );
			currentBlockCursor += Bytes;
		}
		virtualCursor += Bytes;
		return Bytes;
	}

	S32 SeekFile( S32 Offset, U32 Type )
	{
		if ( Type == AIL_FILE_SEEK_CURRENT ) Offset += virtualCursor;
		else if ( Type == AIL_FILE_SEEK_END ) Offset = fileSize - Offset;

		S32 OffsetNoHeader = Offset - sizeof(VAG_HEADER);
		S32 CursorNoHeader = virtualCursor - sizeof(VAG_HEADER);

		if ( OffsetNoHeader <= 0 )
		{
			// Seeks to header - do nothing since next ReadFile flushes everything anyway
			fseek( stream, 0, SEEK_SET );
			virtualCursor = 0;
			return 0;
		}

		if ( UsesInterleave() )
		{
			// Is within the same block?
			if ( GET_BLOCK(OffsetNoHeader) == GET_BLOCK(CursorNoHeader) )
			{
				// Do nothing, just move cursor
				currentBlockCursor = OffsetNoHeader - CursorNoHeader;
			}
			else
			{
				// Read new block
				fseek( stream, GET_BLOCK(OffsetNoHeader) * VB_BLOCK_SIZE, SEEK_SET );
				ReadBlockAndInterleave();
				currentBlockCursor = OffsetNoHeader % VB_BLOCK_SIZE;
			}
		}
		else
		{
			fseek( stream, OffsetNoHeader, SEEK_SET );
		}
		virtualCursor = Offset;
		return virtualCursor;
	}

private:
	void ReadBlockAndInterleave( )
	{
		const size_t VB_SAMPLE_SIZE = 0x10;

		U8 buf[VB_BLOCK_SIZE];
		fread( buf, 1, VB_BLOCK_SIZE, stream );

		// Interleave
		U8* leftChannel = buf;
		U8* rightChannel = buf+(VB_BLOCK_SIZE/2);
		U8* output = blockBuffer;

		for ( int i = 0; i < 512; ++i )
		{
			memcpy( output, leftChannel, VB_SAMPLE_SIZE );
			memcpy( output+VB_SAMPLE_SIZE, rightChannel, VB_SAMPLE_SIZE );
			leftChannel += VB_SAMPLE_SIZE;
			rightChannel += VB_SAMPLE_SIZE;
			output += 2*VB_SAMPLE_SIZE;
		}

		currentBlockCursor = 0;
	}

private:
	FILE* stream;
	VAG_HEADER virtualHeader;
	U8* blockBuffer;
	S32 currentBlockCursor = 0, virtualCursor = 0;
	S32 fileSize;
};

static std::vector<FILE*> adfStreams;
static std::vector<VBStream> vbStreams;

static void XORADFStream( void FAR* Buffer, size_t size )
{
	U8* buf = (U8*)Buffer;

	while ( size != 0 && ((U32)buf & 3) != 0 )
	{
		*buf ++ ^= 0x22;
		--size;
	}

	while ( size >= 4 )
	{
		*(U32*)buf ^= 0x22222222;
		buf += 4;
		size -= 4;
	}

	while ( size != 0 )
	{
		*buf ++ ^= 0x22;
		--size;
	}
}

static U32 AILCALLBACK FAR OpenFileCB(MSS_FILE const FAR* Filename, U32 FAR* FileHandle)
{
	FILE* hFile = fopen( Filename, "rb" );
	*FileHandle = (U32)hFile;

	if ( hFile == nullptr ) return 0;

	size_t length = strlen(Filename);
	if ( Filename[length-4] == '.' &&
		( Filename[length-3] == 'a' || Filename[length-3] == 'A' ) &&
		( Filename[length-2] == 'd' || Filename[length-2] == 'D' ) &&
		( Filename[length-1] == 'f' || Filename[length-1] == 'F' )
		)
	{
		MSS_FILE FAR* name = const_cast<MSS_FILE FAR*>(Filename);
		name[length-3] = 'm';
		name[length-2] = 'p';
		name[length-1] = '3';

		adfStreams.push_back( hFile );
	}
	else if ( Filename[length-3] == '.' &&
		( Filename[length-2] == 'v' || Filename[length-2] == 'V' ) &&
		( Filename[length-1] == 'b' || Filename[length-1] == 'B' )
		)
	{
		MSS_FILE FAR* name = const_cast<MSS_FILE FAR*>(Filename);
		name[length-2] = 'v';
		name[length-1] = 'a';
		name[length] = 'g';
		name[length+1] = '\0';

		fseek( hFile, 0, SEEK_END );
		int size = ftell( hFile );
		fseek( hFile, 0, SEEK_SET );
		vbStreams.emplace_back( hFile, 32000, 1, size );
	}

	return (U32)hFile;
}

static void AILCALLBACK FAR CloseFileCB(U32 FileHandle)
{
	FILE* hFile = (FILE*)FileHandle;
	{
		auto it = std::find( adfStreams.begin(), adfStreams.end(), hFile );
		if ( it != adfStreams.end() )
		{
			adfStreams.erase( it );
			fclose( hFile );
			return;
		}
	}
	{
		auto it = std::find_if( vbStreams.begin(), vbStreams.end(), [hFile](const VBStream& s) {
			return s.GetStream() == hFile;
			} );
		if ( it != vbStreams.end() )
		{
			vbStreams.erase( it );
			fclose( hFile );
			return;
		}

	}
	fclose( hFile );
}

static S32 AILCALLBACK FAR SeekFileCB(U32 FileHandle, S32 Offset, U32 Type)
{
	FILE* hFile = (FILE*)FileHandle;
	auto it = std::find_if( vbStreams.begin(), vbStreams.end(), [hFile](const VBStream& s) {
		return s.GetStream() == hFile;
	} );
	if ( it != vbStreams.end() )
	{
		return it->SeekFile( Offset, Type );
	}

	fseek( hFile, Offset, Type );
	return ftell( hFile );
}

static U32 AILCALLBACK FAR ReadFileCB(U32 FileHandle, void FAR* Buffer, U32 Bytes)
{
	FILE* hFile = (FILE*)FileHandle;
	{
		auto it = std::find( adfStreams.begin(), adfStreams.end(), hFile );
		if ( it != adfStreams.end() )
		{
			size_t bytesRead = fread( Buffer, 1, Bytes, hFile );
			XORADFStream( Buffer, bytesRead );
			return bytesRead;
		}
	}
	{
		auto it = std::find_if( vbStreams.begin(), vbStreams.end(), [hFile](const VBStream& s) {
			return s.GetStream() == hFile;
		} );
		if ( it != vbStreams.end() )
		{
			return it->ReadFile( Buffer, Bytes );
		}
	}
	size_t bytesRead = fread( Buffer, 1, Bytes, hFile );
	return bytesRead;
}

void AttachVBCallbacks()
{
	AIL_set_file_callbacks( OpenFileCB, CloseFileCB, SeekFileCB, ReadFileCB );
}