#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include "garble.h"

#define CE_PIN 7
#define CSN_PIN 8
#define GARAGE_RX_NDX 0
#define GARAGE_RX_ADDR "1Node"
#define CAR_TX_NDX 1
#define CAR_TX_ADDR "2Node"
#define HOUSE_TX_NDX 2
#define HOUSE_TX_ADDR "3Node"
#define RUN_LEFT_REQ 0x12345678
#define RUN_RIGHT_REQ 0x87654321
#define RUN_BOTH_REQ 0x0f1e2d3c
#define REQUEST_TIMEOUT 5000
#define RETRY_TRASNMISSION 1000
#define PAYLOAD_SIZE sizeof(unsigned long)

static unsigned long get_next_request(void);
static unsigned long millis_elapsed_since(unsigned long);
static void send_request(void);
static void set_prompt_response(unsigned long);
static void send_prompt_response(void);

enum request_state
{
	PAUSE_BETWEEN_REQUESTS,
	INIT_REQUEST,
	AWAIT_PROMPT,
	SEND_RESPONSE
};

RF24 radio(CE_PIN, CSN_PIN);
uint8_t pipe_addr[][6] = { GARAGE_RX_ADDR, CAR_TX_ADDR, HOUSE_TX_ADDR };
unsigned long current_request;
unsigned long prompt;
unsigned long prompt_response;
unsigned long request_sent_millis;
unsigned long start_of_request_millis;
enum request_state current_state = PAUSE_BETWEEN_REQUESTS;

void setup() {
  Serial.begin(115200);
  delay(10);
  if (!radio.begin())
  {
    Serial.println(F("radio hardware is not responding!!"));
    //while (1) {}  // hold in infinite loop
  }
  
  radio.setPALevel(RF24_PA_LOW);
  radio.setPayloadSize(PAYLOAD_SIZE);
  radio.openWritingPipe(pipe_addr[CAR_TX_NDX]);
  radio.openReadingPipe(0, pipe_addr[GARAGE_RX_NDX]);

  radio.stopListening();
}

void loop() {
  switch (current_state)
  {
    default:
    case PAUSE_BETWEEN_REQUESTS:
      delay(10000);
      start_of_request_millis = millis();
      current_state = INIT_REQUEST;
      break;
    case INIT_REQUEST:
      current_request = get_next_request();
      Serial.print("new request start: ");
      Serial.println(current_request);
      send_request();
      current_state = AWAIT_PROMPT;
      break;
    case AWAIT_PROMPT:
      uint8_t pipe;
      if (radio.available(&pipe))
      {
        uint8_t bytes = radio.getPayloadSize();
        if (bytes == PAYLOAD_SIZE)
        {
          unsigned long prompt;
          radio.read(&prompt, bytes);
          radio.stopListening();
          set_prompt_response(prompt);
          current_state = SEND_RESPONSE;
        }
        else
        {
          Serial.println("malformed payload");
        }
      }
      else if (millis_elapsed_since(request_sent_millis) >= RETRY_TRASNMISSION)
      {
        Serial.println("Same request resend");
        send_request();
      }
      break;
    case SEND_RESPONSE:
      for (int i = 0; i < 5; i++)
      {
        send_prompt_response();
        delay(RETRY_TRASNMISSION);
      }
      current_state = PAUSE_BETWEEN_REQUESTS;
      break;
  }

  if ((current_state != PAUSE_BETWEEN_REQUESTS) && (millis_elapsed_since(start_of_request_millis) >= REQUEST_TIMEOUT))
  {
    Serial.println("Req timeout");
    current_state = PAUSE_BETWEEN_REQUESTS;
  }
}

static unsigned long get_next_request()
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

static void send_request()
{
  radio.stopListening();
  bool report = radio.write(&current_request, PAYLOAD_SIZE);
  delay(5);
  radio.startListening();
  request_sent_millis = millis();
}

static void set_prompt_response(unsigned long prompt)
{
  prompt_response = calculate_response(prompt);
  Serial.print("prompt: ");
  Serial.print(prompt);
  Serial.print(" response: ");
  Serial.println(prompt_response);
}

static void send_prompt_response()
{
  radio.stopListening();
  bool report = radio.write(&prompt_response, PAYLOAD_SIZE);
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
