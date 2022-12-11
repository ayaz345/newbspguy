// For reduce rgb colors (but only for RGB, no alpha channel process)
// but alpha can easy added by small code modifications
#pragma once

#include "bsptypes.h"

unsigned char FixBounds( int i );
unsigned char FixBounds( float i );
unsigned char FixBounds( double i );


class Quantizer
{
	typedef struct tagNode
	{
		bool bIsLeaf;
		unsigned int nPixelCount;
		unsigned int nRedSum;
		unsigned int nGreenSum;
		unsigned int nBlueSum;
		unsigned int nIndex;
		struct tagNode* pChild[ 8 ];
		struct tagNode* pNext;
	} Node;

protected:
	Node* m_pTree;
	unsigned int m_nLeafCount;
	Node* m_pReducibleNodes[ 8 ];
	unsigned int m_nMaxColors;
	unsigned int m_nOutputMaxColors;
	unsigned int m_nColorBits;
	unsigned char m_lastIndex;
	
public:
	Quantizer( unsigned int nMaxColors, unsigned int nColorBits );
	virtual ~Quantizer( );
	bool ProcessImage( COLOR3 * image, unsigned long size);
	void FloydSteinbergDither(unsigned char* image, long width, long height, unsigned char* target, COLOR3* pal);
	void ApplyColorTable(COLOR3* image, unsigned int size, COLOR3* pal);
	unsigned int GetColorCount( );
	void SetColorTable( COLOR3 * prgb );
	unsigned char GetNearestIndex(COLOR3 c, COLOR3* pal );
	unsigned char GetNearestIndexFast(COLOR3 c, COLOR3* pal );

protected:
	unsigned int GetLeafCount( Node* pTree );
	void AddColor( Node** ppNode, unsigned char r, unsigned char g, unsigned char b, unsigned int nColorBits, int nLevel, unsigned int* pLeafCount, Node** pReducibleNodes );
	void* CreateNode( int nLevel, unsigned int nColorBits, unsigned int* pLeafCount, Node** pReducibleNodes );
	void ReduceTree( unsigned int nColorBits, unsigned int* pLeafCount, Node** pReducibleNodes );
	void DeleteTree( Node** ppNode );
	void GetPaletteColors( Node* pTree, COLOR3* prgb, unsigned int* pIndex, unsigned int* pSum );
	unsigned char GetNextBestLeaf( Node** pTree, unsigned int nLevel, COLOR3 c, COLOR3* pal );
	bool ColorsAreEqual(COLOR3 a, COLOR3 b);
};

