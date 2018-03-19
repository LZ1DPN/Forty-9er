/*
Revision 1.0 - Main code by Richard Visokey AD7C - www.ad7c.com
Revision 2.0 - November 6th, 2013...  ever so slight revision by  VK8BN for AD9851 chip Feb 24 2014
Revision 3.0 - April, 2016 - AD9851 + ARDUINO PRO NANO + integrate cw decoder (by LZ1DPN) (uncontinued version)
Revision 4.0 - May 31, 2016  - deintegrate cw decoder and add button for band change (by LZ1DPN)
Revision 5.0 - July 20, 2016  - change LCD with OLED display + IF --> ready to control transceiver RFT SEG-100 (by LZ1DPN)
Revision 6.0 - August 16, 2016  - serial control buttons from computer with USB serial (by LZ1DPN) (1 up freq, 2 down freq, 3 step increment change, 4 print state)
									for no_display work with DDS generator
Revision 7.0 - November 30, 2016  - added some things from Ashhar Farhan's Minima TRX sketch to control transceiver, keyer, relays and other ... (LZ1DPN mod)								
Revision 8.0 - December 12, 2016  - EK1A trx end revision. Setup last hardware changes ... (LZ1DPN mod)
Revision 9.0 - January 07, 2017  - EK1A trx last revision. Remove not worked bands ... trx work well on 3.5, 5, 7, 10, 14 MHz (LZ1DPN mod)
Revision 10.0 - March 13, 2017 	 - scan function
Revision 11.0 - October 22, 2017 	 - RIT + other - other
Revision 12.0 - January 22, 2018  - support AD9833 oscillator (VFO for NorCal NC40a/2018) (49-er/2018)

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Include the library code
#include "Wire.h"
#include <SPI.h>
#include <rotary.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 5   //12
Adafruit_SSD1306 display(OLED_RESET);

//Setup some items
const int SINE = 0x2000;                    // Define AD9833's waveform register value.
const int SQUARE = 0x2020;                  // When we update the frequency, we need to
const int TRIANGLE = 0x2002;                // define the waveform when we end writing.    
unsigned long FREQ = 1000;

//#define TX_RX (4)          // mute + (+12V) relay - antenna switch relay TX/RX, and +V in TX for PA - RF Amplifier (2 sided 2 possition relay)
#define FBUTTON (A0)       // tuning step freq CHANGE from 1Hz to 1MHz step for single rotary encoder possition
//#define ANALOG_KEYER (A1)  // KEYER input - for analog straight key
#define BTNDEC (A2)        // BAND CHANGE BUTTON from 1,8 to 29 MHz - 11 bands
char inTx = 0;     // trx in transmit mode temp var
char keyDown = 1;   // keyer down temp vat

const int FSYNC = 10;                       // Standard SPI pins for the AD9833 waveform generator.
const int SCLK = 13;                         // CLK and DATA pins are shared with the TFT display.
const int SDATA = 11;
const float CRYSTAL = 25000000.0;

#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }
Rotary r = Rotary(3,2); // sets the pins for rotary encoder uses.  Must be interrupt pins.
  
//unsigned long xit=1200; // RIT +600 Hz
unsigned long rx=7000000; // Starting frequency of VFO
unsigned long rx2=1; // temp variable to hold the updated frequency
//unsigned long rxof=800; //800
//unsigned long freqIF=6000000;
//unsigned long rxif=(freqIF-rxof); // IF freq, will be summed with vfo freq - rx variable, my xtal filter now is made from 6 MHz xtals
unsigned long rxRIT=0;
int RITon=0;
unsigned long increment = 100; // starting VFO update increment in HZ. tuning step
int buttonstate = 0;   // temp var
String hertz = "100Hz";
int  hertzPosition = 0;
int byteRead = 0;    // for serial comunication
int var_i = 0;

//byte ones,tens,hundreds,thousands,tenthousands,hundredthousands,millions ;  //Placeholders
String freq; // string to hold the frequency

// buttons temp var
int BTNdecodeON = 0;   
int BTNinc = 3; // set number of default band minus 1

// start variable setup

void setup() {

//set up the pins in/out and logic levels

  pinMode(FSYNC, OUTPUT);
  pinMode(SDATA, OUTPUT);
  pinMode(SCLK, OUTPUT);
  digitalWrite(FSYNC, HIGH);
  digitalWrite(SDATA, LOW);
  digitalWrite(SCLK, HIGH);
   

pinMode(BTNDEC,INPUT);    // band change button
digitalWrite(BTNDEC,HIGH);    // level

pinMode(FBUTTON,INPUT); // Connect to a button that goes to GND on push - rotary encoder push button - for FREQ STEP change
digitalWrite(FBUTTON,HIGH);  //level

Wire.begin();
// Initialize the Serial port so that we can use it for debugging
Serial.begin(115200);

// Can't set SPI MODE here because the display and the AD9833 use different MODES.
SPI.begin();
delay(999); 

  Serial.println("*Initialize AD9833\n");
  Serial.println("Start VFO ver 12.0");

  UpdateDDS(0x2100);                                    
  UpdateDDS(0x50C7);                                    
  UpdateDDS(0x4000);                                    
  UpdateDDS(0xC000);                                    
  UpdateDDS(0x2000); 
  // SWITCH TO SOURCE = DDS 
  // digitalWrite(SOURCE, LOW);
  // NOW THERE SHOULD BE A 
  // 384 Hz SIGNAL AT THE OUTPUT                               
  delay(999);
  
  
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C address 0x3C (for oled 128x32)
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  // Clear the buffer.
    display.clearDisplay();	
	display.setTextSize(2);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.println(rx);
	display.setTextSize(1);
	display.setCursor(0,16);
	display.print("St:");display.print(hertz);
	display.setCursor(64,16);
	display.print("rit:");display.print(rxRIT);
	display.display();
  
   //  rotary
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  
}

///// START LOOP - MAIN LOOP

void loop() {
	checkBTNdecode();  // BAND change
	
// freq change 
  if ((rx != rx2) || (RITon == 1)){
	    showFreq();
  	  UpdateFreq(rx, SQUARE);
      rx2 = rx;
      }

//  step freq change + RIT ON/OFF  
  buttonstate = digitalRead(FBUTTON);
  if(buttonstate == LOW) {
        setincrement();        
    };
//=============================================================	

///	  SERIAL COMMUNICATION - remote computer control for DDS - 1, 2, 3, 4, 5, 6 - worked 
   /*  check if data has been sent from the computer: */
if (Serial.available()) {
    /* read the most recent byte */
    byteRead = Serial.read();
	if(byteRead == 49){     // 1 - up freq
		rx = rx + increment;
		UpdateFreq(rx, SQUARE);
        Serial.println(rx);
		}
	if(byteRead == 50){		// 2 - down freq
		rx = rx - increment;
		UpdateFreq(rx, SQUARE);
		Serial.println(rx);
		}
	if(byteRead == 51){		// 3 - up increment
		setincrement();
		Serial.println(increment);
		}
	if(byteRead == 52){		// 4 - print VFO state in serial console
		Serial.println("VFO_VERSION 14.0");
		Serial.println(rx);
//		Serial.println(rxbfo);
		Serial.println(increment);
		Serial.println(hertz);
		}
  if(byteRead == 53){		// 5 - scan freq forvard 40kHz 
             var_i=0;           
             while(var_i<=40000){
                var_i++;
                rx = rx + 10;
				UpdateFreq(rx, SQUARE);
                Serial.println(rx);
//                showFreq();
                if (Serial.available()) {
					if(byteRead == 53){
					    break;						           
					}
				}
             }        
   }

   if(byteRead == 54){   // 6 - scan freq back 40kHz  
             var_i=0;           
             while(var_i<=40000){
                var_i++;
                rx = rx - 10;
				UpdateFreq(rx, SQUARE);
                Serial.println(rx);
//                showFreq();
                if (Serial.available()) {
                    if(byteRead == 54){
                        break;                       
                    }
                }
             }        
   }  
 }
	
//===============================================================	
}	  
/// END of main loop ///
/// ===================================================== END ============================================


/// START EXTERNAL FUNCTIONS

ISR(PCINT2_vect) {
  unsigned char result = r.process();
if (result) {  
	if (RITon==0){
		if (result == DIR_CW){rx=rx+increment;}
		else {rx=rx-increment;}
	}
	if (RITon==1){
		if (result == DIR_CW){
		  rxRIT=rxRIT+50;
		  }
		else {
		  rxRIT=rxRIT-50;
	 	  }
  } 
}
}

// step increments for rotary encoder button
void setincrement(){
  if(increment == 0){increment = 1; hertz = "1Hz"; hertzPosition=0;RITon=0;} 
  else if(increment == 1){increment = 10; hertz = "10Hz"; hertzPosition=0;RITon=0;}
  else if(increment == 10){increment = 50; hertz = "50Hz"; hertzPosition=0;RITon=0;}
  else if (increment == 50){increment = 100;  hertz = "100Hz"; hertzPosition=0;RITon=0;}
  else if (increment == 100){increment = 500; hertz="500Hz"; hertzPosition=0;RITon=0;}
  else if (increment == 500){increment = 1000000; hertz="1Mhz"; hertzPosition=0;RITon=0;} 
  else{increment = 0; hertz = "ritON"; hertzPosition=0; RITon=1;};  
  showFreq();
  delay(250); // Adjust this delay to speed up/slow down the button menu scroll speed.
}

// oled display functions
void showFreq(){
	display.clearDisplay();	
	display.setTextSize(2);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.println(rx);
	display.setTextSize(1);
	display.setCursor(0,16);
	display.print("St:");display.print(hertz);
	display.setCursor(64,16);
	display.print("rit:");display.print(rxRIT);
	display.display();
}

//  BAND CHANGE !!! band plan - change if need 
void checkBTNdecode(){
  
BTNdecodeON = digitalRead(BTNDEC);    
    if(BTNdecodeON == LOW){
         BTNinc = BTNinc + 1;
         
         if(BTNinc > 6){
              BTNinc = 3;
              }
              
          switch (BTNinc) {
          case 1:
            rx=1810000;
            break;
          case 2:
            rx=3500000;
            break;
          case 3:
            rx=5351500;
            break;
          case 4:
            rx=7000000;
            break;
          case 5:
            rx=10100000;
            break;
          case 6:
            rx=14000000;
            break;
          case 7:
            rx=18068000;
            break;    
          case 8:
            rx=21000000;
            break;    
          case 9:
            rx=24890000;
            break;    
          case 10:
            rx=28000000;
            break;
          case 11:
            rx=29100000;
            break;        
          default:             
            break;
        }         
        delay(150);     
    }
}

//// OK END OF PROGRAM

// ///////////////////////////////////////////////////////////// 
//! Updates the DDS registers
// ///////////////////////////////////////////////////////////// 
void UpdateDDS(unsigned int data){
  unsigned int pointer = 0x8000;
  // Serial.print(data,HEX);Serial.print(" ");
  digitalWrite(FSYNC, LOW);     // AND NOW : WAIT 5 ns
  for (int i=0; i<16; i++){
   if ((data & pointer) > 0) { digitalWrite(SDATA, HIGH); }
      else { digitalWrite(SDATA, LOW); }
    digitalWrite(SCLK, LOW);
    digitalWrite(SCLK, HIGH);
    pointer = pointer >> 1 ;
  }
  digitalWrite(FSYNC, HIGH);
}

// ///////////////////////////////////////////////////////////// 
//! Updates the Freq registers
// ///////////////////////////////////////////////////////////// 
void UpdateFreq(long FREQ, int WAVE){
   
  long FTW = (FREQ * pow(2, 28)) / CRYSTAL;
  if (WAVE == SQUARE) FTW = FTW << 1;
  unsigned int MSB = (int)((FTW & 0xFFFC000) >> 14);    
  unsigned int LSB = (int)(FTW & 0x3FFF);
  LSB |= 0x4000;
  MSB |= 0x4000; 
  UpdateDDS(0x2100);   
  UpdateDDS(LSB);                   
  UpdateDDS(MSB);                   
  UpdateDDS(0xC000);                
  UpdateDDS(WAVE);
   
}
