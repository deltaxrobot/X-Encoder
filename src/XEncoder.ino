#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_ADD 10

#define PIN_A 2
#define PIN_B 3
#define PIN_E 4

#define COMPARE_VALUE_TIMER OCR1A
#define turn_on_timer1 (TIMSK1 |= (1 << OCIE1A))
#define turn_off_timer1 (TIMSK1 &= ~(1 << OCIE1A))

bool is_string_completed = false;
String received_string = "";

volatile int64_t absolute_pulse;
int64_t last_absolute_pulse;
int32_t incremental_pulse;
long period;
long timer_cycle;
float ratio = 5.12;
bool e_stt;
bool is_auto_send_e_stt;
bool is_absolute_mode = true;
bool led_blink = false;

void setup()
{
  EEPROM.begin();
  EEPROM.get(EEPROM_ADD, ratio);

  Serial.begin(115200);
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_E, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_A), intterupt_a, RISING);

  init_timer1();

  if (ratio == 0)
    ratio = 5.12;
  //Serial.println(sizeof(absolute_pulse), 10);
}

void loop()
{
  if (is_auto_send_e_stt)
  {
    bool _e_stt = fast_read_pin(PIN_E);
    if (_e_stt != e_stt)
    {
      e_stt = _e_stt;
      Serial.println(!e_stt);
    }
  }
  serial_execute();
}

void init_timer1()
{
  noInterrupts();

  TCCR1A = TCCR1B = TCNT1 = 0;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS10);
  TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0) | (1 << COM1B1) | (1 << COM1B0));

  TCCR1B &= ~(1 << CS11);
  TCCR1B |= (1 << CS10);

  COMPARE_VALUE_TIMER = 15999;

  interrupts();
}

bool fast_read_pin(uint8_t pin)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);

  if (*portInputRegister(port) & bit)
    return true;
  return false;
}
void fast_write_pin(uint8_t pin, uint8_t val) {
	uint8_t bit = digitalPinToBitMask(pin);
	uint8_t port = digitalPinToPort(pin);
	volatile uint8_t *out;

	out = portOutputRegister(port);
	uint8_t oldSREG = SREG;
	cli();

	if (val == LOW) {
		*out &= ~bit;
	} else {
		*out |= bit;
	}

	SREG = oldSREG;
}

void intterupt_a()
{
  if (fast_read_pin(PIN_B))
  {
    absolute_pulse++;
  }
  else
  {
    absolute_pulse--;
  }
  if (absolute_pulse % 256 == 0)
  {
    led_blink = !led_blink;
    fast_write_pin(LED_BUILTIN, led_blink);
  }
}

ISR(TIMER1_COMPA_vect)
{
  timer_cycle += 1;
  if (timer_cycle == period)
  {
    timer_cycle = 0;
    Serial.print('P');
    if (is_absolute_mode)
    {
      if (ratio == 1)
      {
        Serial.println(absolute_pulse, 10);
      }
      else
      {
        Serial.println(absolute_pulse / ratio, 2);
      }
    }
    else
    {
      if (ratio == 1)
      {
        Serial.println(absolute_pulse - last_absolute_pulse, 10);
      }
      else
      {
        Serial.println((absolute_pulse - last_absolute_pulse) / ratio, 2);
      }
    }
    last_absolute_pulse = absolute_pulse;
  }
}

void serial_execute()
{

  while (Serial.available())
  {
    char inChar = (char)Serial.read();

    if (inChar == '\n')
    {
      is_string_completed = true;
      break;
    }

    received_string += inChar;
  }

  if (!is_string_completed)
    return;

  String message_buffer = received_string.substring(0, 4);
   //M316 select mode: 0 - absolute mode, 1 - relative mode, eg: M316 0
  if (message_buffer == "M316")
  {
    float _val = received_string.substring(5).toFloat();
    if (_val == 0)
    {
      is_absolute_mode = true;
    }
    else if (_val == 1)
    {
      is_absolute_mode = false;
    }
    absolute_pulse = 0;
    last_absolute_pulse = 0;
    incremental_pulse = 0;
    Serial.println("Ok");
  }
  //M317 T[time]: return pulses every [time] (ms)
  //M317 : return current pulses number
  else if (message_buffer == "M317") 
  {
    String keyval = received_string.substring(5);
    if (keyval == "")
    {
      turn_off_timer1;

      Serial.print('P');
      if (is_absolute_mode)
      {
        if (ratio == 1)
        {
          Serial.println(absolute_pulse, 10);
        }
        else
        {
          Serial.println(absolute_pulse / ratio, 2);
        }
        last_absolute_pulse = absolute_pulse;
      }
      else
      {
        incremental_pulse = absolute_pulse - last_absolute_pulse;
        last_absolute_pulse = absolute_pulse;
        if (ratio == 1)
        {
          Serial.println(incremental_pulse, 10);
        }
        else
        {
          Serial.println(incremental_pulse / ratio, 2);
        }
      }
    }
    else
    {
      period = received_string.substring(6).toInt(); //eg: keyval = "T100" -> period = 100
      Serial.println("Ok");
      turn_on_timer1;
    }
  }
  //M318 [num] : enter the ratio PULSES_PER_MM
  else if (message_buffer == "M318")
  {
    float _ra = received_string.substring(6).toFloat();
    if (_ra != ratio && _ra != 0)
    {
      ratio = _ra;
      EEPROM.put(EEPROM_ADD, ratio);
    }
    else
    {
      Serial.println(ratio);
    }
    Serial.println("Ok");
  }
  // M319 V: đọc cảm biến tiệm cận rồi in ra giá trị hiện tại của cảm biến (0 - 1)
  // M319 T: tự động in ra giá trị khi có thay đổi
  else if (message_buffer == "M319")
  {
    char mode = received_string.charAt(5);
    if (mode == 'V')
    {
      is_auto_send_e_stt = false;
      e_stt = fast_read_pin(PIN_E);
      Serial.println(!e_stt);
    }
    else if (mode == 'T')
    {
      is_auto_send_e_stt = true;
      Serial.println("Ok");
    }
  }

  is_string_completed = false;
  received_string = "";
}

// WARNING: You may need these function to build
//Print.cpp
// size_t Print::println(int64_t number, int base)
// {
//     size_t n = 0;
//     n += print(number, base);
//     n += println();
//     return n;
// }

// size_t Print::print(int64_t number, int base)
// {
//     size_t n = 0;
//     if (number < 0)
//     {
//         write('-');
//         number = -number;
//         n++;
//     }
//     n += print((uint64_t)number, base);
//     return n;
// }

// size_t Print::println(uint64_t number, int base)
// {
//     size_t n = 0;
//     n += print((uint64_t)number, base);
//     n += println();
//     return n;
// }

// size_t Print::print(uint64_t number, int base)
// {
//     size_t n = 0;
//     unsigned char buf[64];
//     uint8_t i = 0;

//     if (number == 0)
//     {
//         n += print((char)'0');
//         return n;
//     }

//     if (base < 2) base = 2;
//     else if (base > 16) base = 16;

//     while (number > 0)
//     {
//         uint64_t q = number/base;
//         buf[i++] = number - q*base;
//         number = q;
//     }

//     for (; i > 0; i--)
//     n += write((char) (buf[i - 1] < 10 ?
//     '0' + buf[i - 1] :
//     'A' + buf[i - 1] - 10));

//     return n;
// }
