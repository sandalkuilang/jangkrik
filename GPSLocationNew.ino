/*
Retrieve and post your location to a Sparkfun Datastream using the SIM808 board

Retrieves the location of the device using cellular triangulation and posts to a cloud gateway every 1 minutes.

created 25 May 2016
by sandalkuilang
*/

#include <SoftwareSerial.h>
#include <stdio.h>
#include <string.h>

#define RX_PIN 2 //connect to RX Pin
#define TX_PIN 3 //connect to TX Pin
#define DEBUG true

SoftwareSerial ss = SoftwareSerial(TX_PIN, RX_PIN); //initialize software serial

String APN = "internet"; //Set APN for Mobile Service for TELKOMSEL
String URL = "http://data.sparkfun.com/input/";

String response; //global variable for pulling AT command responses from inside functions (there has to be a better way to do this)

/* Timer Const */
int ATtimeOut = 5000; // How long we will give an AT command to complete
int FIRST_TIME_BOOT = 5000; // give 5s GSM device to be ready
uint16_t INTERVAL_GPS = 30; // in second

//cloud URL Building
/*
https://data.sparkfun.com/streams/NJ65RArGbRUd3A8DNxab
*/
const String publicKey = "NJ65RArGbRUd3A8DNxab"; //Public Key for data stream
const String privateKey = "5dReDrY9mDhJY0pBPa7g"; //Private Key for data stream
const String deleteKey = "Bpe5L2NZ9LUakLReA4OW"; //Delete Key for data stream
const byte NUM_FIELDS = 6; //number of fields in data stream
const String fieldNames[NUM_FIELDS] = { "date","latitude", "longitude", "altitude", "speed", "course" }; //actual data fields
String fieldData[NUM_FIELDS]; //holder for the data values
String content;

void setup()
{
	setupBaudRate();
	delay(FIRST_TIME_BOOT);
	setupGPS();
	setupGPRS(); 
	Serial.println("Proudly Made in Indonesia");
}

void loop()
{
	if (ss.available())
	{ 
		char c;
		c = ss.read();
		if (c == 0x0A || c == 0x0D) 
		{
			// Process NMEA string 
			if (content.indexOf("UGNSINF: 1,1") == 1)
			{ 
				if (processGPS(content))
				{
					makeRequest();
				}
			}
			else
			{
				// process another data, SMS or Call
			} 
			content = String("");
		}
		else content.concat(c); 
	}
}

bool processGPS(String value)
{ 
	char frame[100];

	// First get the NMEA string  
	memset(frame, '\0', 100);    // Initialize the string 
	 
	// Parses the string 
	char *strings[30];
	int i = 0;

	value.toCharArray(frame, 100, 0);

	strings[i] = strtok(frame, ",");

	for (int i = 0; i < 30; i++)
	{
		strings[i] = strtok(NULL, ",");
	}
	 
	if (strstr(strings[1], "1") != NULL)  // Fix status
	{
		fieldData[0] = String(strings[1]); // Date
		fieldData[1] = String(strings[2]); // Latitude
		fieldData[2] = String(strings[3]); // Longitude
		fieldData[3] = String(strings[4]); // Altitude
		fieldData[4] = String(strings[5]); // Speed
		fieldData[5] = String(strings[6]); // Course 
		return true;
	} 
	return false;
}
 
void makeRequest()
{
	//Make HTTP GET request and then close out GPRS connection
	/* Lots of other options in the HTTP setup, see the datasheet: google -sim800_series_at_command_manual */
	 
	//initialize HTTP service. If it's already on, this will throw an Error. 
	sendATCommand("AT+HTTPINIT", 100);  

	//Mandatory, Bearer profile identifier
	sendATCommand("AT+HTTPPARA=\"CID\",1", 100); 

#ifdef DEBUG
	Serial.print("Send URL: ");
#endif

	sendURL();
	delay(1000);

	clearBuffer();

#ifdef DEBUG
	Serial.print("Make GET Request: ");
#endif 
	ss.println("AT+HTTPACTION=0");
	for (int d = 0; d <= 10; d++)
	{
		delay(500);
		flushBuffer(); //Flush out the Serial Port
	} 
}

boolean sendURL() {
	//builds url for Cloud GET Request, sends request and waits for reponse. See sendATCommand() for full comments on the flow
	int complete = 0;
	char c;
	String content;
	unsigned long commandClock = millis(); // Start the timeout clock
										   //Print all of the URL components out into the Serial Port
	ss.print("AT+HTTPPARA=\"URL\",\"");
#ifdef DEBUG
	Serial.print("AT+HTTPPARA=\"URL\",\"");
#endif

	ss.print(URL);
#ifdef DEBUG
	Serial.print(URL);
#endif

	ss.print(publicKey);
#ifdef DEBUG
	Serial.print(publicKey);
#endif

	ss.print("?private_key=");
#ifdef DEBUG
	Serial.print("?private_key=");
#endif

	ss.print(privateKey);
#ifdef DEBUG
	Serial.print(privateKey);
#endif

	for (int i_url = 0; i_url < NUM_FIELDS; i_url++) {
		ss.print("&");
#ifdef DEBUG
		Serial.print("&");
#endif
		ss.print(fieldNames[i_url]);
#ifdef DEBUG
		Serial.print(fieldNames[i_url]);
#endif
		ss.print("=");
#ifdef DEBUG
		Serial.print("=");
#endif
		ss.print(fieldData[i_url]);
#ifdef DEBUG
		Serial.print(fieldData[i_url]);
#endif
	}
	ss.print("\"");

#ifdef DEBUG
	Serial.print("\"");
#endif

	ss.println();
#ifdef DEBUG
	Serial.println();
#endif
	return 1; 
}

void flushBuffer() { 
	char inChar;
	String outputChar;
	for (int i = 0; i < 10; i++)
	{
		while (ss.available()) {
			inChar = ss.read();
#if DEBUG
			Serial.print(inChar);
#endif // DEBUG
			delay(10);
		}
	}
}
 
void clearBuffer() { 
	while (ss.available()) { // do it fast
		ss.read(); //if there is anything is the serial Buffer, clear it out
		delay(10);
	}
}

void setupBaudRate() 
{
	Serial.begin(9600);
	ss.begin(9600);
}

void setupGPS() 
{ 
	char cgnsurcCommand[25];
	String CGNSURC = "AT+CGNSURC=";

	sendATCommand("AT+IPR=9600", 100); // set baud rate to 9600
	sendATCommand("AT+CGNSPWR=1", 100); // turn on GPS
	sendATCommand("AT+CGNSSEQ=RMC", 100); // set resulr mode to RMC

	CGNSURC.concat(INTERVAL_GPS); // append with custom interval declared above
	CGNSURC.toCharArray(cgnsurcCommand, 25);
	sendATCommand(cgnsurcCommand, 100); // set interval for automatic return value from GPS device
}
  
void setupGPRS()
{ 	 
	sendATCommand("ATE0", 200); 

	sendATCommand("AT+CMGF=1", 200); 

	sendATCommand("AT+CGATT=1", 200); 

	sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 200); 

	// setup APN
	int complete = 0;
	char c;
	String content;
	unsigned long commandClock = millis(); 
	ss.print("AT+SAPBR=3,1,\"APN\",\"");
	ss.print(APN);
	ss.print("\"");
	ss.println();
	 
	sendATCommand("AT+SAPBR=1,1", 2000); 
}

void sendATCommand(char* ATcommand, unsigned int timeout) {
	uint8_t x = 0;
	char responses[100];
	unsigned long previous;

	memset(responses, '\0', 100);    // Initialice the string
	 
	if (ATcommand[0] != '\0')
	{
		ss.println(ATcommand);    // Send the AT command 
		delay(100);
	}

	x = 0;
	previous = millis();

	// this loop waits for the answer
	do {
		if (ss.available() != 0) {    // if there are data in the UART input buffer, reads it and checks for the asnwer
			responses[x] = ss.read();
			x++;
		}
	} while (((millis() - previous) < timeout));    // Waits for the asnwer with time out
	 
} 