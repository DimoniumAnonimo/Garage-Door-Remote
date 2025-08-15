#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#include "garble.h"

// board
#define CE_PIN 7
#define CSN_PIN 8
// common
#define GARAGE_ADDR {0xe2, 0x5a, 0x5b, 0x22, 0x51}
#define CAR_ADDR {0x5c, 0x94, 0xe6, 0x08, 0x74}
#define HOUSE_ADDR {0xeb, 0x94, 0xe6, 0x08, 0x74}
#define RUN_LEFT_REQ 0x12345678
#define RUN_RIGHT_REQ 0x87654321
#define RUN_BOTH_REQ 0x0f1e2d3c
#define CMD_SUCCESS 0xA6A543A4
#define PAYLOAD_SIZE sizeof(unsigned long)
// transmitter
#define GARAGE_PIPE 1
#define TX_TIMEOUT 500
#define NUM_ATTEMPTS 5

enum state_machine_steps
{
  SM_IDLE,
  SM_SEND_COMMAND,
  SM_WAIT_PROMPT,
  SM_SEND_RESPONSE,
  SM_WAIT_CONFIRM,
};

static void get_next_request(unsigned long *);
static void radio_send_data(unsigned long);
static unsigned long millis_elapsed_since(unsigned long);

RF24 radio(CE_PIN, CSN_PIN, 100000);
uint8_t garage_addr_bytes[5] = GARAGE_ADDR;
uint8_t car_addr_bytes[5] = CAR_ADDR;
uint8_t house_addr_bytes[5] = HOUSE_ADDR;
enum state_machine_steps current_step = SM_IDLE;
unsigned long current_request = RUN_LEFT_REQ;
byte attempts_remaining;
unsigned long last_attempt;
unsigned long prompt_response;

void setup() {
  Serial.begin(115200);
  printf_begin();
  delay(10);
  while (!radio.begin())
  {
    Serial.println(F("radio hardware is not responding!!"));
    delay(1000);
  }
  
  radio.setPALevel(RF24_PA_MIN);
  radio.setPayloadSize(PAYLOAD_SIZE);
  radio.openWritingPipe(car_addr_bytes);
  radio.openReadingPipe(GARAGE_PIPE, garage_addr_bytes);

  radio.stopListening(car_addr_bytes);

  //radio.printPrettyDetails();
  //while (1);
}

void loop()
{
	uint8_t pipe;
	switch (current_step)
	{
		case SM_IDLE:
			delay(5000);
			get_next_request(&current_request);
			attempts_remaining = NUM_ATTEMPTS;
			current_step = SM_SEND_COMMAND;
			break;
		case SM_SEND_COMMAND:
			radio_send_data(current_request);
			last_attempt = millis();
			attempts_remaining--;
			current_step = SM_WAIT_PROMPT;
			break;
		case SM_WAIT_PROMPT:
			if (radio.available(&pipe))
			{
				if (pipe == GARAGE_PIPE)
				{
					uint8_t bytes = radio.getPayloadSize();
					if (bytes == PAYLOAD_SIZE)
					{
						unsigned long prompt_in;
						radio.read(&prompt_in, bytes);
						prompt_response = calculate_response(prompt_in);
						Serial.print("prompt: 0x");
						Serial.print(prompt_in, HEX);
						Serial.print(" resp: 0x");
						Serial.println(prompt_response, HEX);
						attempts_remaining = NUM_ATTEMPTS;
						current_step = SM_SEND_RESPONSE;
					}
				}
			}
			else if (millis_elapsed_since(last_attempt) > TX_TIMEOUT)
			{
				if (attempts_remaining > 0)
				{
					current_step = SM_SEND_COMMAND;
				}
				else
				{
					Serial.println("failed to receive prompt");
					current_step = SM_IDLE;
				}
			}
			break;
		case SM_SEND_RESPONSE:
			radio_send_data(prompt_response);
			last_attempt = millis();
			attempts_remaining--;
			current_step = SM_WAIT_CONFIRM;
			break;
		case SM_WAIT_CONFIRM:
			if (radio.available(&pipe))
			{
				if (pipe == GARAGE_PIPE)
				{
					uint8_t bytes = radio.getPayloadSize();
					if (bytes == PAYLOAD_SIZE)
					{
						unsigned long result_in;
						radio.read(&result_in, bytes);
						Serial.print("confirmation: 0x");
						Serial.println(result_in, HEX);
						//if (result_in == CMD_SUCCESS)
						if (1)
						{
							//Serial.println("request successful");
							Serial.println("");
							current_step = SM_IDLE;
						}
					}
				}
			}
			else if (millis_elapsed_since(last_attempt) > TX_TIMEOUT)
			{
				if (attempts_remaining > 0)
				{
					current_step = SM_SEND_RESPONSE;
				}
				else
				{
					Serial.println("failed to receive confirmation");
					current_step = SM_IDLE;
				}
			}
			break;
		default:
			current_step = SM_IDLE;
			break;
	}
}

static void get_next_request(unsigned long *current_request)
{
  switch (*current_request)
  {
    case RUN_LEFT_REQ:
      *current_request = RUN_RIGHT_REQ;
      break;
    case RUN_RIGHT_REQ:
      *current_request = RUN_BOTH_REQ;
      break;
    case RUN_BOTH_REQ:
    default:
      *current_request = RUN_LEFT_REQ;
      break;
  }
}

static void radio_send_data(unsigned long data)
{
  radio.stopListening(car_addr_bytes);
  delay(5);
  Serial.print("sending 0x");
  Serial.println(data, HEX);
  bool report = radio.write(data, PAYLOAD_SIZE);
  delay(5);
  radio.startListening();
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
