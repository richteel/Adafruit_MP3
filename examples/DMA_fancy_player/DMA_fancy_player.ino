/* 
 *  A fancy DMA MP3 player
 *  the PLAY pin will start playback from the beginning of the file
 *  the PAUSE pin will pause playback at the current point
 *  the RESUME pin will resume playback at the point it was paused
 *  a potentiometer is connected to the VOLUME pin to set the playback volume
 */
 
#include "Adafruit_MP3.h"
#include <SPI.h>
#include <SD.h>

#include <Adafruit_ZeroDMA.h>
#include "utility/dma.h"

#define VOLUME_MAX 2047
#define PIN_PAUSE 5
#define PIN_RESUME 6
#define PIN_PLAY 9
#define PIN_VOLUME A3

const char *filename = "test3.mp3";
const int chipSelect = 10;

Adafruit_ZeroDMA leftDMA;
Adafruit_ZeroDMA rightDMA;
ZeroDMAstatus    stat; // DMA status codes returned by some functions
File dataFile;
Adafruit_MP3_DMA player;
DmacDescriptor *desc;

//the output data buffers
int16_t *ping, *pong;
int volume;

//this gets called when the player wants more data
int getMoreData(uint8_t *writeHere, int thisManyBytes){
  int bytesRead = 0;
  while(dataFile.available() && bytesRead < thisManyBytes){
    *writeHere = dataFile.read();
    writeHere++;
    bytesRead++;
  }
  return bytesRead;
}


//this will get called when data has been decoded
void decodeCallback(int16_t *data, int len){
  for(int i=0; i<len; i++){
    int val = map(*data, -32768, 32767, 0, volume);
    *data++ = val;
  }
}

void dma_callback(Adafruit_ZeroDMA *dma) {
  
  digitalWrite(13, HIGH);
  //try to fill the next buffer
  if(player.fill()){
    //stop
    leftDMA.abort();
    rightDMA.abort();
  }
  digitalWrite(13, LOW);
}

void doNothing(Adafruit_ZeroDMA *dma) {
  
}

void initMonoDMA(){
  //set up the DMA channels
  leftDMA.setTrigger(MP3_DMA_TRIGGER);
  leftDMA.setAction(DMA_TRIGGER_ACTON_BEAT);
  stat = leftDMA.allocate();

  //ask for the buffers we're going to use
  player.getBuffers(&ping, &pong);

  //make the descriptors
  desc = leftDMA.addDescriptor(
    ping,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
    MP3_OUTBUF_SIZE,
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false);
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  desc = leftDMA.addDescriptor(
    pong,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
    MP3_OUTBUF_SIZE,
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false);                   // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  //make the descriptor list loop
  leftDMA.loop(true);
  leftDMA.setCallback(dma_callback);

  rightDMA.setTrigger(MP3_DMA_TRIGGER);
  rightDMA.setAction(DMA_TRIGGER_ACTON_BEAT);
  stat = rightDMA.allocate();

  //make the descriptors
  desc = rightDMA.addDescriptor(
    ping + 1,                    // move data from here
    (void *)(&DAC->DATA[1]), // to here (M4)
    MP3_OUTBUF_SIZE,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  desc = rightDMA.addDescriptor(
    pong + 1,                    // move data from here
    (void *)(&DAC->DATA[1]), // to here (M4)
    MP3_OUTBUF_SIZE,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  //make the descriptor list loop
  rightDMA.loop(true);
  rightDMA.setCallback(doNothing);
}

void initStereoDMA(){
    //######### LEFT CHANNEL DMA ##############//
  
  //set up the DMA channels
  leftDMA.setTrigger(MP3_DMA_TRIGGER);
  leftDMA.setAction(DMA_TRIGGER_ACTON_BEAT);
  stat = leftDMA.allocate();

  //ask for the buffers we're going to use
  player.getBuffers(&ping, &pong);

  //make the descriptors
  desc = leftDMA.addDescriptor(
    ping,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?

  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  desc = leftDMA.addDescriptor(
    pong,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?
    
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  //make the descriptor list loop
  leftDMA.loop(true);
  leftDMA.setCallback(dma_callback);

  //######### RIGHT CHANNEL DMA ##############//

  rightDMA.setTrigger(MP3_DMA_TRIGGER);
  rightDMA.setAction(DMA_TRIGGER_ACTON_BEAT);
  stat = rightDMA.allocate();

  //make the descriptors
  desc = rightDMA.addDescriptor(
    ping + 1,                    // move data from here
    (void *)(&DAC->DATA[1]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  desc = rightDMA.addDescriptor(
    pong + 1,                    // move data from here
    (void *)(&DAC->DATA[1]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  //make the descriptor list loop
  rightDMA.loop(true);
  rightDMA.setCallback(doNothing);
}

// the setup routine runs once when you press reset:
void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  while(!Serial);

  pinMode(PIN_PAUSE, INPUT_PULLUP);
  pinMode(PIN_RESUME, INPUT_PULLUP);
  pinMode(PIN_PLAY, INPUT_PULLUP);
  
  while (!SD.begin(12000000, chipSelect)) {
    Serial.println("Card failed, or not present");
    delay(2000);
  }
  Serial.println("card initialized.");

  dataFile = SD.open(filename);
  if(!dataFile){
    Serial.println("could not open file!");
    while(1);
  }

  //set the DAC to the center of the range
  analogWrite(A0, 2048);
  analogWrite(A1, 2048);

  //begin the player
  player.begin();

  //this will be how the player asks for data
  player.setBufferCallback(getMoreData);

  //this will be how the player asks you to clean the data
  player.setDecodeCallback(decodeCallback);
}

void loop() {
  //read volume control
  int v = analogRead(PIN_VOLUME);
  volume = map(v, 0, 1023, 0, VOLUME_MAX);

  //read pins
  
  if(!digitalRead(PIN_PAUSE)){
    player.pause();
  }
  else if(!digitalRead(PIN_RESUME)){
    player.resume();
  }
  else if(!digitalRead(PIN_PLAY)){
    //this button will restart playback of the file
    rightDMA.abort();
    leftDMA.abort();

    if(dataFile){
      dataFile.close();
    }
    dataFile = SD.open(filename);
    if(!dataFile){
      Serial.println("could not open file!");
      while(1);
    }

    player.play(); //this will automatically fill the first buffer and get the channel info
    
    if(player.numChannels == 1)
      initMonoDMA(); //this is a mono file
    
    else if(player.numChannels == 2)
      initStereoDMA(); //this is a stereo file
    
    else{
      Serial.println("only mono and stereo files are supported");
      while(1);
    }
    
    rightDMA.startJob();
    leftDMA.startJob();
  }
  delay(20);
}

