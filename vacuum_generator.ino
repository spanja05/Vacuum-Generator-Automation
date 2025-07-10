#include <LiquidCrystal_I2C.h>
#include <OneButton.h>  // Library for push functionality on rotary encoder
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

int menuCounter = 0;  // counter to keep track of which menu item the user is on.
float upperLimitVal;  // the upper limit set by the user for the vacuum tank.
float lowerLimitVal;  // the lower limit set by the user for the vacuum tank.
float currentReading = 19; //random hard coded. keeps track of the current amount in the vacuum tank.
float dataRead;  // the data read from an address.

const int UPPER_LIM_ADDR = 0; // the location where the upper limit is stored.
const int LOWER_LIM_ADDR = 4; // the location where the lower limit is stored.

bool upperSelected = false;  // indicates if the menu option for setting the upper limit is selected.
bool lowerSelected = false;  // indicates if the menu option for setting the lower limit is selected.
float upperLimitUpdate;      // the new value set by the user for the upper limit. 
float lowerLimitUpdate;      // the new value set by the user for the lower limit. 

const int MAX_RATE = 5; // The minimum difference between the current reading and the previous reading within a 10 second interval approaching a critical rate of change. 
int maxTimeDiffRate = 10000; // the time difference to check how fast the current reading is falling in milliseconds.
unsigned long lastTimeCheck = 0; // the time checked previously for detecting rate of change of vacuum tank.
int prevTankReading = 19; // to check for how fast the rate is changing in the vacuum tank.

const int ROTARY_CLK = 2; // CLK pin. 
const int ROTARY_DT = 3;  // DT pin. 
const int PUSH_BUTTON = 4;  // Button input pin. 
const int ROTARY_5V = 5;  // 5V Supply pin. 
const int ROTARY_GND = 6; // GND pin.  

OneButton button(PUSH_BUTTON, true);  // A OneButton object on pin PUSH_BUTTON. 

//Statuses for the rotary encoder
int CLKNow;
int CLKPrevious;
int DTNow;
int DTPrevious;

bool refreshLCD = true;
bool refreshSelection = false;

enum State {
  COMPRESSED_AIR_OFF,
  COMPRESSED_AIR_ON,
  VACUUM_ON,
  VACUUM_OFF
};

State currentState;

void setup() 
{
  Serial.begin(9600);
  pinMode(ROTARY_CLK, INPUT_PULLUP);
  pinMode(ROTARY_DT, INPUT_PULLUP);
  pinMode(PUSH_BUTTON, INPUT_PULLUP);  // Still needed for OneButton
  pinMode(ROTARY_GND, OUTPUT);
  pinMode(ROTARY_5V, OUTPUT);

  digitalWrite(ROTARY_GND, LOW);
  digitalWrite(ROTARY_5V, HIGH);
  
  lcd.init();
  lcd.backlight();
  
  lcd.clear();
  printLCD();
  
  currentState = COMPRESSED_AIR_OFF;
  EEPROM.get(UPPER_LIM_ADDR, upperLimitVal);
  EEPROM.get(LOWER_LIM_ADDR, lowerLimitVal);
  if (isnan(upperLimitVal) && isnan(lowerLimitVal)){
    EEPROM.put(UPPER_LIM_ADDR, 25.0);
    EEPROM.put(LOWER_LIM_ADDR, 19.0);
    EEPROM.get(UPPER_LIM_ADDR, upperLimitVal);
    EEPROM.get(LOWER_LIM_ADDR, lowerLimitVal);
  }
  
  // Setup rotary encoder interrupt (only CLK pin)
  CLKPrevious = digitalRead(ROTARY_CLK);
  attachInterrupt(digitalPinToInterrupt(ROTARY_CLK), rotate, CHANGE);
  
  button.attachClick(handleButtonClick);  // Single click handler
  //  button.attachDoubleClick(handleDoubleClick);
  //  button.attachLongPressStart(handleLongPress);
}

void loop() 
{
  // CRITICAL: Must call this every loop for OneButton to work
  button.tick();
  
  if(refreshLCD == true) {
    updateLCD();  
    if(upperSelected == true || lowerSelected == true) {
      // do nothing
    }
    else {
      updateCursorPosition();
    }
    
    refreshLCD = false;
  }

  unsigned long currentTime = millis();
  Serial.print("110 currentTime: ");
  Serial.println(currentTime);
  if (currentTime - lastTimeCheck >= maxTimeDiffRate){
    currentReading = currentReading - 1; // this is where you get the actual value the sensor reads
    if (abs(currentReading - prevTankReading) >= MAX_RATE){
        // THIS IS WHERE YOU SOUND THE ALERT WITH DIGITAL WRITE
        Serial.println("ALERT!!!!!!!!");
    }
    prevTankReading = currentReading;
    currentReading = 13; // random hard coded rn
    lastTimeCheck = currentTime;
  }

  //  Serial.print("90 Current reading: ");
  //  Serial.println(currentReading);
   if(currentReading < lowerLimitVal || currentReading >= upperLimitVal){
      controlVacuumState();
   }

  if(refreshSelection == true) {
    updateSelection();
    refreshSelection = false;
  }
}

void handleButtonClick() {
  switch(menuCounter) {
     case 0:
     upperSelected = !upperSelected;
     break;

     case 1:
     lowerSelected = !lowerSelected;
     break;
  } 
  
  refreshLCD = true;
  refreshSelection = true;
}

void rotate()
{  
  //-----Upper Option Selected--------------------------------------------------------------
  if(upperSelected == true)
  {
    EEPROM.get(UPPER_LIM_ADDR, dataRead);
    CLKNow = digitalRead(ROTARY_CLK); //Read the state of the CLK pin
  // If last and current state of CLK are different, then a pulse occurred  
  if (CLKNow != CLKPrevious  && CLKNow == 1)
  {
    // If the DT state is different than the CLK state then
    // the encoder is rotating in A direction, so we increase
    if (digitalRead(ROTARY_DT) != CLKNow) 
    {
      
      if(dataRead < 27) //we do not go above 27.5
      {
        upperLimitUpdate = dataRead + 0.5;
        EEPROM.put(UPPER_LIM_ADDR, upperLimitUpdate);   
        EEPROM.get(UPPER_LIM_ADDR, upperLimitVal);
      }
      else
      {
        //don't change the value  // what is the lowest the upper limit can go till?
      }      
    } 
    else 
    {
      if(dataRead < 23.5) //we do not go below 23.5
      {
          // don't change the value
      }
      else
      {
      // Encoder is rotating B direction so decrease
        upperLimitUpdate = dataRead - 0.5;
        EEPROM.put(UPPER_LIM_ADDR, upperLimitUpdate);   
        EEPROM.get(UPPER_LIM_ADDR, upperLimitVal);
      }      
    }    
  }
  CLKPrevious = CLKNow;  // Store last CLK state
  }
  //---Lower Option Selected---------------------------------------------------------------
  else if(lowerSelected == true)
  {
    EEPROM.get(LOWER_LIM_ADDR, dataRead);
    CLKNow = digitalRead(ROTARY_CLK); //Read the state of the CLK pin
  // If last and current state of CLK are different, then a pulse occurred  
  if (CLKNow != CLKPrevious  && CLKNow == 1)
  {
    // If the DT state is different than the CLK state then
    // the encoder is rotating in A direction, so we increase
    if (digitalRead(ROTARY_DT) != CLKNow) 
    {
      if(dataRead > 22.5) //we do not go above 23.2. The value seems to stop at 23.2 most likely because the if statement is checked after it is already incremented
      {
        // don't change the value
      }
      else
      {
        lowerLimitUpdate = dataRead + 0.5;
        EEPROM.put(LOWER_LIM_ADDR, lowerLimitUpdate);   
        EEPROM.get(LOWER_LIM_ADDR, lowerLimitVal);
      }      
    } 
    else 
    {
      if(dataRead < 15.5) //we do not go below 15.5
      {
          // don't change the value
      }
      else
      {
      // Encoder is rotating in B direction, so decrease
        lowerLimitUpdate = dataRead - 0.5;
        EEPROM.put(LOWER_LIM_ADDR, lowerLimitUpdate);  
        EEPROM.get(LOWER_LIM_ADDR, lowerLimitVal);
      }      
    }    
  }
  CLKPrevious = CLKNow;  // Store last CLK state
  }
  else //MENU COUNTER----------------------------------------------------------------------------
  {
  CLKNow = digitalRead(ROTARY_CLK); //Read the state of the CLK pin
  // If last and current state of CLK are different, then a pulse occurred  
  if (CLKNow != CLKPrevious  && CLKNow == 1)
  {
    // If the DT state is different than the CLK state then
    // the encoder is rotating in A direction, so we increase
    if (digitalRead(ROTARY_DT) != CLKNow) 
    {
      if(menuCounter < 1) //we do not go above 1
      {
        menuCounter++;  
      }
      else
      {
        menuCounter = 0;  
      }      
    } 
    else 
    {
      if(menuCounter < 1) //we do not go below 0
      {
          menuCounter = 1;
      }
      else
      {
      // Encoder is rotating CW so decrease
        menuCounter--;      
      }      
    }    
  }
  CLKPrevious = CLKNow;  // Store last CLK state
  }

  //Refresh LCD after changing the counter's value
  refreshLCD = true;
}

void controlVacuumState()
{
  static unsigned long actionStartTime = 0;

  switch(currentState){
    case COMPRESSED_AIR_OFF:
      if (currentReading < lowerLimitVal){
        //do digitalWrite to the compressed air valve pin and set it to high
        actionStartTime = millis();
        currentState = COMPRESSED_AIR_ON;
        Serial.println("Turned on compressed air valve");
      }
      break;
    case COMPRESSED_AIR_ON:
      Serial.print("250 Time Difference: ");
      Serial.println(millis() - actionStartTime);
      if (millis() - actionStartTime >= 500){
        //do digitalWrite to vacuum valve pin and set it to high
        currentState = VACUUM_ON;
        currentReading = 27;
        Serial.println("Turned on vacuum valve");
        Serial.print("257 Current Reading: ");
        Serial.println(currentReading);
      }
      break;
    case VACUUM_ON:
      if (currentReading >= upperLimitVal){
        //do digitalWrite to vacuum valve pin and set it to low
        actionStartTime = millis(); //starting new timer to turn off
        currentState = VACUUM_OFF;
        Serial.println("Turned off vacuum valve");
      }
      break;
    case VACUUM_OFF:
      Serial.print("268 Time Difference: ");
      Serial.println(millis() - actionStartTime);
      if (millis() - actionStartTime >= 500){
        //do digitalWrite to compressed air pin and set it to low
        currentState = COMPRESSED_AIR_OFF;
        Serial.println("Turned off compressed air valve");
      }
      break;
  }
}

void printLCD()
{
  //These are the values which are not changing the operation
  
  lcd.setCursor(0,0); //1st line, 2nd block
  lcd.print("Vacuum Gauge Sensor"); //text
  //----------------------
  lcd.setCursor(1,1); //1st line, 2nd block
  lcd.print("Upper: "); //text
  //----------------------
  lcd.setCursor(1,2); //2nd line, 2nd block
  lcd.print("Lower: "); //text
  //----------------------
  lcd.setCursor(1,3); //2nd line, 2nd block
  lcd.print("Amt: "); //text
}

void updateLCD()
{  
  lcd.setCursor(8,1); //1st line, 10th block
  lcd.print("   "); //erase the content by printing space over it
  lcd.setCursor(8,1); //1st line, 10th block
  lcd.print(upperLimitVal); //print the value of menu1_Value variable

  lcd.setCursor(8,2);
  lcd.print("   ");
  lcd.setCursor(8,2);
  lcd.print(lowerLimitVal); // 
}

void updateCursorPosition()
{
  //Clear display's ">" parts 
  lcd.setCursor(0,1); //1st line, 1st block
  lcd.print(" "); //erase by printing a space
  lcd.setCursor(0,2);
  lcd.print(" "); 
     
  //Place cursor to the new position
  switch(menuCounter) //this checks the value of the counter (0, 1, 2 or 3)
  {
    case 0:
    lcd.setCursor(0,1); //1st line, 1st block
    lcd.print(">"); 
    break;
    //-------------------------------
    case 1:
    lcd.setCursor(0,2); //2nd line, 1st block
    lcd.print(">"); 
    break; 
  }
}

void updateSelection()
{
  //When a menu is selected ">" becomes "X"

  if(upperSelected == true)
  {
    lcd.setCursor(0,1); //1st line, 1st block
    lcd.print("X"); 
  }
  //-------------------
   if(lowerSelected == true)
  {
    lcd.setCursor(0,2); //2nd line, 1st block
    lcd.print("X"); 
  }
}
