#include <Arduino.h>

#define DIA 64   //diameter (mm)
#define PPR 1024 //pulse per revolution

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
float ratio = 1.0;
bool e_stt;
bool is_auto_send_e_stt;
bool is_absolute_mode = true;

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_E, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_A), intterupt_a, RISING);

  init_timer1();
  Serial.println(sizeof(absolute_pulse), 10);
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
  if (Serial.available())
  {
    //Serial.flush();
    // received_string = Serial.readStringUntil('\n', Serial.available());
    // int last_index = received_string.length() - 1;
    // if (received_string[last_index] == '\n')
    // {
    //   is_string_completed = true;
    //   received_string.remove(last_index);
    // } else {
    //   Serial.println(received_string);
    // }
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
      Serial.println("Ok");
    }
    else if (message_buffer == "M317")
    {
      String keyval = received_string.substring(5);
      if (keyval == "")
      {
        turn_off_timer1;

        Serial.print('P');
        if (is_absolute_mode)
        {
          Serial.println(absolute_pulse, 10);
          last_absolute_pulse = absolute_pulse;
        }
        else
        {
          incremental_pulse = absolute_pulse - last_absolute_pulse;
          last_absolute_pulse = absolute_pulse;
          Serial.println(incremental_pulse, 10);
        }
      }
      else
      {
        period = received_string.substring(6).toInt(); //eg: keyval = "T100" -> period = 100

        //Serial.println(period);
        Serial.println("Ok");
        turn_on_timer1;
      }
    }
    else if (message_buffer == "M318")
    {
      ratio = received_string.substring(6).toFloat();
      Serial.println("Ok");
    }
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
}


// WARNING: You may need these function to build
