/*
This sketch parses a raw data format string from serial and transmits through RF


*/
//20;09;DEBUG_Start;Pulses=48;Pulses(uSec)=284,9664,968,284,296,912,296,912,952,292,960,304,952,300,280,928,280,928,276,928,284,928,280,928,936,308,280,928,280,936,268,936,936,308,944,308,944,308,936,316,940,316,936,312,272,944,268,936,268,944,264,7008;
#define PIN 13

const int numChars = 1024;
char receivedChars[numChars];   // an array to store the received data
const int numPulsesMax = 150;
int pulses[numPulsesMax];
int numberPulses = 0;
bool state = 0;
int nbOfRepeat = 5;

bool recvWithEndMarker() {
  static byte ndx = 0;
  char rc;
  boolean newData = false;

  while (Serial.available() > 0) {
    rc = Serial.read();

    if (rc != '\n' && rc != '\r') {
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      if (ndx > 0) {
        receivedChars[ndx] = '\0'; // terminate the string
        ndx = 0;
        newData = true;
        Serial.println(receivedChars);
      }
    }
  }
  return newData;
}

int parseData() {

  //strcpy (receivedChars , "20;09;DEBUG_Start;Pulses=48;Pulses(uSec)=284,9664,968,284,296,912,296,912,952,292,960,304,952,300,280,928,280,928,276,928,284,928,280,928,936,308,280,928,280,936,268,936,936,308,944,308,944,308,936,316,940,316,936,312,272,944,268,936,268,944,264,7008;\n");

  Serial.print("Number of pulses declared: ");
  char* Cursor;
  Cursor = strstr(receivedChars, ";Pulses=") + 8;
  numberPulses = atoi(Cursor);
  Serial.println(numberPulses);

  int pulseLength = -1;
  int pulseCounter = 0;


  Cursor = strstr(receivedChars, "(uSec)=") + 7;

  while (true)
  {
    pulseLength = atoi(Cursor);
    if (!pulseLength) break;

    pulses[pulseCounter] =  pulseLength;
    Serial.print (pulseCounter);
    Serial.print (" ");
    Serial.println (pulseLength);
    Cursor = strchr ( Cursor, ',' ) + 1;
    pulseCounter++;
    if (pulseCounter >= numPulsesMax) break;

  }
  return (pulseCounter == numberPulses + 4);
}

int sendRawData() {
  int timeBetweenFrames = pulses[1];

  for (int n = 0; n < nbOfRepeat; n++)
  {
    for (int i = 2; i < numberPulses + 3 ; i++) {
      state = !state;
      digitalWrite(PIN, state);
      delayMicroseconds(pulses[i]);

      /*Serial.print(i);
        Serial.print(" ");
        Serial.print(state);
        Serial.print(" for ");
        Serial.print(pulses[i]);
        Serial.println("uS");*/

    }
    //Serial.print("Pause for ");
    //Serial.println(timeBetweenFrames);
    state = 0;
    digitalWrite(PIN, state);
    delayMicroseconds(timeBetweenFrames);
  }
  Serial.println("Sent Raw data OK");


}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  pinMode (PIN, OUTPUT);
  digitalWrite (PIN, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:

  if (recvWithEndMarker()) { //event from serial
    if (parseData()) sendRawData();
    else Serial.println("Error while parsing raw data");

  }

}
