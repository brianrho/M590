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

Author: Brian Ejike
--------------------------------------------------------------------*/

#include <inttypes.h>
#include "M590Client.h"
#include "utility/debug.h"

int16_t M590Client::_state[MAX_LINK] = { NA_STATE, NA_STATE };

M590Client::M590Client(M590Drv * dev) : _link(255), _modem(dev)
{
}

M590Client::M590Client(M590Drv * dev, uint8_t link) : _link(link), _modem(dev)
{
}


////////////////////////////////////////////////////////////////////////////////
// Overrided Print methods
////////////////////////////////////////////////////////////////////////////////

// the standard print method will call write for each character in the buffer
// this is very slow
size_t M590Client::print(const __FlashStringHelper *ifsh)
{
	printFSH(ifsh, false);
}

// if we do override this, the standard println will call the print
// method twice
size_t M590Client::println(const __FlashStringHelper *ifsh)
{
	printFSH(ifsh, true);
}


////////////////////////////////////////////////////////////////////////////////
// Implementation of Client virtual methods
////////////////////////////////////////////////////////////////////////////////

int M590Client::connect(const char * host, uint16_t port)
{
	IPAddress ip;
    if (_modem->resolve_url(host, ip)){
        return connect(ip, port);
    }

	return 0;
}

int M590Client::connect(IPAddress ip, uint16_t port)
{
	LOGINFO1(F("Connecting to"), ip);

	_link = get_first_link();

    if (_link != NO_LINK_AVAIL)
    {
    	if (!_modem->tcp_connect(ip, port, _link))
			return 0;

    	_state[_link] = _link;
    }
	else
	{
    	Serial.println(F("No socket available"));
    	return 0;
    }
    return 1;
}



size_t M590Client::write(uint8_t b)
{
	  return write(&b, 1);
}

size_t M590Client::write(const uint8_t *buf, size_t size)
{
	if (_link >= MAX_LINK || size==0)
	{
		setWriteError();
		return 0;
	}

	bool r = _modem->tcp_write(buf, size, _link);
	if (!r)
	{
        setWriteError();
		LOGERROR1(F("Failed to write to socket"), _link);
		delay(1000);
		stop();
		return 0;
	}

	return size;
}



int M590Client::available()
{
	if (_link != 255)
	{
		int bytes = _modem->avail_data(_link);
		return bytes;
	}

}

int M590Client::read()
{
	uint8_t b;
	if (!available())
		return -1;

	bool conn_close = false;
	_modem->read_data(&b, false, _link, &conn_close);

	if (conn_close)
	{
		_state[_link] = NA_STATE;
		_link = 255;
	}

	return b;
}

int M590Client::read(uint8_t* buf, size_t size)
{
	if (!available())
		return -1;
	return _modem->read_data(buf, size, _link);
}

int M590Client::peek()
{
	uint8_t b;
	if (!available())
		return -1;

	bool conn_close = false;
	_modem->read_data(&b, true, _link, &conn_close);

	if (conn_close)
	{
		_state[_link] = NA_STATE;
		_link = 255;
	}

	return b;
}


void M590Client::flush()
{
	while (available())
		read();
}



void M590Client::stop()
{
	if (_link == 255)
		return;

	LOGINFO1(F("Disconnecting "), _link);

	_modem->tcp_close(_link);
    
    _state[_link] = NA_STATE;
    _link = 255;
    
}


uint8_t M590Client::connected()
{
    return (status() == ESTABLISHED);
}


M590Client::operator bool()
{
  return _link != 255;
}


////////////////////////////////////////////////////////////////////////////////
// Additional WiFi standard methods
////////////////////////////////////////////////////////////////////////////////


uint8_t M590Client::status()
{
	if (_link == 255)
	{
		return CLOSED;
	}
    
	if (_modem->avail_data(_link))
	{
		return ESTABLISHED;
	}

    if (_modem->check_link_status(_link))
	{
		return ESTABLISHED;
	}
	_state[_link] = NA_STATE;
	_link = 255;

	return CLOSED;
}

////////////////////////////////////////////////////////////////////////////////
// Private Methods
////////////////////////////////////////////////////////////////////////////////

uint8_t M590Client::get_first_link()
{
    for (int i = 0; i < MAX_LINK; i++)
	{
      if (_state[i] == NA_STATE)
      {
          return i;
      }
    }
    return LINK_NOT_AVAIL;
}


size_t M590Client::printFSH(const __FlashStringHelper *ifsh, bool appendCrLf)
{
	size_t size = strlen_P((char*)ifsh);
	
	if (_link >= MAX_LINK || size==0)
	{
        setWriteError();
		return 0;
	}

	bool r = _modem->tcp_write(ifsh, size, _link, appendCrLf);
	if (!r)
	{
        setWriteError();
		LOGERROR1(F("Failed to write to socket"), _link);
		delay(1000);
		stop();
		return 0;
	}

	return size;
}
