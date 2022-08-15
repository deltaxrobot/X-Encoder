#include <Arduino.h>
#include "fastio.h"
#include "EEPROM.h"

#define DIA 64   // diameter (mm)
#define PPR 1024 // pulse per revolution

#define PIN_A 2
#define PIN_B 3
#define PIN_E 4

#define PULSE_PER_MM_ADDRESS 10
#define DEFAULT_PULSE_PER_MM 10.24f

#define COMPARE_VALUE_TIMER OCR1A
#define turn_on_timer1 (TIMSK1 |= (1 << OCIE1A))
#define turn_off_timer1 (TIMSK1 &= ~(1 << OCIE1A))

bool is_string_completed = false;
String received_string = "";

volatile int64_t absolute_pulse;
int64_t last_absolute_pulse;
int32_t incremental_pulse;
long period;
long timer_counter;
float pulse_per_mm;
bool e_stt;
bool is_auto_send_e_stt;
bool is_absolute_mode = true;
volatile bool is_timer_running = false;

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_E, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_A), intterupt_a, CHANGE);

  init_timer1();

  EEPROM.begin();
  EEPROM.get(PULSE_PER_MM_ADDRESS, pulse_per_mm);
  delay(3);
  if (pulse_per_mm == 0)
  {
    pulse_per_mm = DEFAULT_PULSE_PER_MM;
    EEPROM.put(PULSE_PER_MM_ADDRESS, pulse_per_mm);
  }

  Serial.println("Begin:");
  Serial.println(pulse_per_mm);
}

void loop()
{
  if (is_auto_send_e_stt)
  {
    bool _e_stt = READ(PIN_E);
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

void intterupt_a()
{
  if (READ(PIN_B))
  {
    if (READ(PIN_A))
    {
      absolute_pulse--;
    }
    else
    {
      absolute_pulse++;
    }
  }
  else
  {
    if (READ(PIN_A))
    {
      absolute_pulse++;
    }
    else
    {
      absolute_pulse--;
    }
  }
}

ISR(TIMER1_COMPA_vect)
{
  if (!is_timer_running)
    return;
  timer_counter += 1;
  if (timer_counter == period)
  {

    unsigned long per = micros();
    timer_counter = 0;
    Serial.print('P');

    if (is_absolute_mode)
    {
      Serial.println(absolute_pulse / pulse_per_mm, 4);
    }
    else
    {
      Serial.println((absolute_pulse - last_absolute_pulse) / pulse_per_mm, 4);
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
    else if (inChar != '\r')
    {
      received_string += inChar;
    }
  }

  if (!is_string_completed)
    return;

  is_timer_running = false;
  turn_off_timer1;

  if (received_string == "IsXEncoder")
  {
    Serial.println("YesXEncoder");
    is_string_completed = false;
    received_string = "";
    is_timer_running = true;
    turn_on_timer1;
    return;
  }

  String message_buffer = received_string.substring(0, 4);

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
    absolute_pulse = last_absolute_pulse = 0;
    incremental_pulse = 0;
    Serial.println("Ok");
  }
  else if (message_buffer == "M317")
  {
    if (received_string.length() < 5)
    {
      Serial.print('P');
      if (is_absolute_mode)
      {
        Serial.println(absolute_pulse / pulse_per_mm, 4);
        last_absolute_pulse = absolute_pulse;
      }
      else
      {
        incremental_pulse = absolute_pulse - last_absolute_pulse;
        last_absolute_pulse = absolute_pulse;
        Serial.println(incremental_pulse / pulse_per_mm, 4);
      }
    }
    else
    {
      int _per = received_string.substring(6).toInt(); // eg: "M317 T100" -> period = 100
      if (_per > 0)
        period = _per;
      timer_counter = 0;
      Serial.println("Ok");
      is_timer_running = true;
      turn_on_timer1;
    }
  }
  else if (message_buffer == "M318")
  {
    pulse_per_mm = received_string.substring(6).toFloat();
    EEPROM.put(PULSE_PER_MM_ADDRESS, pulse_per_mm);
    Serial.println("Ok");
    is_timer_running = true;
    turn_on_timer1;
  }
  else if (message_buffer == "M319")
  {
    char mode = received_string.charAt(5);
    if (mode == 'V')
    {
      is_auto_send_e_stt = false;
      e_stt = READ(PIN_E);
      Serial.println(!e_stt);
    }
    else if (mode == 'T')
    {
      is_auto_send_e_stt = true;
      Serial.println("Ok");
    }

    is_timer_running = true;
    turn_on_timer1;
  }
  else if (message_buffer == "M314")
  {
    absolute_pulse = last_absolute_pulse = 0;
    incremental_pulse = 0;
    timer_counter = 0;
    Serial.println("Ok");
  }

  is_string_completed = false;
  received_string = "";
}

// WARNING: You may need these function to build
// Print.h
//  size_t println(int64_t number, int base);
//  size_t print(int64_t number, int base);
//  size_t println(uint64_t number, int base);
//  size_t print(uint64_t number, int base);

// Print.cpp
//  size_t Print::println(int64_t number, int base)
//  {
//      size_t n = 0;
//      n += print(number, base);
//      n += println();
//      return n;
//  }

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
