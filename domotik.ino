
#include "typeenum.h" //enumeration evenements
#include "SIM900.h" // bibliotheque shield gsm
#include <SoftwareSerial.h> // port serie logiciel
#include "sms.h" // biliotheque de gestion de sms
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

#define pinRelais1 2
#define pinRelais2 3
#define pinRelais3 4

#define pinBouton1 5
#define pinTemperature A1
#define pinPowerGsm 9

#define I2C_ADDR    0x3F // <<----- Add your address here.  Find it from I2C Scanner
#define BACKLIGHT_PIN     3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7

#define temperatureMiniAlerte 3
#define delaiSecondeReleveTemp 120
#define nbMesureTemp 10

#define numeroAutorise "+33663450044"  //numero autorisé à envoyer et recevoir


// variables thermostat
byte modeThermostat;

//variables temporelles
unsigned long temp_time;
int temp_delay = 5000;

//variables affichage
String strMessage="";

//variables bouton logiciel
int buttonState;             //Variable pour l'état actuel du bouton poussoir
int lastButtonState = LOW;   // Variable pour l'état précédent du bouton poussoir
// les variables suivantes sont de type long car le temps, mesuré en millisecondes
// devient rapidement un nombre qui ne peut pas être stocké dans un type int.
long lastDebounceTime = 0;  // variable pour mémoriser le temps écoulé depuis le dernier changement de la LED
long debounceDelay = 50;    //intervalle anti-rebond

//index action bouton
int actionBouton = 1;

//variables SMS
char contenuSMS[50];
byte position;
String smsResultat;


//init écran LCD
LiquidCrystal_I2C  lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

// initialize the library instances
SMSGSM sms;

void setup() {
  lcd.begin (20,4); // <<—– My LCD was 20*4
  // Switch on the backlight
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home (); // go home
  lcd.setCursor (0,0);
  lcd.print("Init. en cours");

  // initialize the digital pin as an output.
  pinMode(pinRelais1, OUTPUT);
  pinMode(pinRelais2, OUTPUT);
  pinMode(pinRelais3, OUTPUT);
  pinMode(pinBouton1, INPUT_PULLUP);


  //-------------------------------------
  //Allume carte gsm
  //-------------------------------------
  gsmPowerUp();

  //-------------------------------------
  //initialise la communication serie
  //-------------------------------------
  Serial.begin(9600);// opens serial port, sets data rate to 9600 bps
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  // connection state
  boolean notConnected = true;

  String strContenuSMS="";
  
  // Start GSM connection
  while (notConnected)
  {
    if (gsm.begin(2400))
    notConnected = false;
    else
    {
      strMessage = "ERREUR=gsm non connecte";
    }
  }
  if (!notConnected)
  {
    delay(1000);
    strMessage = "Service actif       ";
    strContenuSMS = strMessage;
    strContenuSMS.toCharArray(contenuSMS,50);
    sms.SendSMS(numeroAutorise, contenuSMS);
  }
  
  //test calcul temperature
  float t = temperatureCourant();
  delay(100);
  temp_time = millis();
  
  //initialisation thermostat
  activerModeThermostat(MODE_CONFORT);

  //Affichage sur port serie
  Serial.println(strMessage);
  //Affichage sur écran LCD
  lcd.setCursor (0,0);
  lcd.print(strMessage);
}

//-------------------------------------
// activation d'un mode de thermostat
//-------------------------------------
void activerModeThermostat(typeEvenement evtThermostat) {
    
    //ACTIVATION DES RELAIS au contact du thermostat
    switch (evtThermostat){
      case MODE_CONFORT:
        digitalWrite(pinRelais1, HIGH);
        digitalWrite(pinRelais2, LOW);
        digitalWrite(pinRelais3, LOW);
        lcd.setCursor (0,1);

        lcd.print("Confort  ");
  
        break;
      case MODE_ECO:
        digitalWrite(pinRelais1, LOW);
        digitalWrite(pinRelais2, HIGH);
        digitalWrite(pinRelais3, LOW);

        lcd.setCursor (0,1);
        lcd.print("Eco     ");
        break;
      case MODE_HORSGEL:
        digitalWrite(pinRelais1, LOW);
        digitalWrite(pinRelais2, LOW);
        digitalWrite(pinRelais3, HIGH);

        lcd.setCursor (0,1);
        lcd.print("Hors gel");
        break;
    }        
    delay(100);
}

//-------------------------------------
// sondage du mode de thermostat courant
//-------------------------------------
  typeEvenement modeThermostatCourant() {

  int etatRelais1 = digitalRead(pinRelais1);
  int etatRelais2 = digitalRead(pinRelais2);
  int etatRelais3 = digitalRead(pinRelais3);

  typeEvenement mode;
  mode = AUCUN;

// remplacer par une boucle testant les différents états de relais pour déterminer le mode thermostat

  if (etatRelais1 == HIGH && etatRelais3 ==  LOW){
    mode = MODE_CONFORT;
  }

  if (etatRelais1 == LOW && etatRelais3 ==  LOW){
    mode = MODE_ECO;
  }

  if (etatRelais1 == LOW && etatRelais3 ==  HIGH){
    mode = MODE_HORSGEL;
  }
  return mode;
}

//-------------------------------------
// sondage de la temperature courante
//-------------------------------------
float temperatureCourant (){
  //calcul moyenne de 10 mesures de temperature
  int sensorVal=0;
  for (int i=1;i<=nbMesureTemp;i++){
    sensorVal= sensorVal + analogRead(pinTemperature);
    delay(100);
  }
  sensorVal= sensorVal/nbMesureTemp;
  
  // Converti la lecture en tension
  float tension = sensorVal * 5000.0;
  tension /= 1024.0; 
  
  // Convertir la tension (mv) en degre celcius
  // pour une sonde LM335 avec 10.24mv = 1 degre Kelvin
  float temperature = (tension /10.24) - 273.15;

  return temperature;
}

String float2String(float fNumber){
    int intNumber = fNumber;
    int decNumber = (fNumber - intNumber) * 100;
    String strNumber;
    strNumber = String(intNumber);
    strNumber = strNumber + ".";
    strNumber = strNumber + String(decNumber);
    return strNumber;
}

//-------------------------------------
// creation du message de status
//-------------------------------------
String recupererStatus(){
  //variables  
  typeEvenement mode;
  String strThermostat;
  String strTemperature;
  String msgReponseStatus;

  //Temperateure
  float temperature = temperatureCourant();
  //conversion format float en chaine de caractère
  strTemperature = float2String(temperature);

  //Mode de thermostat;
    mode = modeThermostatCourant();
    switch (mode) {
      case MODE_CONFORT:
      strThermostat = "MODE_CONFORT";
      break;
      case MODE_ECO:
      strThermostat = "MODE_ECO";
      break;
      case MODE_HORSGEL:
      strThermostat = "MODE_HORSGEL";
      break;
    }

  // constituer une chaine comprenant le mode et la temperature
  msgReponseStatus = "";
  msgReponseStatus = msgReponseStatus + strThermostat;
  msgReponseStatus = msgReponseStatus + "\n";
  msgReponseStatus = msgReponseStatus + strTemperature + " C  ";
  return msgReponseStatus;
}

void gsmPowerUp()
{
 pinMode(pinPowerGsm, OUTPUT); 
 digitalWrite(pinPowerGsm,LOW);
 delay(1000);
 digitalWrite(pinPowerGsm,HIGH);
 delay(2000);
 digitalWrite(pinPowerGsm,LOW);
 delay(3000);
}

void loop() 
{
  //Test bouton poussoir
  int reading = digitalRead(pinBouton1);
  if (reading == LOW)
  {
     actionBouton = actionBouton + 1;
     if (actionBouton >3) {actionBouton = 1;}
     delay(100);
     switch (actionBouton) {
      case 1:
        activerModeThermostat(MODE_CONFORT);
        break;
      case 2:
        activerModeThermostat(MODE_ECO);
        break;
      case 3:
        activerModeThermostat(MODE_HORSGEL);
        break;
     }
  }
  
  //relevé périodique temperature
  if (millis() - temp_time >temp_delay)  
  {
    float temperature = temperatureCourant();
    String strTemperature = float2String(temperature) + " C  ";

    Serial.println(strTemperature);
    lcd.setCursor (0,2);
    lcd.print(strTemperature);
    lcd.setCursor (10,2);
    lcd.print(analogRead(pinTemperature));
    temp_time = millis();
  }

  //Consultation réception SMS
  char numeroAppelant[20];
  String strContenuSMS="";
  position = sms.IsSMSPresent(SMS_UNREAD);
  if (position) {
      // recuperation numero appelant et message
      sms.GetSMS(position,numeroAppelant,20,contenuSMS, 50);
      
      strContenuSMS = contenuSMS; // recuperation du contenu dans un String
      strContenuSMS.trim();

      //Affichage numero appelant et message durant 3 secondes
      lcd.setCursor (0,4);
      lcd.print(numeroAppelant);
      lcd.setCursor (12,4);
      lcd.print(contenuSMS);
      delay(3000);
      lcd.setCursor (0,4);
      lcd.print("                   ");
       
      // suppression boite de reception      
      for (int i=1;i<=position;i++){
        char r=sms.DeleteSMS(i);
      }          
      bool renvoyerStatus = false;
    
      // analyse contenu SMS
      if (strContenuSMS.substring(0,12) == "MODE_CONFORT"){
        activerModeThermostat(MODE_CONFORT);
        renvoyerStatus = true;
      }
      if (strContenuSMS.substring(0,8) == "MODE_ECO"){
        activerModeThermostat(MODE_ECO);
        renvoyerStatus = true;
      }
      if (strContenuSMS.substring(0,12) == "MODE_HORSGEL"){
        activerModeThermostat(MODE_HORSGEL);
        renvoyerStatus = true;
      }
      if (strContenuSMS.substring(0,6) == "STATUS"){
        renvoyerStatus = true;
      }
  
      // Envoi SMS status
      if (renvoyerStatus){
        strContenuSMS = recupererStatus();
        strContenuSMS.toCharArray(contenuSMS,50);
        sms.SendSMS(numeroAutorise, contenuSMS);
      }
   
  }
}
