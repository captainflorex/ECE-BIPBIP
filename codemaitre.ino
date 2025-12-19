#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>

// --- DEFINITION DES NOTES ---
#define NOTE_DO4  262
#define NOTE_DO_DIESE4 277
#define NOTE_RE4  294
#define NOTE_MI4  330
#define NOTE_FA_DIESE4 370
#define NOTE_SOL4 392
#define NOTE_SOL_DIESE4 415
#define NOTE_LA4  440
#define NOTE_SI4  494
#define NOTE_DO5  523
#define NOTE_DO_DIESE5 554
#define NOTE_RE5  587
#define NOTE_MI5  659
#define NOTE_SOL5 784
#define NOTE_LA5  880

// --- CONFIGURATION ---
#define MAX_LONGUEUR_MSG 100 
#define MAX_LONGUEUR_PSEUDO 12

#define LARGEUR_ECRAN 128
#define HAUTEUR_ECRAN 64
Adafruit_SSD1306 display(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);

// --- BRANCHEMENTS (PINS) ---
const int PIN_ENCODEUR_CLK = 3; 
const int PIN_ENCODEUR_DT  = 4; 
const int PIN_ENCODEUR_SW  = 2;  
const int PIN_BOUTON_SW3   = A6; 

const int PIN_LED_R = 5;
const int PIN_LED_G = 6;
const int PIN_LED_B = 9;

const int PIN_CE  = 7;
const int PIN_CSN = 8;
const int PIN_BUZZER = 10; 

// --- RADIO ---
RF24 radio(PIN_CE, PIN_CSN);
const byte adresse[6] = "00001";

// --- CARTE MEMOIRE (EEPROM) ---
const int ADRESSE_CANAL      = 0;   
const int ADRESSE_SON        = 5;   
const int ADRESSE_ETAT       = 10;  
const int ADRESSE_PRIORITE   = 15;  
const int ADRESSE_CURSEUR    = 20;  
const int ADRESSE_CURSEUR_P  = 25;  
const int ADRESSE_PSEUDO     = 30;  
const int ADRESSE_MESSAGE    = 50;  

// VARIABLES GLOBALES
int canalActuel = 108; 
int profilSonore = 0;     

// Ajout d'un état pour le sous-menu réglages
enum Etat { ETAT_MENU, ETAT_MENU_REGLAGES, ETAT_ECRITURE, ETAT_PRIORITE, ETAT_EDIT_PSEUDO, ETAT_ENVOI, ETAT_LECTURE, ETAT_CONFIG_CANAL, ETAT_CONFIG_SON };
Etat etatActuel = ETAT_MENU;

// DONNEES TEXTE
char message[MAX_LONGUEUR_MSG + 1];           
char messageRecu[MAX_LONGUEUR_MSG + 1];  
int positionCurseur = 0; 

char monPseudo[MAX_LONGUEUR_PSEUDO + 1]; 
char pseudoRecu[MAX_LONGUEUR_PSEUDO + 1];
int curseurPseudo = 0; 

const char listeCaracteres[] = "< abcdefghijklmnopqrstuvwxyz\x85\x82\x8A\x88\x87 ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?!";
const int tailleListe = sizeof(listeCaracteres) - 1;
int indexCaractereSelectionne = 1; 

int optionMenu = 0; 
int optionSousMenu = 0; // Pour le menu réglages
int prioriteMessage = 1; 
int prioriteRecue = 1;

unsigned long dernierAppuiBouton = 0;
int dernierEtatClk;

// --- PROTOTYPES ---
void changerCouleurLed(int r, int g, int b);
void biper();
void jouerMelodie(bool modeDemo);
int lireRotationEncodeur();
void envoyerMessage();
void verifierReception();
void afficherMenu();
void afficherMenuReglages(); // NOUVEAU
void afficherEcritureMessage();
void afficherEcriturePseudo();
void afficherChoixPriorite();
void afficherMessageRecu();
void afficherConfigCanal(); 
void afficherConfigSon(); 
void chargerSauvegarde(); 
void sauvegarderTout(); 

// --- IMPLEMENTATION ---

void changerCouleurLed(int r, int g, int b) {
  analogWrite(PIN_LED_R, r);
  analogWrite(PIN_LED_G, g);
  analogWrite(PIN_LED_B, b);
}

void biper() {
  tone(PIN_BUZZER, 2000, 50); 
}

// --- GESTION DE LA MEMOIRE ---

void chargerSauvegarde() {
  int canalSauvegarde, sonSauvegarde;
  EEPROM.get(ADRESSE_CANAL, canalSauvegarde);
  EEPROM.get(ADRESSE_SON, sonSauvegarde);
  
  if (canalSauvegarde >= 0 && canalSauvegarde <= 125) canalActuel = canalSauvegarde;
  if (sonSauvegarde >= 0 && sonSauvegarde <= 2) profilSonore = sonSauvegarde;

  int etatSauvegarde;
  EEPROM.get(ADRESSE_ETAT, etatSauvegarde);
  
  if (etatSauvegarde >= 0 && etatSauvegarde <= 8) { // 8 car on a ajouté un état
    etatActuel = (Etat)etatSauvegarde;
  } else {
    etatActuel = ETAT_MENU;
  }

  EEPROM.get(ADRESSE_PRIORITE, prioriteMessage);
  EEPROM.get(ADRESSE_CURSEUR, positionCurseur);
  EEPROM.get(ADRESSE_CURSEUR_P, curseurPseudo);
  
  for (int i = 0; i < MAX_LONGUEUR_PSEUDO; i++) monPseudo[i] = EEPROM.read(ADRESSE_PSEUDO + i);
  monPseudo[MAX_LONGUEUR_PSEUDO] = '\0'; 
  
  for (int i = 0; i < MAX_LONGUEUR_MSG; i++) message[i] = EEPROM.read(ADRESSE_MESSAGE + i);
  message[MAX_LONGUEUR_MSG] = '\0'; 
}

void sauvegarderTout() {
  EEPROM.put(ADRESSE_CANAL, canalActuel);
  EEPROM.put(ADRESSE_SON, profilSonore);
  EEPROM.put(ADRESSE_ETAT, (int)etatActuel);
  EEPROM.put(ADRESSE_PRIORITE, prioriteMessage);
  EEPROM.put(ADRESSE_CURSEUR, positionCurseur);
  EEPROM.put(ADRESSE_CURSEUR_P, curseurPseudo);

  for (int i = 0; i < MAX_LONGUEUR_PSEUDO; i++) EEPROM.update(ADRESSE_PSEUDO + i, monPseudo[i]);
  for (int i = 0; i < MAX_LONGUEUR_MSG; i++) EEPROM.update(ADRESSE_MESSAGE + i, message[i]);
}

void jouerMelodie(bool modeDemo) {
  if (profilSonore == 0) { // Retro
    int notes[] = {NOTE_DO4, NOTE_MI4, NOTE_SOL4, NOTE_DO5};
    int limite = modeDemo ? 3 : 4; 
    for (int i = 0; i < limite; i++) { tone(PIN_BUZZER, notes[i], 100); delay(120); }
  } 
  else if (profilSonore == 1) { // Nokia 3.5s
    int melodie[] = { 
      NOTE_MI5, NOTE_RE5, NOTE_FA_DIESE4, NOTE_SOL_DIESE4, 
      NOTE_DO_DIESE5, NOTE_SI4, NOTE_RE4, NOTE_MI4, 
      NOTE_SI4, NOTE_LA4, NOTE_DO_DIESE4, NOTE_MI4, NOTE_LA4 
    };
    int durees[] = { 8, 8, 4, 4, 8, 8, 4, 4, 8, 8, 4, 4, 2 };
    
    for (int i = 0; i < 13; i++) {
      int dureeNote = 1000 / durees[i]; 
      if (!modeDemo) changerCouleurLed(0, 0, 0); // Eteindre LED
      tone(PIN_BUZZER, melodie[i], dureeNote);
      delay(dureeNote * 0.30);
      if (!modeDemo) { // Rallumer LED (Flash)
         if (prioriteRecue == 2) changerCouleurLed(255, 0, 0);
         else if (prioriteRecue == 0) changerCouleurLed(0, 255, 0);
         else changerCouleurLed(0, 0, 255);
      }
      delay(dureeNote); 
      noTone(PIN_BUZZER);
    }
  } 
  else { // Alarme
    int limite = modeDemo ? 4 : 20; 
    for (int i = 0; i < limite; i++) { tone(PIN_BUZZER, (i % 2 == 0) ? NOTE_LA5 : NOTE_MI5, 100); delay(100); }
  }
  noTone(PIN_BUZZER); 
}

int lireRotationEncodeur() {
  int etatClkActuel = digitalRead(PIN_ENCODEUR_CLK);
  int resultat = 0;
  if (etatClkActuel != dernierEtatClk && etatClkActuel == 1) {
    if (digitalRead(PIN_ENCODEUR_DT) != etatClkActuel) resultat = 1;
    else resultat = -1;
  }
  dernierEtatClk = etatClkActuel;
  return resultat;
}

void envoyerMessage() {
  radio.stopListening();
  display.clearDisplay();
  display.setCursor(20, 20); display.print(F("ENVOI...")); display.display();
  
  int len = strlen(message);
  int index = 0;
  while (index < len) {
    char paquet[32] = {0}; 
    paquet[0] = 'T';
    int i = 0;
    for (i = 0; i < 31 && (index + i) < len; i++) { paquet[i + 1] = message[index + i]; }
    
    radio.write(paquet, 32); 
    index += i; delay(40);
  }
  char paquetFin[32] = {0};
  paquetFin[0] = 'F'; paquetFin[1] = prioriteMessage; 
  strncpy(&paquetFin[2], monPseudo, 29); 
  
  radio.write(paquetFin, 32);
  
  radio.startListening();
}

void verifierReception() {
  if (radio.available()) {
    char paquet[32] = {0};
    radio.read(paquet, 32);
    
    if (paquet[0] == 'T') {
      int len = strlen(messageRecu);
      if (len < MAX_LONGUEUR_MSG) strncat(messageRecu, &paquet[1], 31);
    } 
    else if (paquet[0] == 'F') {
      prioriteRecue = paquet[1];
      strncpy(pseudoRecu, &paquet[2], MAX_LONGUEUR_PSEUDO); 
      pseudoRecu[MAX_LONGUEUR_PSEUDO] = '\0';
      
      if (prioriteRecue == 2) changerCouleurLed(255, 0, 0);       
      else if (prioriteRecue == 0) changerCouleurLed(0, 255, 0); 
      else changerCouleurLed(0, 0, 255);                          
      
      jouerMelodie(false); 
      
      etatActuel = ETAT_LECTURE;
      sauvegarderTout(); 
      afficherMessageRecu();
    }
  }
}

// --- AFFICHAGES AVEC CADRES ---

void afficherMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(40, 0); display.println(F("- MENU -"));
  
  // Cadre autour de la sélection
  if (optionMenu == 0) display.drawRoundRect(0, 20, 128, 18, 4, SSD1306_WHITE);
  else if (optionMenu == 1) display.drawRoundRect(0, 42, 128, 18, 4, SSD1306_WHITE);

  display.setCursor(10, 25); display.print(F("Ecrire Message")); 
  display.setCursor(10, 47); display.print(F("Reglages"));
  
  display.display();
}

void afficherMenuReglages() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(30, 0); display.println(F("- REGLAGES -"));
  
  // Cadre dynamique
  int y = 15 + (optionSousMenu * 12);
  display.drawRoundRect(0, y-2, 128, 13, 3, SSD1306_WHITE);

  display.setCursor(10, 15); display.print(F("Canal"));
  display.setCursor(10, 27); display.print(F("Sonnerie"));
  display.setCursor(10, 39); display.print(F("Mon Pseudo"));
  display.setCursor(10, 51); display.print(F("Retour"));

  display.display();
}

void afficherConfigCanal() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0); display.println(F("CHOIX CANAL"));
  display.setTextSize(2);
  display.setCursor(40,25); display.print(canalActuel);
  display.setTextSize(1);
  display.setCursor(0,55); display.println(F("Clic -> VALIDER"));
  display.display();
}

void afficherConfigSon() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0); display.println(F("CHOIX SONNERIE"));
  display.setTextSize(2);
  display.setCursor(10,25); 
  if (profilSonore == 0) display.print(F("Retro"));
  else if (profilSonore == 1) display.print(F("Nokia"));
  else display.print(F("Alarme"));
  display.setTextSize(1);
  display.setCursor(0,55); display.println(F("Clic -> VALIDER"));
  display.display();
}

void afficherEcritureMessage() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.print(F("Message (")); display.print(positionCurseur); display.print(F(")"));
  display.setCursor(0, 15); display.print(message);
  display.fillRect(0, 50, 128, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  
  if (listeCaracteres[indexCaractereSelectionne] == '<') { display.setCursor(45, 53); display.print(F("EFFACER")); } 
  else { display.setCursor(60, 53); display.print(listeCaracteres[indexCaractereSelectionne]); }
  display.display();
}

void afficherEcriturePseudo() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.print(F("MON PSEUDO"));
  display.setCursor(0, 15); display.print(monPseudo);
  display.setCursor(0, 35); display.setTextSize(1); display.print(F("SW3 -> SAUVER"));
  display.fillRect(0, 50, 128, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  
  if (listeCaracteres[indexCaractereSelectionne] == '<') { display.setCursor(45, 53); display.print(F("RETOUR")); } 
  else { display.setCursor(60, 53); display.print(listeCaracteres[indexCaractereSelectionne]); }
  display.display();
}

void afficherChoixPriorite() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.println(F("URGENCE ?"));
  
  // Cadre autour de la sélection
  display.drawRoundRect(0, 25, 128, 20, 4, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(15, 28);
  switch(prioriteMessage) {
    case 0: display.print(F("Faible")); break;
    case 1: display.print(F("Normale")); break;
    case 2: display.print(F("Urgent")); break;
    case 3: display.print(F("Annuler")); break;
  }
  display.display();
}

void afficherMessageRecu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0); display.print(F("De:")); display.print(pseudoRecu);
  if (prioriteRecue == 2) { display.setCursor(100, 0); display.print(F("!")); }
  display.setCursor(0, 15); display.println(messageRecu);
  display.setCursor(0, 55); display.print(F("-> Menu"));
  display.display();
}

// --- SETUP & LOOP ---

void setup() {
  chargerSauvegarde(); 

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW); 
  pinMode(PIN_ENCODEUR_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODEUR_DT, INPUT_PULLUP);
  pinMode(PIN_ENCODEUR_SW, INPUT_PULLUP);
  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  changerCouleurLed(0, 0, 0);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        digitalWrite(PIN_LED_R, HIGH); while(1); 
    }
  }
  display.display(); delay(500); display.clearDisplay();

  if (!radio.begin()) {
    display.setCursor(0,0); display.print(F("ERR RADIO!")); display.display();
    while(1) { digitalWrite(PIN_LED_R, HIGH); delay(200); digitalWrite(PIN_LED_R, LOW); delay(200); }
  }
  
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setChannel(canalActuel); 
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(adresse);
  radio.openReadingPipe(1, adresse);
  radio.startListening();
  
  dernierEtatClk = digitalRead(PIN_ENCODEUR_CLK);
  
  switch(etatActuel) {
    case ETAT_MENU: afficherMenu(); break;
    case ETAT_MENU_REGLAGES: afficherMenuReglages(); break;
    case ETAT_ECRITURE: afficherEcritureMessage(); break;
    case ETAT_PRIORITE: afficherChoixPriorite(); break;
    case ETAT_EDIT_PSEUDO: afficherEcriturePseudo(); break;
    case ETAT_CONFIG_CANAL: afficherConfigCanal(); break;
    case ETAT_CONFIG_SON: afficherConfigSon(); break;
    case ETAT_LECTURE: afficherMessageRecu(); break;
    default: etatActuel = ETAT_MENU; afficherMenu(); break;
  }
}

void loop() {
  if (etatActuel != ETAT_ENVOI) {
    verifierReception();
  }

  int rotation = lireRotationEncodeur();
  bool clicEncodeur = (digitalRead(PIN_ENCODEUR_SW) == LOW);
  bool clicBoutonSW3 = (analogRead(PIN_BOUTON_SW3) < 100); 

  if ((clicEncodeur || clicBoutonSW3) && (millis() - dernierAppuiBouton > 200)) {
    dernierAppuiBouton = millis(); 
    biper(); 
  } else {
    clicEncodeur = false;
    clicBoutonSW3 = false;
  }

  // --- LOGIQUE D'ETATS MODIFIEE ---

  switch (etatActuel) {
    case ETAT_MENU:
      if (rotation != 0) {
        optionMenu += rotation;
        if (optionMenu < 0) optionMenu = 1; else if (optionMenu > 1) optionMenu = 0;
        afficherMenu();
      }
      if (clicEncodeur) {
        if (optionMenu == 0) { // ECRIRE MESSAGE
           etatActuel = ETAT_ECRITURE;
           positionCurseur = 0;
           memset(message, 0, sizeof(message)); 
           indexCaractereSelectionne = 1; 
           sauvegarderTout(); 
           afficherEcritureMessage();
        } else if (optionMenu == 1) { // REGLAGES
           etatActuel = ETAT_MENU_REGLAGES;
           optionSousMenu = 0;
           sauvegarderTout();
           afficherMenuReglages();
        }
      }
      break;

    case ETAT_MENU_REGLAGES: // NOUVEAU SOUS-MENU
      if (rotation != 0) {
        optionSousMenu += rotation;
        if (optionSousMenu < 0) optionSousMenu = 3; else if (optionSousMenu > 3) optionSousMenu = 0;
        afficherMenuReglages();
      }
      if (clicEncodeur) {
        if (optionSousMenu == 0) { // Config Canal
           etatActuel = ETAT_CONFIG_CANAL;
           sauvegarderTout();
           afficherConfigCanal();
        } else if (optionSousMenu == 1) { // Config Son
           etatActuel = ETAT_CONFIG_SON;
           sauvegarderTout();
           afficherConfigSon();
        } else if (optionSousMenu == 2) { // Config Pseudo
           etatActuel = ETAT_EDIT_PSEUDO;
           curseurPseudo = 0; // Reset curseur pour modif propre
           sauvegarderTout();
           afficherEcriturePseudo();
        } else if (optionSousMenu == 3) { // Retour
           etatActuel = ETAT_MENU;
           sauvegarderTout();
           afficherMenu();
        }
      }
      break;

    case ETAT_CONFIG_CANAL:
      if (rotation != 0) {
        canalActuel += rotation;
        if (canalActuel < 0) canalActuel = 125; else if (canalActuel > 125) canalActuel = 0;
        afficherConfigCanal();
      }
      if (clicEncodeur) {
        radio.stopListening();
        radio.setChannel(canalActuel);
        radio.startListening();
        changerCouleurLed(0, 255, 0); delay(100); changerCouleurLed(0,0,0);
        etatActuel = ETAT_MENU_REGLAGES; // Retour sous-menu
        sauvegarderTout(); 
        afficherMenuReglages();
      }
      break;

    case ETAT_CONFIG_SON:
      if (rotation != 0) {
        profilSonore += rotation;
        if (profilSonore < 0) profilSonore = 2; else if (profilSonore > 2) profilSonore = 0;
        afficherConfigSon(); 
        jouerMelodie(true); 
      }
      if (clicEncodeur) {
        changerCouleurLed(0, 255, 0); 
        tone(PIN_BUZZER, 2000, 100); delay(150); tone(PIN_BUZZER, 2000, 100);
        delay(200);
        changerCouleurLed(0,0,0);
        etatActuel = ETAT_MENU_REGLAGES; // Retour sous-menu
        sauvegarderTout(); 
        afficherMenuReglages();
      }
      break;

    case ETAT_ECRITURE:
      if (rotation != 0) {
        indexCaractereSelectionne += rotation;
        if (indexCaractereSelectionne >= tailleListe) indexCaractereSelectionne = 0;
        else if (indexCaractereSelectionne < 0) indexCaractereSelectionne = tailleListe - 1;
        afficherEcritureMessage();
      }
      if (clicEncodeur) {
        if (listeCaracteres[indexCaractereSelectionne] == '<') { 
           etatActuel = ETAT_MENU; 
           sauvegarderTout();
           afficherMenu(); 
        } 
        else if (positionCurseur < MAX_LONGUEUR_MSG) {
           message[positionCurseur] = listeCaracteres[indexCaractereSelectionne];
           positionCurseur++;
           message[positionCurseur] = '\0';
           sauvegarderTout(); 
           afficherEcritureMessage();
        }
      }
      if (clicBoutonSW3) {
        etatActuel = ETAT_PRIORITE;
        prioriteMessage = 1; 
        changerCouleurLed(0, 0, 255); 
        sauvegarderTout();
        afficherChoixPriorite();
      }
      break;

    case ETAT_PRIORITE:
      if (rotation != 0) {
        prioriteMessage += rotation;
        if (prioriteMessage < 0) prioriteMessage = 0; else if (prioriteMessage > 3) prioriteMessage = 3;
        
        if (prioriteMessage == 0) changerCouleurLed(0, 255, 0); 
        else if (prioriteMessage == 1) changerCouleurLed(0, 0, 255); 
        else if (prioriteMessage == 2) changerCouleurLed(255, 0, 0); 
        else if (prioriteMessage == 3) changerCouleurLed(0, 0, 0); 
        
        afficherChoixPriorite();
      }
      if (clicEncodeur) {
        if (prioriteMessage == 3) { // Annuler / Retour
          etatActuel = ETAT_ECRITURE; changerCouleurLed(0,0,0); 
          sauvegarderTout();
          afficherEcritureMessage(); 
        } else { // ENVOI DIRECT ! (Plus de pseudo ici)
          etatActuel = ETAT_ENVOI;
        }
      }
      break;

    case ETAT_EDIT_PSEUDO:
      if (rotation != 0) {
        indexCaractereSelectionne += rotation;
        if (indexCaractereSelectionne >= tailleListe) indexCaractereSelectionne = 0;
        else if (indexCaractereSelectionne < 0) indexCaractereSelectionne = tailleListe - 1;
        afficherEcriturePseudo();
      }
      if (clicEncodeur) {
        if (listeCaracteres[indexCaractereSelectionne] == '<') { // Retour sans sauver
           etatActuel = ETAT_MENU_REGLAGES; 
           sauvegarderTout();
           afficherMenuReglages(); 
        } 
        else if (curseurPseudo < MAX_LONGUEUR_PSEUDO) {
           monPseudo[curseurPseudo] = listeCaracteres[indexCaractereSelectionne];
           curseurPseudo++;
           monPseudo[curseurPseudo] = '\0';
           sauvegarderTout(); 
           afficherEcriturePseudo();
        }
      }
      if (clicBoutonSW3) { // Valider et Quitter
         etatActuel = ETAT_MENU_REGLAGES;
         sauvegarderTout();
         afficherMenuReglages();
      }
      break;

    case ETAT_ENVOI:
      envoyerMessage(); 
      delay(1000);
      etatActuel = ETAT_MENU;
      sauvegarderTout(); 
      afficherMenu();
      break;
      
    case ETAT_LECTURE:
      if (clicEncodeur || clicBoutonSW3) {
        changerCouleurLed(0,0,0); 
        memset(messageRecu, 0, sizeof(messageRecu)); 
        memset(pseudoRecu, 0, sizeof(pseudoRecu));
        etatActuel = ETAT_MENU;
        sauvegarderTout();
        afficherMenu();
      }
      break;
  }
}
