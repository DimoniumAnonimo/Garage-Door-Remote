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
#define REQUEST_TIMEOUT 5000
#define RETRY_TRASNMISSION 200
#define PAYLOAD_SIZE sizeof(unsigned long)

static void enact_request(void);
static bool valid_request(void);
static void set_prompt_and_response(void);
static void send_prompt(void);
static unsigned long millis_elapsed_since(unsigned long);

RF24 radio(CE_PIN, CSN_PIN, 100000);
uint8_t pipe_addr[][5] = { GARAGE_RX_ADDR, CAR_TX_ADDR, HOUSE_TX_ADDR };
unsigned long prompt;
unsigned long payload_in;
unsigned long prompt_response;
unsigned long expected_response;
bool awaiting_response = false;
unsigned long request_millis;
unsigned long prompt_millis;
unsigned long active_request;

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("initializing radio");
  if (!radio.begin())
  {
    Serial.println(F("radio hardware is not responding!!"));
    while (1) {}  // hold in infinite loop
  }
  
  char cstr[64];
  sprintf(cstr, "Tx Addr: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
    pipe_addr[GARAGE_RX_NDX][0],
    pipe_addr[GARAGE_RX_NDX][1],
    pipe_addr[GARAGE_RX_NDX][2],
    pipe_addr[GARAGE_RX_NDX][3],
    pipe_addr[GARAGE_RX_NDX][4]);
  Serial.println(cstr);
  
  sprintf(cstr, "Rx Addr: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
    pipe_addr[CAR_TX_NDX][0],
    pipe_addr[CAR_TX_NDX][1],
    pipe_addr[CAR_TX_NDX][2],
    pipe_addr[CAR_TX_NDX][3],
    pipe_addr[CAR_TX_NDX][4]);
  Serial.println(cstr);
  
  radio.setPALevel(RF24_PA_LOW);
  radio.setPayloadSize(PAYLOAD_SIZE);
  radio.openWritingPipe(pipe_addr[GARAGE_RX_NDX]);
  radio.openReadingPipe(1, pipe_addr[CAR_TX_NDX]);
  //radio.openReadingPipe(1, pipe_addr[HOUSE_TX_NDX]);
  
  radio.startListening();
}

void loop() {
  uint8_t pipe;
  if (radio.available(&pipe))
  {
    uint8_t bytes = radio.getPayloadSize();
    if (bytes == PAYLOAD_SIZE)
    {
      radio.read(&payload_in, bytes);
      Serial.print("full payload rec: ");
      Serial.println(payload_in);
      if (awaiting_response)
      {
        prompt_response = payload_in;
        if (prompt_response == expected_response)
        {
          Serial.println("response confirmed");
          enact_request();
          active_request = 0;
          awaiting_response = false;
        }
        else
        {
          Serial.println("response does not match");
        }
      }
      else
      {
        active_request = payload_in;
        if (valid_request())
        {
          awaiting_response = true;
          request_millis = millis();
          set_prompt_and_response();
          send_prompt();
        }
      }
    }
    else
    {
      Serial.println("Malformed payload");
    }
  }
  else if (awaiting_response)
  {
    if (millis_elapsed_since(request_millis) >= REQUEST_TIMEOUT)
    {
      Serial.println("req timeout");
      awaiting_response = false;
    }
    if (millis_elapsed_since(prompt_millis) >= RETRY_TRASNMISSION)
    {
      Serial.println("same prompt sent");
      send_prompt();
    }
  }
}

static void enact_request()
{
  switch (active_request)
  {
    case RUN_LEFT_REQ:
      Serial.println("run left accepted");
      break;
    case RUN_RIGHT_REQ:
      Serial.println("run right accepted");
      break;
    case RUN_BOTH_REQ:
      Serial.println("run both accepted");
      break;
    default:
      Serial.println("Error: unrecognized command");
      break;
  }
}

static bool valid_request()
{
  if (active_request == RUN_LEFT_REQ)
  {
    Serial.println("run left recognized");
    return true;
  }
  if (active_request == RUN_RIGHT_REQ)
  {
    Serial.println("run right recognized");
    return true;
  }
  if (active_request == RUN_BOTH_REQ)
  {
    Serial.println("run both recognized");
    return true;
  }
    Serial.println("command not recognized");
  return false;
}

static void set_prompt_and_response()
{
  prompt = millis();
  expected_response = calculate_response(prompt);
  Serial.print("prompt: ");
  Serial.print(prompt);
  Serial.print(" expected response: ");
  Serial.println(expected_response);
}

static void send_prompt()
{
  radio.stopListening();
  bool report = radio.write(&prompt, PAYLOAD_SIZE);
  delay(5);
  radio.startListening();
  prompt_millis = millis();
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
