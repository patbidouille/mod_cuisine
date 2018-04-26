/*
 Projet de sonde luminosité, température et humidité.
 Ajout de commande des volets roulants pour une gestion de la température du logement.

 Basé sur l'exemple Basic ESP8266 MQTT

 Il cherche une connexion sur un serveur MQTT puis :
  - Il affiche les luminosités/humidités, lunière, pression baro intener (et externe).
  - Il envoie tout cela en MQTT
  - La température interne, (externe)
  - L'humidité interne, (externe)
  - La lumiére
  - La pression barométrique et les autres parmêtres du BMP180

 En fonction des paramètres lumiére, température,
 il pourra monter ou descendre le volets roulants.

 Affichage normal : (heure option) Tempéarute, Humidité
 Si BP = 1s -> (peut étre 1 pression)
   Affiche Temp int/ext, Lum int/ext, hum int/ext
 Si BP = 3S ->
   Affiche pression, (prévisions météos options)
 Si BP = 10s -> désactivation volets

 FUTUR :
 On pourra définir les paramètres via une instruction MQTT

 Fonction accessoire :
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 Exemples :
 MQTT:
 https://github.com/aderusha/IoTWM-ESP8266/blob/master/04_MQTT/MQTTdemo/MQTTdemo.ino
 Witty:
 https://blog.the-jedi.co.uk/2016/01/02/wifi-witty-esp12f-board/
 Module tricapteur:
 http://arduinolearning.com/code/htu21d-bmp180-bh1750fvi-sensor-example.php


*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
// BH1750FVI est un IC de capteur de lumière ambiante numérique pour interface de bus I2C.
#include <BH1750FVI.h>
////#include <Adafruit_BMP085.h>  //Le BMP180 est le nouveau capteur de pression barométrique numérique de Bosch Sensortec.
//Le HTU21D est un capteur d'humidité et de température numérique économique, facile à utiliser et très précis.
//#include "Adafruit_HTU21DF.h"
#include "SparkFunHTU21D.h"

// Include custom images
#include "WeatherStationImages.h"
#include "WeatherStationFonts.h"


// DHT 11 sensor
#define DHTPIN 2
#define DHTTYPE DHT11


// Utilisation d’une photo-résistance
// Et ports pour cmd volet
const int port = A0;    // LDR
#define haut 12
#define arret 13
#define bas 15
#define lbp 14

// Buffer pour convertir en chaine de l'adresse IP de l'appareil
char buffer[20];
const char* ssid1 = "FREEBOX_CHRISTINE_P2_EXT";
const char* ssid2 = "FREEBOX_CHRISTINE_P2";
const char* password = "";
const char* mqtt_server = "192.168.0.3";

// Création objet
WiFiClient espClient;
PubSubClient client(espClient);
// DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);
// Capteur de lumiére
BH1750FVI LightSensor(0x23); //Default I2C adres when connecting the addr pin to ground.
// Capteur barométrique
////Adafruit_BMP085 bmp;
// Capteur temp/hum
//Adafruit_HTU21DF htu = Adafruit_HTU21DF();
HTU21D myHumidity;
// Initialize the OLED display using Wire library
SSD1306  display(0x3c, 4, 5);


// Variables
int valeur = 0;
float vin = 0;
char msg[50];
int value = 0;
unsigned long readTime;
char message_buff[100];     //Buffer qui permet de décoder les messages MQTT reçus
long lastMsg = 0;           //Horodatage du dernier message publié sur MQTT
long lastRecu = 0;
bool debug = true;          //Affiche sur la console si True
bool mess = false;          // true si message reçu
String sujet = "";          // string pour le tropic
String mesg = "";           //string pour le message mqtt
String humI = "";           // string pour l'humidité int
String tempI = "";          // string pour la température int
String envoi = "";          // String pour formet le mesg d'envoi mqtt
int pal_li = 960;           // palier lum int
int pal_le = 960;           // palier lum ext
int action = 0;             // mode suivant appui sur le bp
long temp = 0;		    // compteur de temps

volatile byte interruptCounter = 0;
int numberOfInterrupts = 0;

int buttonPushCounter = 0;   // counter for the number of button presses
int buttonState = 0;         // current state of the button
int lastButtonState = 0;     // previous state of the button



//========================================
void setup() {
  pinMode(haut, OUTPUT);     // Initialize le mvmt haut
  pinMode(arret, OUTPUT);     // Initialize le mvmt arret
  pinMode(bas, OUTPUT);     // Initialize le mvmt bas
  pinMode(lbp, INPUT);     // Initialize le BP
  digitalWrite(lbp, LOW); 
  attachInterrupt(digitalPinToInterrupt(lbp), handleInterrupt, RISING);
  Serial.begin(115200);
  // Initialising the UI will init the display too.
  display.init();
  display.clear();

  //delay(500);

  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(30, 31, "Bonjour");
//  delay(1000);

  display.setFont(ArialMT_Plain_10);
  display.drawString(32, 0, "Hello world");
  display.display();
  delay(1500);


  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.subscribe("#");
  
  //int count???? = 0;  // Initialise le compteur de tentative de connection MQTT
  dht.begin();                      // initialize temperature sensor
    /*
 Set the address for this sensor 
 you can use 2 different address
 Device_Address_H "0x5C"
 Device_Address_L "0x23"
 you must connect Addr pin to A3 .
 */
  //LightSensor.SetAddress(Device_Address_H);//Address 0x5C
 // To adjust the slave on other address , uncomment this line
 // lightMeter.SetAddress(Device_Address_L); //Address 0x5C
 //-----------------------------------------------
  /*
   set the Working Mode for this sensor 
   Select the following Mode:
    Continuous_H_resolution_Mode
    Continuous_H_resolution_Mode2
    Continuous_L_resolution_Mode
    OneTime_H_resolution_Mode
    OneTime_H_resolution_Mode2
    OneTime_L_resolution_Mode
    
    The data sheet recommanded To use Continuous_H_resolution_Mode
  */
  LightSensor.begin();
  LightSensor.setMode(BH1750_Continuous_H_resolution_Mode); //or another mode if you like.
  myHumidity.begin(); 
  
  //lightMeter.begin();
  /*bmp.begin();
  if (!bmp.begin()) {
      if ( debug) {
	Serial.println("Could not find a valid BMP085 sensor, check wiring!");
	while (1) {}
      }
  }*/
  /*if (!htu.begin()) {
    Serial.println("Couldn't find sensor!");
    while (1);
  }
  Wire.begin();
  */
}


//========================================
void setup_wifi() {

  int cpt = 0;
  boolean ssid = true;
  delay(10);
  int ss = 1;

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Connexion au WiFi");

  // We start by connecting to a WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    if (ssid) {
      WiFi.begin(ssid1, password);
    }
    else {
      ss = 2;
      WiFi.begin(ssid2, password);
    }
    if (debug) {
        Serial.println();
        Serial.print("Connecting to ");
        if (ss == 1) {
          Serial.println(ssid1);
        } else {
          Serial.println(ssid2);
        }
    }

    int counter = 0;
    while ((WiFi.status() != WL_CONNECTED) && (cpt <= 20)) {
      delay(500);
      Serial.print(".");
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(24, 0, "Connecting to WiFi");
      display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
      display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
      display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
      display.display();

      counter++;
      cpt=cpt+1;
//      Serial.print(".");
    }

    // Si après 20 essais le 1 ssid ne réponds pas on passe au 2ieme
    if (cpt >= 20) {
      if (ssid) {
        ssid=false;
      } else {
        ssid=true;
      }
    }
  }

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(32, 0, "WiFi connecté");
  display.drawString(32, 20, "IP address");
  // On récupère et on prépare le buffer contenant l'adresse IP attibué à l'ESP-01
  IPAddress ip = WiFi.localIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  ipStr.toCharArray(buffer, 20);
  display.drawString(30, 30, String(ipStr));
  display.display();
  delay(2000);

  // Connexion au serveur MQTT
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(25, 0, "Connecting to MQTT");
  display.drawString(32, 20, mqtt_server);
  display.display();
  delay(2000);
  if ( debug ) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Connecting to ");
    Serial.println(mqtt_server);
  }
/*  Serial.print(" as ");
  Serial.println(clientName);
*/
}

//========================================
// Déclenche les actions à la réception d'un message
// D'après http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
void callback(char* topic, byte* payload, unsigned int length) {

  // pinMode(lbp,OUTPUT);
  int i = 0;
  if ( debug ) {
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length,DEC));
  }
  sujet = String(topic);
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';

  String msgString = String(message_buff);
  mesg = msgString;
  if ( debug ) {
    Serial.println("Payload: " + msgString);
  }
  mess = true;

}


//========================================
void reconnect() {
  // Loop until we're reconnected
  int counter = 0;
  int compt = 0;
  boolean noconnect = true;
  // tant qu'il ne trouve pas un serveur affiche rond
  while ((!client.connected()) && (noconnect == true)) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Attempting MQTT connection");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    counter++;


    if (debug ) {
      Serial.print("Attempting MQTT connection...");
    }
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.setFont(ArialMT_Plain_16);
      display.drawString(23, 20, "Connecté sur");
      display.drawString(18, 38, "Serveur MQTT !");
      display.display();
      if (debug ) {
        Serial.println("connected");
      }
      // Once connected, publish an announcement...
      client.publish("cuisine", "hello world");
      // ... and resubscribe
      client.subscribe("#");
      noconnect=false;
    } else {
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.setFont(ArialMT_Plain_10);
      display.drawString(2, 0, "On réessaye dans 3 sec");
      display.setFont(ArialMT_Plain_16);
      display.drawString(5, 20, "Erreur !");
      display.drawString(0, 38, "Non Connecté !");
      display.display();
      delay(500);
      if ( debug ) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      }
      // Wait 5 seconds before retrying
      if (compt <= 3) {
        delay(3000);
        counter=0;
        compt++;
      } else {
        noconnect=false;
        
      }
    }

  }
  delay(1500);
}

//========================================
void sensorRead() {
  readTime = millis();
 // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)

  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
 // float f = dht.readTemperature(true);
  tempI=String(t);
  humI=String(h);
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) { // || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  char buffer[20];
  strcpy(msg, "Température ");
  dtostrf(t,3, 2, buffer);
  // On concatene les 2 tab de char
  strcat(msg, buffer);
  client.publish("cuisine",msg);
  //Serial.println(buffer);
  strcpy(msg, "Humiditée ");
  dtostrf(h,3, 2, buffer);
  strcat(msg, buffer);
  client.publish("cuisine",msg);

  //client.publish("cuisine",sprintf(buf, "%f", h));
  if ( debug ) {
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print(" *C ");
  }
}



//========================================
void readbp() {
  action = 0;
  long tmp = millis();
  if ( digitalRead(lbp) == 1 ) {
  while ( millis() - tmp <= 11000) {
    if (( digitalRead(lbp) == 1 ) && ( millis() - tmp <= 1000 )) {
      action = 1;
      if ( debug ) {
        Serial.print("mode = 1");
        Serial.println(millis() - tmp);
      }
    }
    if (( digitalRead(lbp) == 1 ) && ((millis() - tmp >= 1100 ) && (millis() - tmp <= 3000 ))) {
      action = 2;
      if ( debug ) {
        Serial.print("mode = 2");
        Serial.println(millis() - tmp);
      }
    }
    if (( digitalRead(lbp) == 1 ) && ((millis() - tmp >= 3100 ) && (millis() - tmp <= 10000 ))) {
      action = 3;
      if ( debug ) {
        Serial.print("mode = 3");
        Serial.println(millis() - tmp);
      }
    }
  }
  affoled("", 52, "Bouton poussoir", 8, "mode="+String(action), 10);
//  display.clear();
//  display.setTextAlignment(TEXT_ALIGN_LEFT);
//  display.setFont(ArialMT_Plain_10);
//  display.drawString(25, 0, "Bouton poussoir");
//  display.drawString(32, 20, "mode="+String(mode));
//  display.display();
  delay(1500);
  pinMode(lbp,OUTPUT);
  digitalWrite(lbp,LOW);
  pinMode(lbp,INPUT);
  }
}


//========================================
void traite-bp() {
  //filter out any noise by setting a time buffer 
   if ( ( millis () - lastDebounceTime ) > debounceDelay )   { 
 
     //if the button has been pressed, lets toggle the LED from "off to on" or "on to off" 
     if   (   ( buttonState   ==   HIGH )   &&   ( ledState   <   0 )   )   { 
 
       digitalWrite ( ledPin ,   HIGH ) ;   //turn LED on 
       ledState   =   - ledState ;   //now the LED is on, we need to change the state 
       lastDebounceTime   =   millis ( ) ;   //set the current time 
     } 
     else   if   (   ( buttonState   ==   HIGH )   &&   ( ledState   >   0 )   )   { 
 
       digitalWrite ( ledPin ,   LOW ) ;   //turn LED off 
       ledState   =   - ledState ;   //now the LED is off, we need to change the state 
       lastDebounceTime   =   millis ( ) ;   //set the current time 
     } //close if/else 
 
   } //close if(time buffer) 
}

//========================================
void affoled (String titre, int pt, String sujet, int ps, String mesg, int pm) {
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(pt, 0, titre);
  display.setFont(ArialMT_Plain_16);
  display.drawString(ps, 20, sujet);
  display.drawString(pm, 42, mesg);
  display.display();
  
} //End function

//========================================
void handleInterrupt() {
  long dbounce = millis();
  while (millis() - dbounce < 500) {NOP;}
  interruptCounter++;
  buttonState = digitalRead(buttonPin);
}


//========================================
void loop() {
  // test de connection mqtt, sinon reconnecte
  //int counter = 0;
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
    //Serial.println("Lecture du capteur");

  if(interruptCounter>0) {
      interruptCounter--;
      numberOfInterrupts++;
      delay(200);
      if (numberOfInterrupts == 1) { temp = millis(); }
  }
  
  if (millis() - temp >=4000) {
    if (numberOfInterrupts == 1) { action = 1 ); }
    if (numberOfInterrupts == 2) { action = 2 ); }
    if (numberOfInterrupts == 3) { action = 3 ); }
    numberOfInterrupts = 0;
  }
      
  // affiche message reçu en MQTT
  if ( mess == true ) {
    if ( sujet != "cuisine") {
    pinMode(lbp,OUTPUT);
    /*display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(25, 0, "Message de MQTT");
    display.drawString(32, 20, sujet);
    display.drawString(32, 30, mesg);
    display.display();
    */
    affoled("Message de MQTT", 52, sujet, 8, mesg, 10);
    delay(1500);
    }

    // choisie entre plusieuers tropcs
    // mod_cuisine
    // palier lum int -> /palier_li
    // palier lum ext -> /palier_le
    // volet -> /volet
    // puis valide la commande ou l'init

    if ( sujet == "mod_cuisine/palier_li" ) {
      pal_li = mesg.toInt();      // veleur int depuis string ?????
    }
    if ( sujet == "mod_cuisine/palier_le" ) {
      pal_le = mesg.toInt();
    }
    if ( sujet == "mod_cuisine/volet") {
      if ( mesg == "haut") {
        digitalWrite(haut,HIGH);
        affoled("", 52, "commande HAUT", 18, "HAUT", 20);
        /*display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(25, 0, "commande HAUT");
    display.drawString(32, 20, "HAUT");
    display.display();*/
    delay(1500);
      }
      if ( mesg == "bas") {
        digitalWrite(bas,HIGH);
        affoled("", 52, "commande BAS", 18, "BAS", 20);
       /* display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(25, 0, "commande BAS");
    display.drawString(32, 20, "BAS");
    display.display();*/
    delay(1500);
      }
      if ( mesg == "stop") {
        digitalWrite(arret,HIGH);
        affoled("", 52, "commande STOP", 18, "STOP", 20);
        /*display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(25, 0, "commande STOP");
    display.drawString(32, 20, "STOP");
    display.display();*/
    delay(1500);
      }
      digitalWrite(haut,LOW);
      digitalWrite(bas,LOW);
      digitalWrite(arret,LOW);
    }
    if ( sujet == "mod_cuisine" ) {
      if ( mesg == "ON" ) {
        digitalWrite(lbp,HIGH);
      } else {
        digitalWrite(lbp,LOW);
      }
    }

    pinMode(lbp,INPUT);
    mess = false;
  }



  // Lit l’entrée analogique A0
  valeur = analogRead(port);
  // convertit l’entrée en volt
  vin = (valeur * 3.3) / 1024.0;
  sensorRead(); // Lit temp/hum interne
  
    //Serial.println("Lecture du capteur");
      // Récupération de la luminosité externe
 //   uint16_t lux = lightMeter.readLightLevel();
    uint8_t lux = LightSensor.getLightIntensity();// Get Lux value
    // Récupération de la tem externe
    int tempE = 0;////= bmp.readTemperature();
    //Pressure
    int pression = 0;////= bmp.readPressure();
    // Altitude
    int alt = 0;////= bmp.readAltitude();
    // Pressure at sealevel (calculated)
    int atlmer = 0;////= bmp.readSealevelPressure();
    // Altitude réelle
    int reelalt = 0;////= bmp.readAltitude(101500);
    // Humidité externe
    //int humE = htu.readHumidity();
    // temp externe
    //int tempEb = 0; ////htu.readTemperature(); 
    float humE = myHumidity.readHumidity();
    float tempEb = myHumidity.readTemperature();
    
  
  if ( debug ) {
    Serial.print("ldr valeur = ");
    Serial.println(valeur);
    Serial.print("volt = ");
    Serial.println(vin);

  }

  // affichage, on affiche la temp int par défaut
  // puis le reste en fonction du BP
  affoled("Temp + Humidité interne", 0, "TempI=" +tempI+" °C", 4, "HumI=" +humI+" %", 8);
/*  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(8, 0, "Temp + Humidité interne");
  display.setFont(ArialMT_Plain_16);
  display.drawString(4, 20, "Temp=" +tempI+" °C"); 
  //display.drawString(20, 32, tempI+" °C");
  display.drawString(8, 42, "Hum=" +humI+" %"); 
  //display.drawString(20, 72, humI+" %");
  display.display();
*/
  // On lit le bp

  //readbp();
  if (action == 1) { // affiche temp/hum int et ext
    
    affoled("Temp + Humidité externe", 0, "TempE=" +String(tempEb)+" °C", 4, "HumE=" +String(humE)+" %", 8);
    
/*    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(8, 0, "Temp + Humidité interne");
    display.setFont(ArialMT_Plain_16);
    display.drawString(4, 20, "Temp=" + tempI+" °C");
    display.drawString(8, 42, "Hum=" + humI+" %");
    display.display();
*/    
    delay(2000);
  }

  if (action == 2) { // affiche luminosité int/ext et pression
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);
    display.drawString(14, 0, "Luminosité interne");
    display.setFont(ArialMT_Plain_16);
    display.drawString(25, 20, "Niv=" + String(valeur));
    display.drawString(18, 42, "Volts=" + String(vin));
    display.display();
    delay(2000);
  }


    delay(1500);

// renvoie les niveaux des capteurs tous les 180 sec (3min)
// et traite les infos des capteurs pour lever/abaisser les volets
  long now = millis();
  if (lastMsg == 0) {
    lastMsg=now;
  }
  if (now - lastMsg > 180000) {
    lastMsg = now;
    ++value;
    // conversion des valeur String en char
    //char paramChar[.length()+1];
    //param.toCharArray(paramChar,param.length()+1);
    //envoi = "Luminosité interne %ld"+String(valeur);


    snprintf (msg, 50, "hello world #%ld", value); // crer un tab de 50 char avec texte, valeur int
    client.publish("cuisine", msg);
    snprintf (msg, 50, "Luminosité interne", valeur);
    client.publish("cuisine", msg);
    char lE[tempI.length()];                            // creer un var char temporaire
    //lux.toCharArray(lE,tempI.length());               // conversion des valeur String en char
    snprintf (msg, 50, "Luminosité extene", lux);        // concatene les char
    client.publish("cuisine", msg);
    char hI[humI.length()];                             // on refait le cycle pour la valeur suivante
    humI.toCharArray(hI,tempI.length());
    snprintf (msg, 50, "Humidité intene %ld", hI);
    client.publish("cuisine", msg);
    char hE[humI.length()];
    //humE.toCharArray(hE,tempI.length());
    snprintf (msg, 50, "Humidité externe %ld", humE);
    client.publish("cuisine", msg);
    char tI[tempI.length()];
    tempI.toCharArray(tI,tempI.length());
    snprintf (msg, 50, "Température interne %ld", tI);
    client.publish("cuisine", msg);
    //char tE[tempEb.length()];
    //tempEb.toCharArray(tE,tempI.length());
    snprintf (msg, 50, "Température externe %ld", tempEb);
    client.publish("cuisine", msg);
    char p[tempI.length()];
    //press.toCharArray(p,tempI.length());
    snprintf (msg, 50, "Pression %ld", pression);
    client.publish("cuisine", msg);

    while (now - lastMsg > 10000) {
      Serial.print("compteur: "+ (now-lastMsg));
    }

  }

}
