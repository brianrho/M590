/*--------------------------------------------------------------------
This file is part of the Arduino M590 library.

The Arduino M690 library is free software: you can redistribute it
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

#ifndef Ring_Buffer_h
#define Ring_Buffer_h


class Ring_Buffer
{
public:
	Ring_Buffer(unsigned int size=36);
	~Ring_Buffer();

	void reset();
	void init();
	void push(char c);
	int getPos();
	bool endsWith(const char* str);
	void getStr(char * destination, unsigned int skipChars);
	void getStrN(char * destination, unsigned int skipChars, unsigned int num);


private:

	unsigned int _size;
	char* ringBuf;
	char* ringBufEnd;
	char* ringBufP;

};

#endif