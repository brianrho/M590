#include <SoftwareSerial.h>
#include <M590Client.h>

SoftwareSerial myser(4,5);

M590Drv modem;
M590Client client(&modem);

IPAddress ip;

void setup() {
  Serial.begin(9600);
  Serial.println("M590E Test");
  myser.begin(9600);
  modem.begin(&myser, SIM_PRESENT);

  char str[20];
  modem.get_imei(str, sizeof(str));
  Serial.println(str);
  
  if (modem.ppp_connect("web.gprs.mtnnigeria.net")){  // set PDP context and get an IP address
    modem.get_ip(ip);
    Serial.print("IP address: "); Serial.println(ip);

    if (client.connect("httpbin.org", 80)){ 
      char req[] = "GET /ip HTTP/1.1\r\nHost: httpbin.org\r\n\r\n";  // basic HTTP GET
      Serial.println();
      Serial.print("Sending \"GET /ip\" request to httpbin.org...");  // should return a response containing the modem's public IP
      Serial.println();
      client.write((uint8_t *)req, strlen(req));
      
      unsigned long lastRead = millis();
      while (millis() - lastRead < 2000){   // interbyte timeout of 2 secs 
        while (client.available()){
          uint8_t c = client.read();
          Serial.write(c);
          lastRead = millis();
        }
      }
    }
    client.stop();
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  while (Serial.available() > 0){
    myser.write(Serial.read());
  }
  while (myser.available() > 0){
    Serial.write(myser.read());
  }
}
