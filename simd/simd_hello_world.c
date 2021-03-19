#include <stdio.h>
#include <xmmintrin.h>//SSE

int main( int argc, const char* argv[] )
{
	__m128i b = _mm_set_epi32 (3, 2, 1, 0);
	__m128i c = _mm_set_epi32 (3, 2, 1, 0);
	
	__m128i a = _mm_add_epi32(b, c);
	for (int i = 0; i < 4; i++)
		printf( "Hello World %d \n", ((int*) &a)[i]);
}
