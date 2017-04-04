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

#include <Arduino.h>
#include <avr/pgmspace.h>

#include "utility/M590Drv.h"
#include "utility/debug.h"

#define NUM_TAGS    7

#if defined(__SAM3X8E__)
#define vsnprintf_P  vsnprintf
#endif


const char * MODEM_TAGS[] = {
    "\r\nOK\r\n",
    "OK\r\n",
    "\r\nERROR\n",
    " 0,1",
    " 0,3",
    ",CON",
    ",DIS"
};

typedef enum {
    TAG_OK,
    TAG_TCP_OK,
    TAG_ERROR,
    TAG_REG_SUCCESS,
    TAG_REG_ERROR,
    TAG_LINK_CONNECTED,
    TAG_LINK_DISCONNECTED
} ModemTags;

M590Drv::M590Drv(){
}

uint8_t M590Drv::begin(Stream * ss, char sim_state){
    gsm = ss;
    gsm->setTimeout(1000);
    SIM_PRESENCE = sim_state;
  
	bool initOK = false;
	
	for(int i=0; i<5; i++)
	{
		if (send_cmd(F("AT")) == TAG_OK)
		{
			initOK=true;
			break;
		}
		delay(1000);
	}
    
    if (!initOK)
	{
		LOGERROR(F("Cannot initialize modem"));
		//delay(5000);
		return false;
	}
    send_cmd(F("ATE0"));

	if (!SIM_PRESENCE){
		LOGINFO(F("SIM absent."));
		LOGINFO(F("Initialization Complete."));
		return true;
	}
    LOGINFO(F("Waiting for SIM reg"));
    if (send_cmd(F("AT+CREG=0")) != TAG_OK){
        LOGERROR(F("SIM Reg error!"));
        return false;
    }
    int retry;
    for (retry = 0; retry < 20; retry++){
        if (send_cmd_find(F("AT+CREG?"), MODEM_TAGS[TAG_REG_SUCCESS])){
            LOGINFO(F("SIM registered"));
            break;
        }
        delay(1000);
    }
    if (retry == 20){
        LOGERROR(F("SIM Reg failed!"));
        return false;
    }
    LOGINFO1(F("dBm RSSI: "), get_rssi());
    send_cmd(F("AT+CMGF=1"));
    send_cmd(F("AT+CSCS=\"GSM\""));
    _ppp_link = false;
    _buf_pos = 0;
    _curr_link = -1;
    LOGINFO(F("Init done."));
}

int16_t M590Drv::get_rssi(){
    if (!check_serial())
        return false;
    
    char temp[6];
    send_cmd_get(F("AT+CSQ"), F("CSQ:"), F(","), temp, sizeof(temp));
    int rssi = -113 + atoi(temp);
    return rssi;
}

void M590Drv::get_imei(char * str, int len){
    if (!check_serial())
        return;
    
	send_cmd_get(F("AT+CGSN"), F("\r\n"), F("\r\n\r\n"), str, len);
}

uint8_t M590Drv::check_serial(){
    if (send_cmd(F("AT")) == TAG_OK)
        return true;
    LOGDEBUG(F("M590 not found!"));
    return false;
}

uint8_t M590Drv::ppp_connect(const char * apn, const char * uname, const char * pwd){
    if (!check_serial())
        return false;
    
	send_cmd(F("AT+XISP=0"));
	if (send_cmd(F("AT+CGDCONT=1,\"IP\",\"%s\""), 500, apn) != TAG_OK){
        LOGDEBUG(F("Couldnt set APN"));
        return false;
    }
    if (uname != NULL && pwd != NULL)
        send_cmd(F("AT+XGAUTH=1,1,\"%s\",\"%s\""), 500, uname, pwd);
    
	if (get_ip(_ip_addr)){
        LOGINFO(F("PPP link success"));
        return true;
    }
    else{
        LOGERROR(F("PPP link fail"));
        return false;
    }
}

uint8_t M590Drv::get_ip(IPAddress& ip){
    if (!check_serial())
        return false;
    
	send_cmd(F("AT+XIIC=1"), 500);
    
	char retry;
    char temp[20];
	do{
		if (send_cmd_get(F("AT+XIIC?"), F("IC:    "), F("\r\n"), temp, sizeof(temp))){
            if (temp[0] == '1'){
                _ip_addr.fromString(temp + 3);
                ip = _ip_addr;
                _ppp_link = true;
                return _ppp_link;
            }            
        }
        //delay(500);
		
		retry++;
	}
	while (retry < 10);
    
	_ppp_link = false;
    return _ppp_link;
}


uint8_t M590Drv::resolve_url(const char * url, IPAddress& ip){
    if (!check_serial())
        return false;
    
	if (_ppp_link){
        char temp[20];
		if (send_cmd_get(F("AT+DNS=\"%s\""), F("+DNS:"), F("\r\n"), temp, sizeof(temp), 4000, url)){
            if (strstr(temp, "Error") == NULL){
                ip.fromString(temp);
                return true;
            }
            else{
                LOGERROR(F("URL could not be resolved!"));
            }
        }
        else {
            LOGERROR(F("Unexpected DNS response"));
        }
	}
	else {
		LOGERROR(F("No PPP link!"));
    }
    return false;
}

bool M590Drv::check_link_status(uint8_t link){
    if (link >= MAX_LINK)
        return false;
    
    if (_ppp_link){
        gsm->print("AT+IPSTATUS="); gsm->println(link);
        if (gsm->find((char *)MODEM_TAGS[TAG_LINK_CONNECTED]))
            return true;
    }
    return false;
}

uint8_t M590Drv::tcp_connect(IPAddress& host, uint16_t port, uint8_t link){    
    if (link >= MAX_LINK){
        LOGERROR(F("Link not supported!"));
        return false;
    }
    
    if (check_link_status(link)){
        tcp_close(link);
    }
    
	if (_ppp_link){
        char hostbuf[20];
        sprintf(hostbuf, "%d.%d.%d.%d", host[0], host[1], host[2], host[3]);
        char temp[15];
        
		if (send_cmd_get(F("AT+TCPSETUP=%d,%s,%d"), F("UP:"), F("\r\n"), temp, sizeof(temp), 4000, link, hostbuf, port)){
            if (strstr(temp, "0,OK")){
                LOGINFO1(F("Connected to "), host);
                return true;
            }
        }
	}
	else{
		LOGERROR(F("No PPP link!"));
	}
    return false;
}

uint8_t M590Drv::tcp_write(const uint8_t * data, uint16_t len, uint8_t link){
    bool ret = check_link_status(link);
    if (!ret){
        LOGERROR(F("Link is not connected"));
        return false;
    }
    
	LOGDEBUG2(F("> sendData:"), link, len);

	char params[10];
	sprintf(params, "%d,%u", link, len);
    
	if (send_cmd_find(F("AT+TCPSEND=%s"), ">", 1000, params)){
        gsm->write(data, len);
        gsm->write('\r');

        char temp[10];
        if (locate_tag("D:", "\r\n", temp, sizeof(temp), 3000, 0)){
            if (strstr(temp, params)){
                LOGINFO1(F("Data sent of size"), len);
                return true;
            }
            else {
                LOGERROR(F("Error sending data. No PPP link."));
                return false;
            }
        }
        else {
            LOGERROR(F("Data send error. Unexpected response"));
            return false;
        }
    }
    else {
        LOGERROR(F("Data send error. Did not find '>'"));
        return false;
    } 
}

// Overrided sendData method for __FlashStringHelper strings
bool M590Drv::tcp_write(const __FlashStringHelper *data, uint16_t len, uint8_t link, bool appendCrLf)
{
    bool ret = check_link_status(link);
    if (!ret){
        LOGERROR(F("Link is not connected"));
        return false;
    }
    
	LOGDEBUG2(F("> sendData:"), link, len);

	char cmdBuf[20];
	uint16_t len2 = len + 2*appendCrLf;
	sprintf_P(cmdBuf, PSTR("AT+TCPSEND=%d,%u"), link, len2);
	gsm->println(cmdBuf);

	char params[10];
	sprintf_P(params, PSTR("%d,%u"), link, len);
    
	if (send_cmd_find(F("AT+TCPSEND=%s"), ">", 1000, params)){
        PGM_P p = reinterpret_cast<PGM_P>(data);
        for (int i=0; i<len; i++)
        {
            unsigned char c = pgm_read_byte(p++);
            gsm->write(c);
        }
        if (appendCrLf)
        {
            gsm->write('\r');
            gsm->write('\n');
        }
        
        char temp[10];
        if (locate_tag("SEND:", "\r\n", temp, sizeof(temp), 3000, 0)){
            if (strstr(temp, params)){
                LOGINFO1(F("Data sent of size"), len);
                return true;
            }
            else {
                LOGERROR(F("Error sending data. No PPP link."));
                return false;
            }
        }
        else {
            LOGERROR(F("Data send error. Unexpected response"));
            return false;
        }
    }
    else {
        LOGERROR(F("Data send error. Did not find '>'"));
        return false;
    }
}

uint16_t M590Drv::avail_data(uint8_t link)
{
    //LOGDEBUG(_buf_pos);

	// if there is data in the buffer
	if (_buf_pos > 0 && _curr_link == link)
		return _buf_pos;

    int bytes = gsm->available();

	if (bytes)
	{
		//LOGDEBUG1(F("Bytes in the serial buffer: "), bytes);
		if (gsm->find((char *)"CV:"))
		{
			// format is : +TCPRECV:<link>,<length>,<data>

			_curr_link = gsm->parseInt();    // <link>
			gsm->read();                  // ,
			_buf_pos = gsm->parseInt();    // <len>
			gsm->read();                  // ,

			LOGDEBUG();
			LOGDEBUG2(F("Data packet"), _curr_link, _buf_pos);
            
			if(_curr_link == link)
				return _buf_pos;
		}
	}/*
    else {
        LOGDEBUG("Nothing available!");
        return 0;
    }*/
    return 0;
}

bool M590Drv::read_data(uint8_t *data, bool peek, uint8_t link, bool* conn_close)
{
	if (link != _curr_link)
		return false;

	// see Serial.timedRead

	unsigned long _startMillis = millis();
	do
	{
		if (gsm->available())
		{
			if (peek)
			{
				*data = gsm->peek();
			}
			else
			{
				*data = gsm->read();
				_buf_pos--;
			}

			return true;
		}
	} while(millis() - _startMillis < 1000);

    // timed out, reset the buffer
	LOGERROR1(F("TIMEOUT:"), _buf_pos);

    _buf_pos = 0;
	_curr_link = -1;
	*data = 0;
	
	return false;
}

/**
 * Receive the data into a buffer.
 * It reads up to bufSize bytes.
 * @return	received data size for success else -1.
 */
int16_t M590Drv::read_data_buf(uint8_t *buf, uint16_t buf_size, uint8_t link)
{
	if (link != _curr_link)
		return false;

	if(_buf_pos < buf_size)
		buf_size = _buf_pos;
	
	for(int i=0; i<buf_size; i++)
	{
		int c = timed_read();
		//LOGDEBUG(c);
		if(c == -1)
			return -1;
		
		buf[i] = (uint8_t)c;
		_buf_pos--;
	}

	return buf_size;
}

uint8_t M590Drv::tcp_close(uint8_t link){
    bool idx = check_link_status(link);
    
    if (!idx) return true;
    
	idx = send_cmd(F("AT+TCPCLOSE=%d"), 500, link);
    
	if (idx == TAG_TCP_OK){
		LOGINFO(F("TCP link closed"));
        return true;
    }
	else{
		LOGERROR1(F("Failed to close TCP link"), link);
        return false;
    }
}

uint8_t M590Drv::power_down(){
    if (!check_serial())
        return false;
    
	int idx = send_cmd(F("AT+CPWROFF"), 500);
	if (idx == TAG_OK){
		LOGINFO(F("Power off in progress. Completes in 5 secs."));
		return true;
	}
	else{
		LOGERROR(F("Error powering down!"));
		return false;
	}
}

void M590Drv::interact(){
	Serial.println(F("Entering transparent mode..."));
	while (1){
		while (Serial.available() > 0)
			gsm->write(Serial.read());
		while (gsm->available() > 0)
			Serial.write(gsm->read());
	}
}

////////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////////



/*
* Sends the AT command and stops if any of the TAGS is found.
* Extract the string enclosed in the passed tags and returns it in the outStr buffer.
* Returns true if the string is extracted, false if tags are not found of timed out.
*/
bool M590Drv::send_cmd_get(const __FlashStringHelper* cmd, const __FlashStringHelper* startTag, const __FlashStringHelper* endTag, char* outStr, int outStrLen, int init_timeout, ...)
{
    char cmdBuf[CMD_BUFFER_SIZE];

	va_list args;
	va_start (args, init_timeout);
	vsnprintf_P (cmdBuf, CMD_BUFFER_SIZE, (char*)cmd, args);
	va_end (args);
    
	char _startTag[strlen_P((char*)startTag)+1];
	strcpy_P(_startTag,  (char*)startTag);

	char _endTag[strlen_P((char*)endTag)+1];
	strcpy_P(_endTag,  (char*)endTag);
    
    LOGDEBUG(F("----------------------------------------------"));
	LOGDEBUG1(F(">>"), cmd);

	// send AT command to ESP
    gsm->println(cmdBuf);
    
    int idx = locate_tag(_startTag, _endTag, outStr, outStrLen, init_timeout);
    
	return idx;
}

bool M590Drv::locate_tag(const char* startTag, const char* endTag, char* outStr, int outStrLen, int init_timeout, int final_timeout){
    int idx;
	bool ret = false;
    outStr[0] = 0;

	empty_buf();// read result until the startTag is found
	idx = read_until(init_timeout, startTag, false);
    
	if(idx==NUM_TAGS)
	{
        //Serial.print(">> Found first tag: "); Serial.println(startTag);
		// clean the buffer to get a clean string
		ringBuf.init();

		// start tag found, search the endTag
		idx = read_until(500, endTag, false);
        
		if(idx==NUM_TAGS)
		{
            //Serial.print(">> Found 2nd tag: "); Serial.println(endTag);
			// end tag found
			// copy result to output buffer avoiding overflow
			ringBuf.getStrN(outStr, strlen(endTag), outStrLen-1);

            // read the remaining part of the response
            read_until(final_timeout, NULL, false, true);
            
			ret = true;
		}
		else
		{
			LOGWARN1(F("End tag not found"), startTag);
		}
	}
	else if(idx>=0 and idx<NUM_TAGS)
	{
		// the command has returned but no start tag is found
		LOGDEBUG1(F("No start tag found:"), idx);
	}
	else
	{
		// the command has returned but no tag is found
		LOGWARN1(F("No tag found:"), startTag);
	}
    
    LOGDEBUG1(F("---------------------------------------------- >"), outStr);
	LOGDEBUG();
    
    return ret;
}

int M590Drv::read_until(int timeout, const char* tag, bool findTags, bool emptySerBuf)
{
	ringBuf.reset();

	char c;
    unsigned long lastRead = millis();
	int ret = -1;

	while (millis() - lastRead < timeout)
	{
        if(gsm->available())
		{            
            c = (char)gsm->read();
			LOGDEBUG0(c);
			ringBuf.push(c);

			if (tag!=NULL)
			{
				if (ringBuf.endsWith(tag))
				{
					ret = NUM_TAGS;
                    return ret;
					//LOGDEBUG1("xxx");
				}
			}
			if(findTags)
			{
				for(int i=0; i<NUM_TAGS; i++)
				{
					if (ringBuf.endsWith(MODEM_TAGS[i]))
					{
						ret = i;
						return ret;
					}
				}
			}
            lastRead = millis();
		}
    }

	if (!emptySerBuf)
	{
		LOGWARN(F(">>> TIMEOUT >>>"));
	}

    return ret;
}

/*
* Sends the AT command and returns the id of the TAG.
* The additional arguments are formatted into the command using sprintf.
* Return -1 if no tag is found.
*/
int M590Drv::send_cmd(const __FlashStringHelper* cmd, int timeout, ...)
{
	char cmdBuf[CMD_BUFFER_SIZE];

	va_list args;
	va_start (args, timeout);
	vsnprintf_P (cmdBuf, CMD_BUFFER_SIZE, (char*)cmd, args);
	va_end (args);

	empty_buf();

	LOGDEBUG(F("----------------------------------------------"));
	LOGDEBUG1(F(">>"), cmdBuf);

	gsm->println(cmdBuf);

	int idx = read_until(timeout);

	LOGDEBUG1(F("---------------------------------------------- >"), idx);
	LOGDEBUG();

	return idx;
}

bool M590Drv::send_cmd_find(const __FlashStringHelper* cmd, const char * tag, int timeout,...)
{
	char cmdBuf[CMD_BUFFER_SIZE];

	va_list args;
	va_start (args, timeout);
	vsnprintf_P (cmdBuf, CMD_BUFFER_SIZE, (char*)cmd, args);
	va_end (args);

	empty_buf();

	LOGDEBUG(F("----------------------------------------------"));
	LOGDEBUG1(F(">>"), cmdBuf);

	gsm->println(cmdBuf);

	int idx = read_until(timeout, tag, false);

	LOGDEBUG1(F("---------------------------------------------- >"), idx);
	LOGDEBUG();

	return idx==NUM_TAGS;
}

void M590Drv::empty_buf(bool warn)
{
    char c;
	int i=0;
	while(gsm->available() > 0)
    {
		c = gsm->read();
		if (warn==true)
			LOGDEBUG0(c);
		i++;
	}
	if (i>0 and warn==true)
    {
		LOGDEBUG(F(""));
		LOGDEBUG1(F("Dirty characters in the serial buffer! >"), i);
	}
}

// copied from Serial::timedRead
int M590Drv::timed_read()
{
  int _timeout = 1000;
  int c;
  unsigned long _startMillis = millis();
  do
  {
    c = gsm->read();
    if (c >= 0) return c;
  } while(millis() - _startMillis < _timeout);

  return -1; // -1 indicates timeout
}
