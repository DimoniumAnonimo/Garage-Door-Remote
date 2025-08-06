#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include "garble.h"

#define CE_PIN 7
#define CSN_PIN 8
#define GARAGE_RX_NDX 0
#define GARAGE_RX_ADDR {0xe2, 0x5a, 0x5b, 0x22, 0x51}
#define CAR_TX_NDX 1
#define CAR_TX_ADDR {0x5c, 0x94, 0xe6, 0x08, 0x74}
#define HOUSE_TX_NDX 2
#define HOUSE_TX_ADDR {0xeb, 0xf8, 0x5e, 0x1a, 0x54}
#define RUN_LEFT_REQ 0x12345678
#define RUN_RIGHT_REQ 0x87654321
#define RUN_BOTH_REQ 0x0f1e2d3c
#define REQUEST_TIMEOUT 7000
#define RETRY_TRASNMISSION 2000
#define PAYLOAD_SIZE sizeof(unsigned long)

static unsigned long get_next_request(unsigned long);
static unsigned long millis_elapsed_since(unsigned long);
static void send_request(unsigned long*);
static unsigned long get_prompt_response(unsigned long);
static void send_prompt_response(unsigned long*);

RF24 radio(CE_PIN, CSN_PIN);
uint8_t pipe_addr[][5] = { GARAGE_RX_ADDR, CAR_TX_ADDR, HOUSE_TX_ADDR };

void setup() {
  Serial.begin(115200);
  delay(10);
  if (!radio.begin())
  {
    Serial.println(F("radio hardware is not responding!!"));
    //while (1) {}  // hold in infinite loop
  }
  
  char cstr[64];
  sprintf(cstr, "Tx Addr: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
    pipe_addr[CAR_TX_NDX][0],
    pipe_addr[CAR_TX_NDX][1],
    pipe_addr[CAR_TX_NDX][2],
    pipe_addr[CAR_TX_NDX][3],
    pipe_addr[CAR_TX_NDX][4]);
  Serial.println(cstr);
  
  sprintf(cstr, "Rx Addr: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
    pipe_addr[GARAGE_RX_NDX][0],
    pipe_addr[GARAGE_RX_NDX][1],
    pipe_addr[GARAGE_RX_NDX][2],
    pipe_addr[GARAGE_RX_NDX][3],
    pipe_addr[GARAGE_RX_NDX][4]);
  Serial.println(cstr);
  
  radio.setPALevel(RF24_PA_HIGH);
  radio.setPayloadSize(PAYLOAD_SIZE);
  radio.openWritingPipe(pipe_addr[CAR_TX_NDX]);
  radio.openReadingPipe(1, pipe_addr[GARAGE_RX_NDX]);

  radio.stopListening();
}

void loop() {
  unsigned long prompt_response;
  unsigned long last_send;
  static unsigned long current_request = 0;

  delay(10000);
  unsigned long new_req_millis = millis();
  current_request = get_next_request(current_request);
  Serial.print("new request start: ");
  Serial.println(current_request);
  bool prompt_rec = false;
  while (!prompt_rec && millis_elapsed_since(new_req_millis) < REQUEST_TIMEOUT)
  {
    Serial.println("send req");
    send_request(&current_request);
    last_send = millis();
    while (!prompt_rec && millis_elapsed_since(last_send) < RETRY_TRASNMISSION)
    {
      uint8_t pipe;
      if (radio.available(&pipe))
      {
        uint8_t bytes = radio.getPayloadSize();
        if (bytes == PAYLOAD_SIZE)
        {
          unsigned long prompt_in;
          radio.read(&prompt_in, bytes);
          prompt_response = get_prompt_response(prompt_in);
          prompt_rec = true;
        }
        else
        {
          Serial.println("bad payload");
        }
      }
    }
  }
  if (prompt_rec)
  {
    for (int i = 0; i < 10; i++)
    {
      Serial.println("send response");
      send_prompt_response(&prompt_response);
      delay(200);
    }
  }
  else
  {
    Serial.println("Req timeout");
  }
}

static unsigned long get_next_request(unsigned long current_request)
{
  switch (current_request)
  {
    case RUN_LEFT_REQ:
      return RUN_RIGHT_REQ;
      break;
    case RUN_RIGHT_REQ:
      return RUN_BOTH_REQ;
      break;
    case RUN_BOTH_REQ:
    default:
      return RUN_LEFT_REQ;
      break;
  }
}

static void send_request(unsigned long *current_request)
{
  radio.stopListening();
  bool report = radio.write(current_request, PAYLOAD_SIZE);
  delay(5);
  radio.startListening();
}

static unsigned long get_prompt_response(unsigned long prompt)
{
  unsigned long prompt_response = calculate_response(prompt);
  Serial.print("prompt: ");
  Serial.print(prompt);
  Serial.print(" || response: ");
  Serial.println(prompt_response);
  return prompt_response;
}

static void send_prompt_response(unsigned long *prompt_response)
{
  radio.stopListening();
  bool report = radio.write(prompt_response, PAYLOAD_SIZE);
}

static unsigned long millis_elapsed_since(unsigned long then)
{
  unsigned long now = millis();
  if (now < then)
  {
    return (0xFFFFFFFF - (then - now) + 1);
  }
  else
  {
    return now - then;
  }
}
