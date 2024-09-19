#include "LoRaWan_APP.h"
#include "Arduino.h"
#define Percentuale_Di_Successo 100  // percentulale di accettazione messaggio (a scopo di test)
#define RANDOM_DELAY_SIMULA_RETE 0
/* ------------*/
struct Payload_Fields {
  int ID_Sender;
  int16_t txNumber;
  //   int sensor1;
  float latitude;
  float longitude;
};
#define Payload_Syze sizeof(Payload_Fields)
Payload_Fields Rx_Message;
Payload_Fields TX_Message;
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


// #define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE 128  // Define the payload size here
#define WINDOW_LENGTH 10
char txpacket[WINDOW_LENGTH][BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

typedef enum {
  LOWPOWER,
  STATE_RX,
  STATE_TX
} States_t;

int16_t txNumber;
States_t state;
bool sleepMode = false;
int16_t Rssi, rxSize;
unsigned long int Actual_Time = 0;

long int index_Received_Packet = -1;
long int index_Packet_To_Send = 0;
void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  txNumber = 0;
  Rssi = 0;

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

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
  state = STATE_RX;
}


bool Messaggio_OK = true;
int Last_Received_PKT_Number = 0;
int NEW_Packet_received_Number = 0;
int ACK_SENT_Number = 0;
void loop() {
  Actual_Time = millis();
  switch (state) {
    case STATE_TX:
      // delay(1000);
      // txNumber++;
      //      if (Messaggio_OK==true) {
      Actual_Time = millis();


      if (index_Packet_To_Send <= index_Received_Packet) {
        Serial.printf(" \tACK SENT\t%d\t%d\tms\n", ACK_SENT_Number, Actual_Time);
        //   delay(random(500)); // -------------------------------

        Radio.Send((uint8_t *)txpacket[index_Packet_To_Send % WINDOW_LENGTH], strlen(txpacket[index_Packet_To_Send % WINDOW_LENGTH]));
        index_Packet_To_Send++;
        state = LOWPOWER;
      }
      //    } else {
      //      Serial.printf(" \tNO ACK SENT\n");
      //     state=STATE_RX;
      //    }

      break;
    case STATE_RX:
      //  Serial.println("into RX mode");
      Radio.Rx(0);
      state = LOWPOWER;
      break;
    case LOWPOWER:
      if (index_Packet_To_Send <= index_Received_Packet) {
        state = STATE_TX;
      //  Serial.printf("\nStato Rete POS #1 %d\n",Radio.GetStatus()); 
        delay((int) (random(100)>70)*RANDOM_DELAY_SIMULA_RETE);
      }
      // Serial.printf("\nStato Rete POS #2 %d\n",Radio.GetStatus()); 
      Radio.IrqProcess();
      break;
    default:
      break;
  }
}

void OnTxDone(void) {
  // Serial.print("TX done......");
  state = STATE_RX;
}

void OnTxTimeout(void) {
 // Radio.Sleep();
  //   Serial.print("TX Timeout......");
  state = STATE_TX;
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  int temp_ID_Sender;
  String Messaggio_Letto; 
  Rssi = rssi;
  rxSize = size;
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Radio.Sleep();

  sscanf(rxpacket, "S %d %d %s", &temp_ID_Sender, &NEW_Packet_received_Number,&Messaggio_Letto);
  Serial.printf("Messaggio_Letto =  %s", Messaggio_Letto);
   Serial.printf(">>>>>>>>>>>  %s\t Rssi=\t%d %s", rxpacket, Rssi);
  // sprintf(rxpacket,"Rssi = %d\t %s",Rssi,rxpacket );
  Messaggio_OK = (random(100) < Percentuale_Di_Successo);  // Inserire qui CRC e monitor
  if (Messaggio_OK) {
    index_Received_Packet++;
    sprintf(txpacket[index_Received_Packet % WINDOW_LENGTH], "ACK for < %s >\tRssi=\t%d\t%d\t", rxpacket, Rssi, Actual_Time);
    state = STATE_RX;
  } else {
    Serial.printf(" \tNO ACK SENT\n");
    state = STATE_RX;
  }
  //    Serial.printf("\n%sn\n",txpacket[NEW_Packet_received_Number % WINDOW_LENGTH]);

  // if (NEW_Packet_received_Number==Last_Received_PKT_Number){
  //   ACK_SENT_Number++;
  // }
  // else{
  //   ACK_SENT_Number=1;
  //   Last_Received_PKT_Number= NEW_Packet_received_Number;
  // }



  // Serial.println("wait to send next packet");
}