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
#define HOUSE_ADDR {0xeb, 0xf8, 0x5e, 0x1a, 0x54}
#define RUN_LEFT_REQ 0x12345678
#define RUN_RIGHT_REQ 0x87654321
#define RUN_BOTH_REQ 0x0f1e2d3c
#define CMD_SUCCESS 0xA6A543A4
#define PAYLOAD_SIZE sizeof(unsigned long)
// transmitter
#define CAR_PIPE 1
#define HOUSE_PIPE 2
#define TX_TIMEOUT 500
#define NUM_ATTEMPTS 5

enum state_machine_steps
{
  SM_WAIT_FOR_COMMAND,
  SM_SEND_PROMPT,
  SM_WAIT_RESPONSE,
  // SM_ENACT_COMMAND
  SM_CONFIRM_RESPONSE,
};

static void radio_send_data(unsigned long);
static void enact_command(unsigned long);
static bool valid_request(unsigned long);
static unsigned long millis_elapsed_since(unsigned long);

RF24 radio(CE_PIN, CSN_PIN, 100000);
uint8_t garage_addr_bytes[5] = GARAGE_ADDR;
uint8_t car_addr_bytes[5] = CAR_ADDR;
uint8_t house_addr_bytes[5] = HOUSE_ADDR;
enum state_machine_steps current_step = SM_WAIT_FOR_COMMAND;
unsigned long current_command;
unsigned long prompt;
unsigned long expected_response;
byte attempts_remaining;
unsigned long last_attempt;

void setup() {
  Serial.begin(115200);
  printf_begin();
  delay(10);
  while (!radio.begin())
  {
    Serial.println(F("radio hardware is not responding!!"));
    delay(1000);
  }
  
  radio.setPALevel(RF24_PA_LOW);
  radio.setPayloadSize(PAYLOAD_SIZE);
  radio.openWritingPipe(garage_addr_bytes);
  radio.openReadingPipe(CAR_PIPE, car_addr_bytes);
  //radio.openReadingPipe(HOUSE_PIPE, house_addr_bytes);
  //uint8_t wr_addr[5] = {0x11, 0x22, 0x33, 0x44, 0x55};
  //uint8_t rd_addr[5] = {0x55, 0x44, 0x33, 0x22, 0x11};
  //uint8_t b_addr[5] = {0x15, 0x24, 0x33, 0x42, 0x51};
  //radio.openWritingPipe(wr_addr);
  //radio.openReadingPipe(1, rd_addr);
  //radio.openReadingPipe(2, b_addr);

  radio.startListening();
  
  //radio.printPrettyDetails();
  //while (1);
}

void loop()
{
	uint8_t pipe;
  static unsigned long last_update = 0;
	switch (current_step)
	{
		case SM_WAIT_FOR_COMMAND:
			if (radio.available(&pipe))
			{
				if ((pipe == CAR_PIPE) || (pipe == HOUSE_PIPE))
				{
					uint8_t bytes = radio.getPayloadSize();
					if (bytes == PAYLOAD_SIZE)
					{
						radio.read(&current_command, bytes);
						if (valid_request(current_command))
						{
							prompt = millis();
							expected_response = calculate_response(prompt);
              Serial.print("req: 0x");
              Serial.print(current_command, HEX);
              Serial.print(" prompt: 0x");
              Serial.print(prompt, HEX);
              Serial.print(" resp: 0x");
              Serial.println(expected_response, HEX);
							attempts_remaining = NUM_ATTEMPTS;
							current_step = SM_SEND_PROMPT;
						}
					}
				}
			}
			break;
		case SM_SEND_PROMPT:
			radio_send_data(prompt);
			last_attempt = millis();
			attempts_remaining--;
			current_step = SM_WAIT_RESPONSE;
			break;
		case SM_WAIT_RESPONSE:
			if (radio.available(&pipe))
			{
				if ((pipe == CAR_PIPE) || (pipe == HOUSE_PIPE))
				{
					uint8_t bytes = radio.getPayloadSize();
					if (bytes == PAYLOAD_SIZE)
					{
						unsigned long prompt_response;
						radio.read(&prompt_response, bytes);
						//if (prompt_response == expected_response)
						if (1)
						{
							enact_command(current_command);
							current_step = SM_CONFIRM_RESPONSE;
						}
					}
				}
			}
			else if (millis_elapsed_since(last_attempt) > TX_TIMEOUT)
			{
				if (attempts_remaining > 0)
				{
					current_step = SM_SEND_PROMPT;
				}
				else
				{
					Serial.println("failed to receive response");
					current_step = SM_WAIT_FOR_COMMAND;
				}
			}
			break;
		case SM_CONFIRM_RESPONSE:
			for(int i = 0; i < NUM_ATTEMPTS; i++)
			{
				radio_send_data(CMD_SUCCESS);
				delay(TX_TIMEOUT);
			}
			current_step = SM_WAIT_FOR_COMMAND;
			break;
		default:
			current_step = SM_WAIT_FOR_COMMAND;
			break;
	}
}

static void radio_send_data(unsigned long data)
{
  Serial.print("sending 0x");
  Serial.println(data, HEX);
  radio.stopListening(garage_addr_bytes);
  bool report = radio.write(data, PAYLOAD_SIZE);
  delay(5);
  radio.startListening();
}

static void enact_command(unsigned long active_request)
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

static bool valid_request(unsigned long active_request)
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
  Serial.print("command not recognized: 0x");
  Serial.println(active_request, HEX);
  //return false;
  return true;
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
