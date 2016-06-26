/*
Retrieve and post your location to a Sparkfun Datastream using the SIM808 board

Retrieves the location of the device using cellular triangulation and posts to a cloud gateway every 1 minutes.

created 25 May 2016
by sandalkuilang
*/

#include <SoftwareSerial.h>

#define DEBUG true
#define RX_PIN 2 //connect to RX Pin
#define TX_PIN 3 //connect to TX Pin

SoftwareSerial ss = SoftwareSerial(TX_PIN, RX_PIN); //initialize software serial

String APN = "internet"; //Set APN for Mobile Service for TELKOMSEL 

//cloud URL Building
/*
https://data.sparkfun.com/streams/NJ65RArGbRUd3A8DNxab
*/
const String URL  = "http://data.sparkfun.com/input/";
const String publicKey  = "NJ65RArGbRUd3A8DNxab"; //Public Key for data stream
const String privateKey  = "5dReDrY9mDhJY0pBPa7g"; //Private Key for data stream
const String deleteKey  = "Bpe5L2NZ9LUakLReA4OW"; //Delete Key for data stream

const byte NUM_FIELDS  = 6; //number of fields in data stream
const String fieldNames[NUM_FIELDS]  = { "date","latitude", "longitude", "altitude", "speed", "course" }; //actual data fields
String fieldData[NUM_FIELDS]; //holder for the data values

String content; // receive message
bool isHttpRead = false; // read needed
byte tolerate0 = 0; // 90 identify
byte tolerate1 = 0; // 5 identify
byte tolerate5 = 0; // 1 identify
byte tolerate20 = 0; // 0 identify

void setup()
{
	Serial.begin(9600);
	ss.begin(9600);

	delay(5000); // give 5s GSM device to be ready
	setupGPS(5); // check gps every 5 second
	setupGPRS(); 
	delay(1000);
	closeGPRS();
	
	sendATCommand("AT+CMGD=4", 100); // delete all messages
	
	Serial.println(F("Proudly Made in Indonesia"));
}

void loop()
{
	if (ss.available())
	{ 
		bool isPublish = false;
		char c;
		c = ss.read();
		if (c == 0x0A || c == 0x0D) 
		{
			// Process NMEA string 
			if (content.indexOf("UGNSINF: 1,1") == 1)
			{ 
				if (processGPS(content))
				{
					// publish message to server based on speed
					int speed = fieldData[4].toInt(); // get speed

					if (speed <= 2)
					{
						tolerate1 = 0;
						tolerate5 = 0;
						tolerate20 = 0;

						tolerate0 += 1;
						if (tolerate0 > 180) // is 15 minutes
						{
							isPublish = true;
							tolerate0 = 1; // reset counter
						}
						else if (tolerate0 == 1)
						{
							isPublish = true;
						}
					}
					else if (speed > 2 && speed < 11) 
					{
						tolerate0 = 0;
						tolerate5 = 0;
						tolerate20 = 0;

						tolerate1 += 1;
						if (tolerate1 > 12) // 1 minutes
						{
							isPublish = true;
							tolerate1 = 1; // reset counter
						}
						else if (tolerate1 == 2)
						{
							isPublish = true;
						}
					}
					else if (speed >= 11 && speed < 21)
					{
						tolerate0 = 0;
						tolerate1 = 0;
						tolerate20 = 0;

						tolerate5 += 1; 
						if (tolerate5 > 6) // 30 second
						{
							isPublish = true;
							tolerate5 = 1; // reset counter
						}
						else if (tolerate5 == 2)
						{
							isPublish = true;
						}
					}
					else if (speed >= 21 && speed < 40)
					{
						tolerate0 = 0;
						tolerate1 = 0;
						tolerate5 = 0;

						tolerate20 += 1;
						if (tolerate20 > 4) // 20 second
						{
							isPublish = true;
							tolerate20 = 1;
						}
						else if (tolerate20 == 2)
						{
							isPublish = true;
						}
					}
					else if (speed >= 40)
					{ 
						// 5 second
						isPublish = true;
						tolerate0 = 0;
						tolerate1 = 0;
						tolerate5 = 0;
						tolerate20 = 0;
					} 
					
					if (isPublish)
					{
						openGPRS();
						makeRequest();
						closeGPRS();
					}

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
	byte i = 0;

	value.toCharArray(frame, 100, 0);

	strings[i] = strtok(frame, ",");

	for (byte i = 0; i < 30; i++)
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

#if DEBUG
		Serial.println();
		Serial.println("Date : " + fieldData[0]);
		Serial.println("Latitude : " + fieldData[1]);
		Serial.println("Longitude : " + fieldData[2]);
		Serial.println("Altitude : " + fieldData[3]);
		Serial.println("Speed : " + fieldData[4]);
		Serial.println("Course : " + fieldData[5]);
#endif
		return true;
	} 
	return false;
}
 
void makeRequest()
{
	//Make HTTP GET request and then close out GPRS connection
	/* Lots of other options in the HTTP setup, see the datasheet: google -sim800_series_at_command_manual */
	 
#ifdef DEBUG
	Serial.print(F("Send URL: "));
#endif
	
	sendGetRequest();
	uint16_t timeOut = 0;
	while (ss.available() == 0)
	{
		delay(10);
		timeOut += 1;
		if (timeOut >= 350)
		{
			break;
		}
	}

	clearBuffer();

#ifdef DEBUG
	Serial.print(F("Make GET Request: "));
#endif 
	ss.println("AT+HTTPACTION=0");
	for (byte d = 0; d <= 50; d++)
	{
		delay(100);
		flushBuffer(); //Flush out the Serial Port
	} 

	if (isHttpRead) {
		ss.println("AT+HTTPREAD");
		for (byte d = 0; d <= 7; d++)
		{
			delay(500);
			flushBuffer(); //Flush out the Serial Port
		}
	}
}

bool sendGetRequest() {
	//builds url for Cloud GET Request, sends request and waits for reponse. See sendATCommand() for full comments on the flow
	ss.print("AT+HTTPPARA=\"URL\",\"");
#ifdef DEBUG
	Serial.print(F("AT+HTTPPARA=\"URL\",\""));
#endif

	ss.print(URL);
#ifdef DEBUG
	Serial.print(URL);
#endif

	ss.print(publicKey);
#ifdef DEBUG
	Serial.print(publicKey);
#endif
	
	if (privateKey == NULL)
	{
		ss.print("?");
#ifdef DEBUG
		Serial.print("?");
#endif
	}
	else
	{ 
		ss.print("?private_key=");
#ifdef DEBUG
		Serial.print(F("?private_key="));
#endif

		ss.print(privateKey);
#ifdef DEBUG
		Serial.print(privateKey);
#endif
	} 

	for (byte i_url = 0; i_url < NUM_FIELDS; i_url++) { 
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
	for (byte i = 0; i < 10; i++)
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
 
void setupGPS(int interval) 
{ 
	char cgnsurcCommand[25];
	String CGNSURC = "AT+CGNSURC=";

	sendATCommand("AT+IPR=9600", 100); // set baud rate to 9600
	sendATCommand("AT+CGNSPWR=1", 100); // turn on GPS
	sendATCommand("AT+CGNSSEQ=RMC", 100); // set resulr mode to RMC

	CGNSURC.concat(String(interval)); // append with custom interval declared above
	CGNSURC.toCharArray(cgnsurcCommand, 25);
	sendATCommand(cgnsurcCommand, 100); // set interval for automatic return value from GPS device
}
  
void setupGPRS()
{ 	 
	sendATCommand("ATE0", 100); 

	sendATCommand("AT+CMGF=1", 100);

	sendATCommand("AT+CGATT=1", 100);

	sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 100);

	// setup APN
	ss.print("AT+SAPBR=3,1,\"APN\",\"");
	ss.print(APN);
	ss.print("\"");
	ss.println(); 
}

void openGPRS()
{  
	uint16_t timeOut = 0;
	sendATCommand("AT+SAPBR=1,1", 100);
	while (ss.available() == 0)
	{
		delay(10); 
		timeOut += 1;
		if (timeOut >= 350)
		{
			break;
		}
	}

	//initialize HTTP service. If it's already on, this will throw an Error. 
	sendATCommand("AT+HTTPINIT", 50);

	//Mandatory, Bearer profile identifier
	sendATCommand("AT+HTTPPARA=\"CID\",1", 50); 
}

void closeGPRS()
{
	sendATCommand("AT+HTTPTERM", 100);
	sendATCommand("AT+SAPBR=0,1", 400);
}

String sendATCommand(char* ATcommand, unsigned int timeout) {
	unsigned long previous;
	String result = "";
	 
	if (ATcommand[0] != '\0')
	{
		ss.println(ATcommand);    // Send the AT command 
		delay(20);
	}
	 
	previous = millis();

	// this loop waits for the answer
	do {
		if (ss.available() != 0) {    // if there are data in the UART input buffer, reads it and checks for the asnwer
			result.concat(ss.read()); 
		}
	} while (((millis() - previous) < timeout));    // Waits for the asnwer with time out
	return result;
} 