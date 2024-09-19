#include "LoRaWan_APP.h"
#include "Arduino.h"


#define MESSAGE_RESEND false  // true se si rispedisce il messaggio, false altrimenti

#define SENSOR_REPETITION_GENERATION 500  //800
#define RX_TIMEOUT_VALUE 500
#define PKT_TIMEOUT_VALUE 5000

/* ------------*/
struct Payload_Fields {
  int ID_Sender;
  int16_t txNumber;
  // int sensor1;
  float latitude;
  float longitude;
  unsigned long int Sensor_Generation_Time;
  bool ACK;
  unsigned long Timeout_Time;
  unsigned int Trasmission_Number; // Numero di ripetizioni per la spedizione del messaggio
//  char Message[128];  // interfaccia con Raspberry
  char DeviceMAC[24];
  char DeviceSNR[10];
  char Longitude[18];
  char Latitude[18];

};
#define Payload_Syze sizeof(Payload_Fields)
Payload_Fields Rx_Message;
Payload_Fields TX_Message;
int16_t NEW_Data_Generation(Payload_Fields *Payload, int16_t Tx_Number);
int16_t Last_Data_Acquired = 0;
int16_t Next_Data_to_Send = 0;

#define Payload_Syze sizeof(Payload_Fields)
#define Buffer_Data_Acquired 1000
Payload_Fields DATA_Acquired[Buffer_Data_Acquired];
// bool ACK_Received[Buffer_Data_Acquired];
/* ------------*/

#define RF_FREQUENCY 865000000  // Hz

#define TX_OUTPUT_POWER 5  // dBm

#define LORA_BANDWIDTH 0         // [0: 125 kHz, \
                                 //  1: 250 kHz, \
                                 //  2: 500 kHz, \
                                 //  3: Reserved]
#define LORA_SPREADING_FACTOR 7  // [SF7..SF12]
#define LORA_CODINGRATE 1        // [1: 4/5, \
                                 //  2: 4/6, \
                                 //  3: 4/7, \
                                 //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8   // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0    // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false


#define BUFFER_SIZE 128  // Define the payload size here

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

typedef enum {
  LOAD_NEW_WINDOW,
  LOWPOWER,
  STATE_RX,
  STATE_TX
} States_t;

int16_t txNumber;
States_t state;
bool sleepMode = false;
int16_t Rssi, rxSize;

#define WINDOW_LENGHT 10
int16_t Start_window_index = 0;
int16_t Stop_window_index = Start_window_index + WINDOW_LENGHT - 1;


int Tempo_Generazione_Dati() {
  // return 800+random(5*SENSOR_REPETITION_GENERATION);
  // return 500;
  return SENSOR_REPETITION_GENERATION;
}


void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  txNumber = 0;
  Rssi = 0;

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
  state = STATE_TX;
  // for (int i=0;i<Buffer_Data_Acquired;i++)
  //   Last_Data_Acquired = NEW_Data_Generation(&DATA_Acquired[Last_Data_Acquired], Last_Data_Acquired);

  Start_window_index = 0;
  Stop_window_index = Start_window_index + WINDOW_LENGHT - 1;
}

int Timeout = 0;
int Repeat_Sending = false;
unsigned int Number_OF_Repetition = 0;

unsigned long int Sensor_Generation_Time = 0;
unsigned long int Actual_Time = 0;
#define N_ID_Sender 3  // E' l'identificativo del dispositivo IoT Trasmettitore


// Simula la generazione di un dato del sensore
int16_t NEW_Data_Generation(Payload_Fields *Payload, int16_t Tx_Number) {
 
  //  ACK_Received[Tx_Number] = false;
  Payload->ID_Sender = N_ID_Sender;
  Payload->txNumber = Tx_Number;
  //  Payload->sensor1 = (int)random(300);
  Payload->latitude = (float)random(90000000) / 1000000;
  Payload->longitude = (float)random(90000000) / 1000000;
  Payload->Sensor_Generation_Time = millis();  // per statistica
  Payload->ACK = false;
  Payload->Timeout_Time = 0;
  Payload->Trasmission_Number = 0;
  int i_string;
  int i_copy;
  String temp_string;
  temp_string = Serial.readString(); 
// #BSSID:00:87:64:6d:da:00&-72dBm&41.1022,10.1212#

//   i_string=0;
//   while(temp_string[i_string]!='\0')
//         *(Payload->Message+i_string)=temp_string[++i_string];
// *(Payload->Message+i_string)='\0';
//   Serial.printf("----------%s \t\n",Payload->Message  );
  i_string=7;  i_copy=0;
while(temp_string[i_string]!='&')
    *(Payload->DeviceMAC+i_copy++)=temp_string[i_string++];
*(Payload->DeviceMAC+ i_copy)='\0';
i_copy=0;
while(temp_string[++i_string]!='&')
    *(Payload->DeviceSNR+i_copy++)=temp_string[i_string];
  *(Payload->DeviceSNR+ ++i_copy)='\0';
i_copy=0;
while(temp_string[++i_string]!=',')
    *(Payload->Longitude+i_copy++)=temp_string[i_string];
*(Payload->Longitude+ i_copy)='\0';

i_copy=0;
while(temp_string[++i_string]!='#')
     *(Payload->Latitude+i_copy++)=temp_string[i_string];
*(Payload->Latitude+ i_copy)='\0';

  Serial.printf("------MAC ----;%s; \t\n",Payload->DeviceMAC  );
  Serial.printf("------SNR ----;%s; \t\n",Payload->DeviceSNR  );
  Serial.printf("------Longitude ----;%s; \t\n",Payload->Longitude  );
  Serial.printf("------Latitude ----;%s; \t\n",Payload->Latitude  );

  
  return Payload->txNumber + 1;
}

bool Take_Decision = false;

void loop() {  // delay(2000);
  Actual_Time = millis();

  if (Actual_Time - Sensor_Generation_Time >= Tempo_Generazione_Dati()  && Serial.available()>0 ) {
    Serial.printf(""  );

    Last_Data_Acquired = NEW_Data_Generation(&DATA_Acquired[Last_Data_Acquired], Last_Data_Acquired);
    Sensor_Generation_Time = Actual_Time;
    // Serial.printf("\n-NEW DATA -: Time = %6.3f ms # %5.3d of %5.3d < %3d %3d %8f %8f >\n", (float)Actual_Time / 1000, Next_Data_to_Send, Last_Data_Acquired - 1, DATA_Acquired[Last_Data_Acquired - 1].ID_Sender, DATA_Acquired[Last_Data_Acquired - 1].txNumber, DATA_Acquired[Last_Data_Acquired - 1].latitude,DATA_Acquired[Last_Data_Acquired - 1].longitude);
  }
  if (Last_Data_Acquired > Stop_window_index) {
    switch (state) {
      case LOAD_NEW_WINDOW:

        break;
      case STATE_TX:
        if (Next_Data_to_Send <= Stop_window_index) {
          Actual_Time = millis();
          if ((DATA_Acquired[Next_Data_to_Send].ACK == false) && ((int)(DATA_Acquired[Next_Data_to_Send].Timeout_Time - millis()) <= 0)) {
            sprintf(txpacket, "S %3d %3d %8f %8f %s", 
                DATA_Acquired[Next_Data_to_Send].ID_Sender, 
                DATA_Acquired[Next_Data_to_Send].txNumber, 
                DATA_Acquired[Next_Data_to_Send].latitude, 
                DATA_Acquired[Next_Data_to_Send].longitude);
                //DATA_Acquired[Next_Data_to_Send].Message);

            DATA_Acquired[Next_Data_to_Send].Timeout_Time = millis() + PKT_TIMEOUT_VALUE;  // memorizzo l'istante in cui scadrà il timeout
            DATA_Acquired[Next_Data_to_Send].Trasmission_Number++;
            
            Serial.printf("TX\t%d\t%d\tms\tPKT=\t%d\t <%s> %s >>>>>>>>\t\n", DATA_Acquired[Next_Data_to_Send].Trasmission_Number, millis(), DATA_Acquired[Next_Data_to_Send].txNumber);
            //, txpacket,DATA_Acquired[Next_Data_to_Send].Message  );
            
            Radio.Send((uint8_t *)txpacket, strlen(txpacket));
            state = LOWPOWER;
          } else {
            state = STATE_RX;
          }
          Next_Data_to_Send++;
        } else {
          Take_Decision = true;
          state = STATE_RX;
        }
        break;
      case STATE_RX:
        Radio.Rx(0);
        Timeout = millis();
        state = LOWPOWER;
        break;
      case LOWPOWER:
        if (Take_Decision == true) {
          Take_Decision = false;
          Repeat_Sending = true;
          for (int16_t i = Start_window_index; i <= Stop_window_index; i++) {
            Repeat_Sending = Repeat_Sending && DATA_Acquired[i].ACK;
          }
          Repeat_Sending = !Repeat_Sending;  // falso se non si deve ripetere
                                             // #if MESSAGE_RESEND == true
                                             // ----------->>>                  Repeat_Sending=false;
          if (Repeat_Sending == false) {  // Carico una nuova finestra
            Start_window_index = Next_Data_to_Send;  // Anche Start_window_index = Stop_window_index+1;
            Stop_window_index = Start_window_index + WINDOW_LENGHT - 1;
            Next_Data_to_Send = Start_window_index;
            state = STATE_TX;
          } else {  // RETrasmission
 //               Serial.printf(" STATO RETE RITRASMISSIONE %d",     Radio.GetStatus());
            do {
              Next_Data_to_Send = Start_window_index;
              while (((DATA_Acquired[Next_Data_to_Send].ACK == true) || (DATA_Acquired[Next_Data_to_Send].ACK == false && ((int)(DATA_Acquired[Next_Data_to_Send].Timeout_Time - millis()) > 0)))
                     && Next_Data_to_Send < Stop_window_index)
                Next_Data_to_Send++;
            } while (DATA_Acquired[Next_Data_to_Send].ACK == false && ((int)(DATA_Acquired[Next_Data_to_Send].Timeout_Time - millis()) > 0));
              Repeat_Sending = false;
            Radio.Sleep();
            state = STATE_TX;
          }
        } else if (Radio.GetStatus() == RF_RX_RUNNING && (int)(millis() - Timeout) > RX_TIMEOUT_VALUE) {
          Radio.Sleep();
          state = STATE_TX;
        }
        Radio.IrqProcess();
        break;
      default:
        break;
    }
  }
}

void OnTxDone(void) {
//  Serial.printf("DEBUG : Interrupt  >>>>>>>>>>   TX DONE\n");
  Repeat_Sending = false;
  state = STATE_RX;
}

void OnTxTimeout(void) {
// Serial.printf("DEBUG : Interrupt  >>>>>>>>>>   TX Timeout\n");
  Radio.Sleep();
  Repeat_Sending = true;
  state = STATE_TX;
}

void OnRxTimeout(void) {
//  Serial.printf("DEBUG : Interrupt  >>>>>>>>>>   ||||||||||     >>>>>>>>>>>>>>>> RX Timeout\n");
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  int ID_Sender;
  int txNumber;
  //int sensor1;
  float latitude;
  float longitude;
  char Stringa[10];
  Rssi = rssi;
  rxSize = size;
//   Serial.printf("DEBUG : Interrupt  >>>>>>>>>>   RX DONE\n");
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Radio.Sleep();
  sscanf(rxpacket, "ACK for < S %d %d %d", &ID_Sender, &txNumber, &latitude, &longitude);

  // if (DATA_Acquired[Next_Data_to_Send - 1].txNumber == txNumber) {
  //   DATA_Acquired[Next_Data_to_Send - 1].ACK = true;
  //   //    Repeat_Sending = false;  // IL TXNUMBER dell'ACK è giusto
  //   Serial.printf("RX\t\t%d\tms\tPKT=\t%d\tRssi = \t%d\tACK\t   <%s> CORRECT\n", millis(), txNumber, Rssi, rxpacket);
  // }

   DATA_Acquired[txNumber].ACK = true;
   Serial.printf("RX\t\t%d\tms\tPKT=\t%d\tRssi = \t%d\tACK\t   <%s> CORRECT\n", millis(), txNumber, Rssi, rxpacket);
    Timeout = 0;
    state = STATE_TX;
}