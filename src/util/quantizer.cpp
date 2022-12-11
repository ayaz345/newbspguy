// This is an independent project of an individual developer. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com


#include <stdint.h>
#include "quantizer.h"

unsigned char FixBounds( int i )
{
	if ( i > 0xFF )
		return 0xFF;
	else if ( i < 0x00 )
		return 0x00;
	return ( unsigned char )i;
}

unsigned char FixBounds( float i )
{
	if ( i >( double )0xFF )
		return 0xFF;
	else if ( i < ( double )0x00 )
		return 0x00;
	return ( unsigned char )i;
}

unsigned char FixBounds( double i )
{
	if ( i >( double )0xFF )
		return 0xFF;
	else if ( i < ( double )0x00 )
		return 0x00;
	return ( unsigned char )i;
}

Quantizer::Quantizer( unsigned int nMaxColors, unsigned int nColorBits )
{
	m_nColorBits = nColorBits < 8 ? nColorBits : 8;
	m_pTree = 0;
	m_nLeafCount = 0;
	m_lastIndex = 0;
	for ( int i = 0; i <= ( int )m_nColorBits; i++ )
		m_pReducibleNodes[ i ] = 0;
	m_nMaxColors = m_nOutputMaxColors = nMaxColors;
	if ( m_nMaxColors < 16 ) m_nMaxColors = 16;
}

Quantizer::~Quantizer( )
{
	if ( m_pTree )
		DeleteTree( &m_pTree );
}

bool Quantizer::ProcessImage(COLOR3 * image, unsigned long size )
{
	for ( unsigned long i = 0; i < size; i++ )
	{
		COLOR3 pix = image[i];
		AddColor( &m_pTree, pix.r, pix.g, pix.b, m_nColorBits, 0, &m_nLeafCount, m_pReducibleNodes );
	
		if ( m_nLeafCount > m_nMaxColors )
			ReduceTree( m_nColorBits, &m_nLeafCount, m_pReducibleNodes );
	}
	while ( m_nLeafCount > m_nMaxColors )
		ReduceTree( m_nColorBits, &m_nLeafCount, m_pReducibleNodes );
	return true;
}

void Quantizer::ApplyColorTable(COLOR3* image, unsigned int size, COLOR3* pal)
{
	for (unsigned int i = 0; i < size; i++)
	{
		image[i] = pal[GetNearestIndex(image[i], pal)];
	}
}
void Quantizer::FloydSteinbergDither( unsigned char* image, long width, long height, unsigned char* target, COLOR3* pal )
{
	int bytespp = 3;
	for ( long y = 0; y < height; y++ )
	{
		if ( y % 2 == 1 )
		{
			for ( long x = 0; x < width; x++ )
			{
				int i = width * ( height - y - 1 ) + x;
				int j = ( width * y + x ) * bytespp;
				unsigned char k = GetNearestIndexFast( *( COLOR3* )( image + j ), pal );
				int diff[ 3 ];
				target[ i ] = k;
				diff[ 0 ] = image[ j ] - pal[ k ].r;
				diff[ 1 ] = image[ j + 1 ] - pal[ k ].g;
				diff[ 2 ] = image[ j + 2 ] - pal[ k ].b;

				if ( y < height - 1 )
				{
					for ( int l = 0; l < 3; l++ )
						image[ j + ( width * bytespp ) + l ] = FixBounds( image[ j + ( width * bytespp ) + l ] + ( diff[ l ] * 5 ) / 16 );
					if ( x > 0 )
						for ( int l = 0; l < 3; l++ )
							image[ j + ( ( width - 1 ) * bytespp ) + l ] = FixBounds( image[ j + ( ( width - 1 ) * bytespp ) + l ] + ( diff[ l ] * 3 ) / 16 );
					if ( x < width - 1 )
						for ( int l = 0; l < 3; l++ )
							image[ j + ( ( width + 1 ) * bytespp ) + l ] = FixBounds( image[ j + ( ( width + 1 ) * bytespp ) + l ] + ( diff[ l ] * 1 ) / 16 );
				}
				if ( x < width - 1 )
					for ( int l = 0; l < 3; l++ )
						image[ j + bytespp + l ] = FixBounds( image[ j + bytespp + l ] + ( diff[ l ] * 7 ) / 16 );

			}
		}
		else
		{
			for ( long x = width - 1; x >= 0; x-- )
			{
				int i = width * ( height - y - 1 ) + x;
				int j = ( width * y + x ) * bytespp;
				unsigned char k = GetNearestIndexFast( *( COLOR3* )( image + j ), pal );
				int diff[ 3 ];
				target[ i ] = k;
				diff[ 0 ] = image[ j ] - pal[ k ].r;
				diff[ 1 ] = image[ j + 1 ] - pal[ k ].g;
				diff[ 2 ] = image[ j + 2 ] - pal[ k ].b;

				if ( y < height - 1 )
				{
					for ( int l = 0; l < 3; l++ )
						image[ j + ( width * bytespp ) + l ] = FixBounds( image[ j + ( width * bytespp ) + l ] + ( diff[ l ] * 5 ) / 16 );
					if ( x > 0 )
						for ( int l = 0; l < 3; l++ )
							image[ j + ( ( width - 1 ) * bytespp ) + l ] = FixBounds( image[ j + ( ( width - 1 ) * bytespp ) + l ] + ( diff[ l ] * 1 ) / 16 );
					if ( x < width - 1 )
						for ( int l = 0; l < 3; l++ )
							image[ j + ( ( width + 1 ) * bytespp ) + l ] = FixBounds( image[ j + ( ( width + 1 ) * bytespp ) + l ] + ( diff[ l ] * 3 ) / 16 );
				}
				if ( x > 0 )
					for ( int l = 0; l < 3; l++ )
						image[ j - bytespp + l ] = FixBounds( image[ j - bytespp + l ] + ( diff[ l ] * 7 ) / 16 );

			}
		}
	}
}

void Quantizer::AddColor( Node** ppNode, unsigned char r, unsigned char g, unsigned char b,
	unsigned int nColorBits, int nLevel, unsigned int*	pLeafCount, Node** pReducibleNodes )
{
	static unsigned char mask[ 8 ] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
	if ( !( *ppNode ) )
		*ppNode = ( Node* )CreateNode( nLevel, nColorBits, pLeafCount, pReducibleNodes );
	if ( ( *ppNode )->bIsLeaf )
	{
		( *ppNode )->nPixelCount++;
		( *ppNode )->nRedSum += r;
		( *ppNode )->nGreenSum += g;
		( *ppNode )->nBlueSum += b;
	}
	else
	{
		int	shift = 7 - nLevel;
		int	nIndex = ( ( ( r & mask[ nLevel ] ) >> shift ) << 2 ) | ( ( ( g & mask[ nLevel ] ) >> shift ) << 1 ) | ( ( b & mask[ nLevel ] ) >> shift );
		AddColor( &( ( *ppNode )->pChild[ nIndex ] ), r, g, b, nColorBits, nLevel + 1, pLeafCount, pReducibleNodes );
	}
}

void* Quantizer::CreateNode( int nLevel, unsigned int nColorBits, unsigned int* pLeafCount, Node** pReducibleNodes )
{
	Node* pNode = ( Node* )calloc( 1, sizeof( Node ) );
	if ( !pNode ) return 0;
	pNode->bIsLeaf = ( ( unsigned int )nLevel == nColorBits ) ? true : false;
	pNode->nIndex = 0;
	if ( pNode->bIsLeaf ) ( *pLeafCount )++;
	else
	{
		pNode->pNext = pReducibleNodes[ nLevel ];
		pReducibleNodes[ nLevel ] = pNode;
	}
	return pNode;
}

void Quantizer::ReduceTree( unsigned int nColorBits, unsigned int* pLeafCount, Node** pReducibleNodes )
{
	int i = ( int )nColorBits - 1;
	for ( ; ( i > 0 ) && ( !pReducibleNodes[ i ] ); i-- );
	if ( !pReducibleNodes[ i ] ) return;
	Node* pNode = pReducibleNodes[ i ];
	pReducibleNodes[ i ] = pNode->pNext;

	unsigned int nRedSum = 0;
	unsigned int nGreenSum = 0;
	unsigned int nBlueSum = 0;
	unsigned int nChildren = 0;

	for ( i = 0; i < 8; i++ )
	{
		if ( pNode->pChild[ i ] )
		{
			nRedSum += pNode->pChild[ i ]->nRedSum;
			nGreenSum += pNode->pChild[ i ]->nGreenSum;
			nBlueSum += pNode->pChild[ i ]->nBlueSum;
			pNode->nPixelCount += pNode->pChild[ i ]->nPixelCount;
			free( pNode->pChild[ i ] );
			pNode->pChild[ i ] = 0;
			nChildren++;
		}
	}

	pNode->bIsLeaf = true;
	pNode->nRedSum = nRedSum;
	pNode->nGreenSum = nGreenSum;
	pNode->nBlueSum = nBlueSum;
	*pLeafCount -= nChildren - 1;
}

void Quantizer::DeleteTree( Node** ppNode )
{
	for ( int i = 0; i < 8; i++ )
	{
		if ( ( *ppNode )->pChild[ i ] ) DeleteTree( &( ( *ppNode )->pChild[ i ] ) );
	}
	free( *ppNode );
	*ppNode = 0;
}

void Quantizer::GetPaletteColors( Node* pTree, COLOR3* prgb, unsigned int* pIndex, unsigned int* pSum )
{
	if ( pTree )
	{
		if ( pTree->bIsLeaf )
		{
			prgb[ *pIndex ].r = ( unsigned char )( ( pTree->nRedSum ) / ( pTree->nPixelCount ) );
			prgb[ *pIndex ].g = ( unsigned char )( ( pTree->nGreenSum ) / ( pTree->nPixelCount ) );
			prgb[ *pIndex ].b = ( unsigned char )( ( pTree->nBlueSum ) / ( pTree->nPixelCount ) );
			pTree->nIndex = *pIndex;
			if ( pSum )
				pSum[ *pIndex ] = pTree->nPixelCount;
			( *pIndex )++;
		}
		else
		{
			for ( int i = 0; i < 8; i++ )
			{
				if ( pTree->pChild[ i ] )
					GetPaletteColors( pTree->pChild[ i ], prgb, pIndex, pSum );
			}
		}
	}
}

unsigned int Quantizer::GetLeafCount( Node* pTree )
{
	if ( pTree )
	{
		if ( pTree->bIsLeaf )
		{
			return 1;
		}
		else
		{
			unsigned int sum = 0;
			for ( int i = 0; i < 8; i++ )
			{
				if ( pTree->pChild[ i ] )
					sum += GetLeafCount( pTree->pChild[ i ] );
			}
			return sum;
		}
	}
	return 0;
}

unsigned char Quantizer::GetNextBestLeaf( Node** pTree, unsigned int nLevel, COLOR3 c, COLOR3* pal )
{
	if ( ( *pTree )->bIsLeaf )
	{
		return FixBounds( ( int )( *pTree )->nIndex );
	}
	else
	{
		static unsigned char mask[ 8 ] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
		int	shift = 7 - ( int )nLevel;
		int	nIndex = ( ( ( c.r & mask[ nLevel ] ) >> shift ) << 2 ) | ( ( ( c.g & mask[ nLevel ] ) >> shift ) << 1 ) | ( ( c.b & mask[ nLevel ] ) >> shift );
		if ( ( *pTree )->pChild[ nIndex ] )
			return GetNextBestLeaf( &( ( *pTree )->pChild[ nIndex ] ), nLevel + 1, c, pal );
		else
			return GetNearestIndex( c, pal );
	}
}

unsigned int Quantizer::GetColorCount( )
{
	return m_nLeafCount;
}

void Quantizer::SetColorTable( COLOR3* prgb )
{
	unsigned int nIndex = 0;
	if ( m_nOutputMaxColors<16 && m_nLeafCount>m_nOutputMaxColors )
	{
		unsigned int nSum[ 16 ];
		COLOR3 tmppal[ 16 ];
		GetPaletteColors( m_pTree, tmppal, &nIndex, nSum );
		unsigned int j, k, nr, ng, nb, ns, a, b;
		for ( j = 0; j < m_nOutputMaxColors; j++ )
		{
			a = ( j*m_nLeafCount ) / m_nOutputMaxColors;
			b = ( ( j + 1 )*m_nLeafCount ) / m_nOutputMaxColors;
			nr = ng = nb = ns = 0;
			for ( k = a; k < b; k++ )
			{
				nr += tmppal[ k ].r * nSum[ k ];
				ng += tmppal[ k ].g * nSum[ k ];
				nb += tmppal[ k ].b * nSum[ k ];
				ns += nSum[ k ];
			}
			prgb[ j ].r = FixBounds( ( int )( nr / ns ) );
			prgb[ j ].g = FixBounds( ( int )( ng / ns ) );
			prgb[ j ].b = FixBounds( ( int )( nb / ns ) );
		}
	}
	else
	{
		GetPaletteColors( m_pTree, prgb, &nIndex, 0 );
	}
}

bool Quantizer::ColorsAreEqual(COLOR3 a, COLOR3 b)
{
	return (a.r == b.r && a.g == b.g && a.b == b.b);
}

unsigned char Quantizer::GetNearestIndex( COLOR3 c, COLOR3* pal )
{
	if (!pal ) return 0;
	if ( ColorsAreEqual( c, pal[ m_lastIndex ] ) )
		return m_lastIndex;
	unsigned long cur = 0;
	for ( unsigned long i = 0, k = 0, distance = 2147483647; i < m_nLeafCount; i++ )
	{
		k = ( unsigned long )( ( pal[ i ].r - c.r )*( pal[ i ].r - c.r ) + ( pal[ i ].g - c.g )*( pal[ i ].g - c.g ) + ( pal[ i ].b - c.b )*( pal[ i ].b - c.b ) );
		if ( k == 0 )
		{
			m_lastIndex = ( unsigned char )i;
			return ( unsigned char )i;
		}
		if ( k < distance )
		{
			distance = k;
			cur = i;
		}
	}
	m_lastIndex = ( unsigned char )cur;
	return m_lastIndex;
}

unsigned char Quantizer::GetNearestIndexFast( COLOR3 c, COLOR3* pal )
{
	if ( m_nOutputMaxColors<16 && m_nLeafCount>m_nOutputMaxColors )
		return GetNearestIndex( c, pal );
	if ( !pal ) return 0;
	if ( ColorsAreEqual( c, pal[ m_lastIndex ] ) )
		return m_lastIndex;
	m_lastIndex = ( unsigned char )GetNextBestLeaf( &m_pTree, 0, c, pal );
	return m_lastIndex;
}


