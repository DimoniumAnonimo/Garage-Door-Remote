#include "garble.h"

#define MUL_VAL 0x4D9B29A1
#define XOR_VAL 0x6982510D


unsigned long calculate_response(unsigned long prompt)
{
  uint8_t swap_bits[8][2] =
  {
    {31, 4},
    {10, 11},
    {15, 13},
    {3, 1},
    {12, 23},
    {25, 5},
    {18, 29},
    {14, 6}
  };
	unsigned long negate = 0xFFFFFFFF;
	for (int i = 0; i < 8; i++)
	{
		negate &= ~((1 << swap_bits[i][0]) | (1 << swap_bits[i][1]));
	}
    unsigned long response = (prompt * MUL_VAL) ^ XOR_VAL;
    unsigned long swap = 0;
	for (int i = 0; i < 8; i++)
	{
		swap |= ((response >> swap_bits[i][0]) & 1) << swap_bits[i][1];
		swap |= ((response >> swap_bits[i][1]) & 1) << swap_bits[i][0];
	}
    response &= negate;
    response |= swap;

    return response;
}