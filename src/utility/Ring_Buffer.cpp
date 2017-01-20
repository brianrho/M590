/*--------------------------------------------------------------------
This file is part of the Arduino M590 library.

The Arduino M590 library is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

The Arduino M590 library is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with The Arduino M590 library.  If not, see
<http://www.gnu.org/licenses/>.
--------------------------------------------------------------------*/

#include "Ring_Buffer.h"

#include <Arduino.h>

Ring_Buffer::Ring_Buffer(unsigned int size)
{
	_size = size;
	// add one char to terminate the string
	ringBuf = new char[size+1];
	ringBufEnd = &ringBuf[size];
	init();
}

Ring_Buffer::~Ring_Buffer() {}

void Ring_Buffer::reset()
{
	ringBufP = ringBuf;
}

void Ring_Buffer::init()
{
	ringBufP = ringBuf;
	memset(ringBuf, 0, _size+1);
}

void Ring_Buffer::push(char c)
{
	*ringBufP = c;
	ringBufP++;
	if (ringBufP>=ringBufEnd)
		ringBufP = ringBuf;
}



bool Ring_Buffer::endsWith(const char* str)
{
	int findStrLen = strlen(str);

	// b is the start position into the ring buffer
	char* b = ringBufP-findStrLen;
	if(b < ringBuf)
		b = b + _size;

	char *p1 = (char*)&str[0];
	char *p2 = p1 + findStrLen;

	for(char *p=p1; p<p2; p++)
	{
		if(*p != *b)
			return false;

		b++;
		if (b == ringBufEnd)
			b=ringBuf;
	}

	return true;
}



void Ring_Buffer::getStr(char * destination, unsigned int skipChars)
{
	int len = ringBufP-ringBuf-skipChars;

	// copy buffer to destination string
	strncpy(destination, ringBuf, len);

	// terminate output string
	destination[len]=0;
}

void Ring_Buffer::getStrN(char * destination, unsigned int skipChars, unsigned int num)
{
	int len = ringBufP-ringBuf-skipChars;

	if (len>num)
		len=num;

	// copy buffer to destination string
	strncpy(destination, ringBuf, len);

	// terminate output string
	destination[len]=0;
}
