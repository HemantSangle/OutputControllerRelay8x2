/**
 * OutputControllerRelay8x2.pde
 */

#include <SPI.h>
#include "Ethernet.h"
#include "WebServer.h"
#include "Wire.h"

#define SHIELD_1_I2C_ADDRESS  0x20  // 0x20 is the address with all jumpers removed
#define SHIELD_2_I2C_ADDRESS  0x21  // 0x21 is the address with a jumper on position A0

#define MAC_I2C_ADDRESS       0x50  // Microchip 24AA125E48 I2C ROM address

/* If channelInterlocks is set to true, the channels are grouped into
 * pairs starting at 1 (ie: channels 1 & 2, 3 & 4, etc) and only one
 * channel in each pair can be on at any time. For example, if channel
 * 1 is on and 2 is set to on, channel 1 will be turned off first and
 * vice versa. This is to allow control of dual-active devices such as
 * electric curtain motors which must only be driven in one direction
 * at a time. */ 
const byte channelInterlocks = true;

/* CHANGE THIS TO YOUR OWN UNIQUE VALUE.  The MAC number should be
 * different from any other devices on your network or you'll have
 * problems receiving packets. Can be replaced automatically below
 * using a MAC address ROM. */
static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

/* CHANGE THIS TO MATCH YOUR HOST NETWORK.  Most home networks are in
 * the 192.168.0.XXX or 192.168.1.XXX subrange.  Pick an address
 * that's not in use and isn't going to be automatically allocated by
 * DHCP from your router. Can be replaced automatically using DHCP. */
static uint8_t ip[] = { 192, 168, 1, 32 };

#define PREFIX "/control"  // This will be appended to the IP address as the URL
WebServer webserver(PREFIX, 80);

byte shield1BankA = 0; // Current status of all outputs on first shield, one bit per output
byte shield2BankA = 0; // Current status of all outputs on second shield, one bit per output

/* This command is set as the default command for the server.  It
 * handles both GET and POST requests.  For a GET, it returns a simple
 * page with some buttons.  For a POST, it saves the value posted to
 * the buzzDelay variable, affecting the output of the speaker */
void serverCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  /* If we've received a POST request we need to process the submitted form values */
  if (type == WebServer::POST)
  {
    bool repeat;
    char name[20], value[16];
    do
    {
      /* readPOSTparam returns false when there are no more parameters
       * to read from the input.  We pass in buffers for it to store
       * the name and value strings along with the length of those
       * buffers. */
      repeat = server.readPOSTparam( name, 20, value, 16);

      /* This is a standard string comparison function.  It returns 0
       * when there's an exact match. */
      if (strcmp( name, "On" ) == 0)
      {
        setLatchChannelOn( atoi(value) );
      }
      
      if (strcmp( name, "Off" ) == 0)
      {
        setLatchChannelOff( atoi(value) );
      }
      
      if (strcmp( name, "AllOff" ) == 0)
      {
        sendRawValueToLatch1(0);
        sendRawValueToLatch2(0);
      }
      
    } while (repeat);  
    
    // After procesing the POST data, tell the web browser to reload
    // the page using a GET method. 
    server.httpSeeOther(PREFIX);
    return;
  }

  /* for a GET or HEAD, send the standard "it's all OK" headers */
  server.httpSuccess();

  /* Don't output the body for a HEAD request, only for GET */
  if (type == WebServer::GET)
  {
    /* store the HTML in program memory using the P macro */
    P(message) = 
      "<html><head><title>Relays</title>"
      "<body>"
      "<form action='/control' method='POST'>"
      
      "<p><button name='AllOff' value='0'>All Off</button></p>"
      
      "<p><button type='submit' name='On' value='1'>1 On</button><button type='submit' name='Off' value='1'>1 Off</button></p>"
      "<p><button type='submit' name='On' value='2'>2 On</button><button type='submit' name='Off' value='2'>2 Off</button></p>"
      "<p><button type='submit' name='On' value='3'>3 On</button><button type='submit' name='Off' value='3'>3 Off</button></p>"
      "<p><button type='submit' name='On' value='4'>4 On</button><button type='submit' name='Off' value='4'>4 Off</button></p>"
      "<p><button type='submit' name='On' value='5'>5 On</button><button type='submit' name='Off' value='5'>5 Off</button></p>"
      "<p><button type='submit' name='On' value='6'>6 On</button><button type='submit' name='Off' value='6'>6 Off</button></p>"
      "<p><button type='submit' name='On' value='7'>7 On</button><button type='submit' name='Off' value='7'>7 Off</button></p>"
      "<p><button type='submit' name='On' value='8'>8 On</button><button type='submit' name='Off' value='8'>8 Off</button></p>"
      
      "<p><button type='submit' name='On' value='9'>9 On</button><button type='submit' name='Off' value='9'>9 Off</button></p>"
      "<p><button type='submit' name='On' value='10'>10 On</button><button type='submit' name='Off' value='10'>10 Off</button></p>"
      "<p><button type='submit' name='On' value='11'>11 On</button><button type='submit' name='Off' value='11'>11 Off</button></p>"
      "<p><button type='submit' name='On' value='12'>12 On</button><button type='submit' name='Off' value='12'>12 Off</button></p>"
      "<p><button type='submit' name='On' value='13'>13 On</button><button type='submit' name='Off' value='13'>13 Off</button></p>"
      "<p><button type='submit' name='On' value='14'>14 On</button><button type='submit' name='Off' value='14'>14 Off</button></p>"
      "<p><button type='submit' name='On' value='15'>15 On</button><button type='submit' name='Off' value='15'>15 Off</button></p>"
      "<p><button type='submit' name='On' value='16'>16 On</button><button type='submit' name='Off' value='16'>16 Off</button></p>"
      
      "</form></body></html>";

    server.printP(message);
  }
}

/**
 */
void setup()
{
  Wire.begin(); // Wake up I2C bus
  Serial.begin( 38400 );
  Serial.println("SuperHouse.TV Output Controller starting up. v1.0, 16 channel (2 shields)");
  
  Serial.print("Getting MAC address from ROM: ");
  mac[0] = readRegister(0xFA);
  mac[1] = readRegister(0xFB);
  mac[2] = readRegister(0xFC);
  mac[3] = readRegister(0xFD);
  mac[4] = readRegister(0xFE);
  mac[5] = readRegister(0xFF);
  char tmpBuf[17];
  sprintf(tmpBuf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(tmpBuf);
  
  // setup the Ethernet library to talk to the Wiznet board
  Ethernet.begin(mac, ip);  // Use static address defined above
  //Ethernet.begin(mac);      // Use DHCP
  
  // Print IP address:
  Serial.print("My URL: http://");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    if( thisByte < 3 )
    {
      Serial.print(".");
    }
  }
  Serial.println("/control");

  /* Register the default command (activated with the request of
   * http://x.x.x.x/control */
  webserver.setDefaultCommand(&serverCmd);

  /* start the server to wait for connections */
  webserver.begin();

  /* Set up the Relay8 shields */
  initialiseShield(SHIELD_1_I2C_ADDRESS);
  sendRawValueToLatch1(0);  // If we don't do this, channel 6 turns on! I don't know why
  
  initialiseShield(SHIELD_2_I2C_ADDRESS);
  sendRawValueToLatch2(0);  // If we don't do this, channel 6 turns on! I don't know why
  
  Serial.println("Ready.");
}

/**
 */
void loop()
{
  // Process incoming connections one at a time forever
  webserver.processConnection();
}

/**
 */
void initialiseShield(int shieldAddress)
{
  // Set addressing style
  Wire.beginTransmission(shieldAddress);
  Wire.write(0x12);
  Wire.write(0x20); // use table 1.4 addressing
  Wire.endTransmission();

  // Set I/O bank A to outputs
  Wire.beginTransmission(shieldAddress);
  Wire.write(0x00); // IODIRA register
  Wire.write(0x00); // Set all of bank A to outputs
  Wire.endTransmission();
}

/**
 */
void toggleLatchChannel(byte channelId)
{
  if( channelId >= 1 && channelId <= 8 )
  {
    byte shieldOutput = channelId;
    byte channelMask = 1 << (shieldOutput - 1);
    shield1BankA = shield1BankA ^ channelMask;
    sendRawValueToLatch1(shield1BankA);
  }
  else if( channelId >= 9 && channelId <= 16 )
  {
    byte shieldOutput = channelId - 8;
    byte channelMask = 1 << (shieldOutput - 1);
    shield2BankA = shield2BankA ^ channelMask;
    sendRawValueToLatch2(shield2BankA);
  }
}

/**
 */
void setLatchChannelOn (byte channelId)
{
  if( channelInterlocks == true )
  {
    if ( (channelId % 2) == 0)  // This is an even number channel, so turn off the channel before it
    {
      setLatchChannelOff( channelId - 1 );
    } else {                    // This is an odd number channel, so turn off the channel after it
      setLatchChannelOff( channelId + 1 );
    }
  }
  
  if( channelId >= 1 && channelId <= 8 )
  {
    byte shieldOutput = channelId;
    byte channelMask = 1 << (shieldOutput - 1);
    shield1BankA = shield1BankA | channelMask;
    sendRawValueToLatch1(shield1BankA);
  }
  else if( channelId >= 9 && channelId <= 16 )
  {
    byte shieldOutput = channelId - 8;
    byte channelMask = 1 << (shieldOutput - 1);
    shield2BankA = shield2BankA | channelMask;
    sendRawValueToLatch2(shield2BankA);
  }
}


/**
 */
void setLatchChannelOff (byte channelId)
{
  if( channelId >= 1 && channelId <= 8 )
  {
    byte shieldOutput = channelId;
    byte channelMask = 255 - ( 1 << (shieldOutput - 1));
    shield1BankA = shield1BankA & channelMask;
    sendRawValueToLatch1(shield1BankA);
  }
  else if( channelId >= 9 && channelId <= 16 )
  {
    byte shieldOutput = channelId - 8;
    byte channelMask = 255 - ( 1 << (shieldOutput - 1));
    shield2BankA = shield2BankA & channelMask;
    sendRawValueToLatch2(shield2BankA);
  }
}

/**
 */
void sendRawValueToLatch1(byte rawValue)
{
  Wire.beginTransmission(SHIELD_1_I2C_ADDRESS);
  Wire.write(0x12);        // Select GPIOA
  Wire.write(rawValue);    // Send value to bank A
  shield1BankA = rawValue;
  Wire.endTransmission();
}

/**
 */
void sendRawValueToLatch2(byte rawValue)
{
  Wire.beginTransmission(SHIELD_2_I2C_ADDRESS);
  Wire.write(0x12);        // Select GPIOA
  Wire.write(rawValue);    // Send value to bank A
  shield2BankA = rawValue;
  Wire.endTransmission();
}

/**
 * Required to read the MAC address ROM
 */
byte readRegister(byte r)
{
  unsigned char v;
  Wire.beginTransmission(MAC_I2C_ADDRESS);
  Wire.write(r);  // Register to read
  Wire.endTransmission();

  Wire.requestFrom(MAC_I2C_ADDRESS, 1); // Read a byte
  while(!Wire.available())
  {
    // Wait
  }
  v = Wire.read();
  return v;
}
