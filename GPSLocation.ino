/*
Retrieve and post your location to a Sparkfun Datastream using the SIM808 board

Retrieves the location of the device using cellular triangulation and posts to a cloud gateway every 1 minutes.

created 25 May 2016
by sandalkuilang
*/

#include <SoftwareSerial.h>

#define RX_PIN 2 //connect to RX Pin
#define TX_PIN 3 //connect to TX Pin
#define DEBUG true

SoftwareSerial ss = SoftwareSerial(TX_PIN, RX_PIN); //initialize software serial

String APN = "internet"; //Set APN for Mobile Service for TELKOMSEL

String response; //global variable for pulling AT command responses from inside functions (there has to be a better way to do this)

/* Timer Const */
uint16_t ATtimeOut = 5000; // How long we will give an AT command to complete
uint16_t FIRST_TIME_BOOT = 5000; // give 5s GSM device to be ready
byte INTERVAL_GPS = 30; // in second 

//cloud URL Building
/*
https://data.sparkfun.com/streams/NJ65RArGbRUd3A8DNxab
*/
String URL = "http://data.sparkfun.com/input/";
const String publicKey = "NJ65RArGbRUd3A8DNxab"; //Public Key for data stream
const String privateKey = "5dReDrY9mDhJY0pBPa7g"; //Private Key for data stream
const String deleteKey = "Bpe5L2NZ9LUakLReA4OW"; //Delete Key for data stream

const byte NUM_FIELDS = 6; //number of fields in data stream
const String fieldNames[NUM_FIELDS] = { "date","latitude", "longitude", "altitude", "speed", "course" }; //actual data fields
String fieldData[NUM_FIELDS]; //holder for the data values

String content; // receive message
bool isHttpRead = false; // read needed
byte tolerateIdle = 0; // 5 idle

void setup()
{
	setupBaudRate();
	delay(FIRST_TIME_BOOT);
	setupGPS(10); // check gps every 10 second
	setupGPRS(); 
	delay(1000);
	closeGPRS();
	Serial.println("Proudly Made in Indonesia");
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
					int speed = fieldData[4].toInt();
					
					if (speed < 5)
					{
						tolerateIdle += 1;
						if (tolerateIdle == 3) 
						{
							isPublish = true;
						}
						else if (tolerateIdle > 5)
						{
							isPublish = true;
							tolerateIdle = 6;
						}
					}
					else if (speed >= 5 && speed < 20)
					{
						setupGPS(25);
						tolerateIdle = 0;
					}
					else if (speed >= 20 && speed < 40)
					{
						setupGPS(15);
						tolerateIdle = 0;
					}
					else if (speed >= 40)
					{
						isPublish = true;
						tolerateIdle = 0;
					} 
					
					if (isPublish)
					{
						openGPRS();
						makeRequest();
						closeGPRS();

						sendATCommand("AT+CMGD=4", 100); // delete all messages
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
	 
#ifdef DEBUG
	Serial.print("Send URL: ");
#endif
	
	sendGetRequest();
	delay(2800);

	clearBuffer();

#ifdef DEBUG
	Serial.print("Make GET Request: ");
#endif 
	ss.println("AT+HTTPACTION=0");
	for (int d = 0; d <= 50; d++)
	{
		delay(100);
		flushBuffer(); //Flush out the Serial Port
	} 

	if (isHttpRead) {
		ss.println("AT+HTTPREAD");
		for (int d = 0; d <= 7; d++)
		{
			delay(500);
			flushBuffer(); //Flush out the Serial Port
		}
	}
}

boolean sendGetRequest() {
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
		Serial.print("?private_key=");
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
	String outputChar;
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

void setupBaudRate() 
{
	Serial.begin(9600);
	ss.begin(9600);
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
	sendATCommand("ATE0", 200); 

	sendATCommand("AT+CMGF=1", 200); 

	sendATCommand("AT+CGATT=1", 200); 

	sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 200); 

	// setup APN
	ss.print("AT+SAPBR=3,1,\"APN\",\"");
	ss.print(APN);
	ss.print("\"");
	ss.println(); 
}

void openGPRS()
{  
	sendATCommand("AT+SAPBR=1,1", 2800);
	 
	//initialize HTTP service. If it's already on, this will throw an Error. 
	sendATCommand("AT+HTTPINIT", 100);

	//Mandatory, Bearer profile identifier
	sendATCommand("AT+HTTPPARA=\"CID\",1", 100); 
}

void closeGPRS()
{
	sendATCommand("AT+HTTPTERM", 100);
	sendATCommand("AT+SAPBR=0,1", 1000);
}

String sendATCommand(char* ATcommand, unsigned int timeout) {
	unsigned long previous;
	String result = "";
	 
	if (ATcommand[0] != '\0')
	{
		ss.println(ATcommand);    // Send the AT command 
		delay(100);
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