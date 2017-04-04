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

#ifndef M590_H
#define M590_H

#include "Stream.h"
#include "IPAddress.h"
#include "Ring_Buffer.h"

#define SIM_PRESENT	1
#define SIM_ABSENT	0

// Default state value for links
#define NA_STATE -1

// no free link (1 or 0)
#define NO_LINK_AVAIL 255

// selected link not available
#define LINK_NOT_AVAIL  255

// maximum number of links
#define MAX_LINK  2

// maximum size of AT command
#define CMD_BUFFER_SIZE 200

enum tcp_state {
	CLOSED      = 0,
	ESTABLISHED = 1,
};

class M590Drv {
	public:
        M590Drv();
		uint8_t begin(Stream * ss, char sim_state);
		void get_imei(char * str, int len);
        int get_rssi();
		uint8_t ppp_connect(const char * apn, const char * uname=NULL, const char * pwd=NULL);
		uint8_t get_ip(IPAddress& ip);
		uint8_t resolve_url(const char * url, IPAddress& ip);
        bool check_link_status(uint8_t link=0);
		uint8_t tcp_connect(IPAddress& ip, uint16_t port, uint8_t link=0);
		uint8_t tcp_write(const uint8_t * data, uint16_t len, uint8_t link=0);
        bool tcp_write(const __FlashStringHelper *data, uint16_t len, uint8_t link, bool appendCrLf);
        uint16_t avail_data(uint8_t link=0);
        bool read_data(uint8_t *data, bool peek=false, uint8_t link=0, bool* conn_close=NULL);
        int16_t read_data_buf(uint8_t *buf, uint16_t buf_size, uint8_t link=0);
		uint8_t tcp_close(uint8_t link=0);
		uint8_t power_down();
		void interact();
	private:
        uint8_t check_serial();
        int send_cmd(const __FlashStringHelper* cmd, int timeout=1000, ...);
        bool send_cmd_find(const __FlashStringHelper* cmd, const char * tag=NULL, int timeout=1000, ...);
        bool send_cmd_get(const __FlashStringHelper* cmd, const __FlashStringHelper* startTag, const __FlashStringHelper* endTag, char* outStr, int outStrLen, int init_timeout=1000, ...);
        int read_until(int timeout, const char* tag=NULL, bool findTags=true, bool emptySerBuf=false);
        bool locate_tag(const char* startTag, const char* endTag, char* outStr, int outStrLen, int init_timeout=1000, int final_timeout=1000);
        void empty_buf(bool warn=true);
        int timed_read();
        
		uint8_t SIM_PRESENCE;
        Stream * gsm;
        Ring_Buffer ringBuf;
        int _buf_pos;
        uint8_t _curr_link;
        IPAddress _ip_addr;
        bool _ppp_link;
        
        //friend class M590Client;
};

#endif