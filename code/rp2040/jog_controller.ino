#include <Adafruit_NeoPixel.h>
#include <Keyboard.h>


Adafruit_NeoPixel strip(1, 16, NEO_GRB + NEO_KHZ800);

volatile uint8_t aFlag = 0;
volatile uint8_t bFlag = 0;
volatile int32_t encoderPos = 0;
volatile uint32_t reading = 0;
volatile int32_t lastencoderPos = 0;
uint8_t buttonState = 0;
uint32_t lastEnabledTime = 0;
uint8_t lastButtonState = 0;
volatile uint8_t currentAxis = 0; // 0 for x, 1 for y, 2 for z
volatile bool enabled = 0;
#define X_BUTTON 26
#define Y_BUTTON 27
#define Z_BUTTON 28
#define EN_BUTTON 29
#define PRB_BUTTON 6
#define X_MASK (1<<0)
#define Y_MASK (1<<1)
#define Z_MASK (1<<2)
#define EN_MASK (1<<3)
#define PRB_MASK (1<<4)

/*
buttonstate
0: x
1: y
2: z
3: en button
4: probe button
*/

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();
  pinMode(3, INPUT_PULLUP); // encoder a
  pinMode(4, INPUT_PULLUP); // encoder b
  pinMode(X_BUTTON, INPUT_PULLUP);
  pinMode(Y_BUTTON, INPUT_PULLUP);
  pinMode(Z_BUTTON, INPUT_PULLUP);
  pinMode(EN_BUTTON, INPUT_PULLUP);
  pinMode(PRB_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(3), encA, RISING); // attach interrupt to encoder a pin, trigger on rising edge
  attachInterrupt(digitalPinToInterrupt(4), encB, RISING);// attach interrupt to encoder b pin, trigger on rising edge

  Keyboard.begin();

}

void loop() {


  //encoder handling must happen constantly
  if(lastencoderPos != encoderPos){
    // on encoder update

    Serial.println(encoderPos);

    if(encoderPos > lastencoderPos){
      if(enabled){
        lastEnabledTime = millis(); // update activity
        Keyboard.press(KEY_F20); // forward
        Keyboard.release(KEY_F20); //release immediately
      }
      //forward
    }
    else if (encoderPos < lastencoderPos){
      if(enabled){
        lastEnabledTime = millis(); // update activity
        Keyboard.press(KEY_F19); // backward
        Keyboard.release(KEY_F19); //release immediately
      }
      //backward
    }
    lastencoderPos = encoderPos;
  }
  


  //the rest can be done periodically for debouncing
  if((millis() + 5) % 50 == 0){// 5ms after buttons are checked, release all keys to prevent sticking
    Keyboard.releaseAll(); // release all keys to prevent sticking
    //happens after encoder release so its ok
  }



  if(millis() % 50 == 0){

    // every 50 ms, check buttons and update state

    // direct read gpio without digitalRead()
    uint32_t raw_gpio = sio_hw->gpio_in;

    // parse into single byte

    // move right 26 so that 26-29 are in least 4 significant bits, mask with 0x0f for only those 4
    // move right 6 so that bit 6 is in least significant bit, mask with 0x01 for only that bit, then shift left 4 to put it in bit 5 position
    buttonState = ((raw_gpio >> 26) & 0x0F) | (((raw_gpio >> 6) & 0x01) << 4);

    // find fallling edges (0->1 because of inverted pull-up logic)
    // xor identifies all changed
    // & identifies going to high
    uint8_t changedButtons = (buttonState ^ lastButtonState) & buttonState & 0x1F; 
    lastButtonState = buttonState; // Update lastButtonState for the next iteration
    if (changedButtons & X_MASK) {
      //switch to x axis
      currentAxis = 0;
      lastEnabledTime = millis();
      Keyboard.press(KEY_F16);


    } else if (changedButtons & Y_MASK) {
      //switch to y axis
      currentAxis = 1;
      lastEnabledTime = millis();
      Keyboard.press(KEY_F17);


    } else if (changedButtons & Z_MASK) {
        //switch to z axis
        currentAxis = 2;
        lastEnabledTime = millis();
        Keyboard.press(KEY_F18);


    }
    else if (changedButtons & EN_MASK) {
      //toggle enable state
        if(enabled){
          enabled = false;
          Keyboard.press(KEY_F13); // lock
        }
        else{
          enabled = true;
          Keyboard.press(KEY_F14); // unlock
        }

    } else if (changedButtons & PRB_MASK) {


        // PRB button is pressed
        lastEnabledTime = millis();
        if (enabled) {
          Keyboard.press(KEY_F15); // probe
        }


    }
    else {
        // No buttons are pressed, release all keys
        Keyboard.releaseAll();
    }

    if (millis() - lastEnabledTime > 10000) { // 10 seconds of inactivity
      enabled = false;
      Keyboard.press(KEY_F13); // lock
    }

  }
}


void setup1(){
  // no setup on core 1
}


bool blinkon = false;

// core 1 loop for handling LED updates, runs independently of core 0 loop
void loop1(){
  if(enabled){
    //blink green to indicate enabled
    if(blinkon){
      blinkon = false;
      strip.setPixelColor(0,strip.Color(0,0,0)); 
    }
    else{
      blinkon = true;
      strip.setPixelColor(0,strip.Color(0,40,0)); 
    }
  }
  else{
    strip.setPixelColor(0,strip.Color(10,10,0)); // yellow to indicate disabled
  }
  if(currentAxis == 0){
    strip.setPixelColor(1,strip.Color(10,10,40)); // add bright blue to indicate x axis
    strip.setPixelColor(2,strip.Color(0,0,0)); // turn off y axis
    strip.setPixelColor(3,strip.Color(0,0,0)); // turn off z axis
  }
  else if(currentAxis == 1){
    strip.setPixelColor(2,strip.Color(10,10,40)); // add bright blue to indicate y axis
    strip.setPixelColor(1,strip.Color(0,0,0)); // turn off x axis
    strip.setPixelColor(3,strip.Color(0,0,0)); // turn off z axis
  }
  else if(currentAxis == 2){
    strip.setPixelColor(3,strip.Color(10,10,40)); // add bright blue to indicate z axis
    strip.setPixelColor(1,strip.Color(0,0,0)); // turn off x axis
    strip.setPixelColor(2,strip.Color(0,0,0)); // turn off y axis
  }
  strip.show();
  delay(50);
  Serial.println("LED update");
}


// interrupts for encoder pulses
void encA(){
  cli();
  reading = sio_hw->gpio_in & (1<<3 | 1<<4);
  if(reading == (uint32_t)((1<<3 | 1<<4)) && aFlag){
    // a is rising, b is active, counter clockwise rotation
    encoderPos--;
    bFlag = 0;
    aFlag = 0;
  }
  else if (reading == uint32_t(1<<3)){ //a is first rising, wait for b to rise for clockwise rotation
    bFlag = 1;
  }


  sei();
}
void encB(){
  cli();
  reading = sio_hw->gpio_in & (1<<3 | 1<<4);
  if(reading == (uint32_t)((1<<3 | 1<<4)) && bFlag){
    // b is rising, a is active, clockwise rotation
    encoderPos++;
    bFlag = 0;
    aFlag = 0;
  } else if(reading == uint32_t(1<<4)){ //b is first rising, wait for a to rise for counter-clockwise rotation
    aFlag = 1;
  }
  sei();
}

