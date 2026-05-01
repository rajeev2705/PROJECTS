#define in1 12
#define in2 11
#define in3 10
#define in4 9
#define F_led A3
#define B_led A4

#include<SoftwareSerial.h>

SoftwareSerial Serial(A0, A1); //if Tx,Rx Pin Not working just replace all Serial.--> myserial

void setup() {
  // put your setup code here, to run once:
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);   // outputs for motor driver
  pinMode(F_led, OUTPUT);
  pinMode(B_led, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);

  //myserial.begin(9600);    // Serial monitor
  Serial.begin(9600);  // for seing on serial monier
}

void loop() {
  delayMicroseconds(1);
  // put your main code here, to run repeatedly:
  if (Serial.available())   // serach for any signal receive on arduino
  {
    int v = Serial.read(); // read signal and save in v
    Serial.println(v);  // print v on serial moniter for knowingh if working or not
    checker(v);
    if ( v == 'U')
    {

      digitalWrite(B_led, HIGH);
      Serial.println("Back Light ON");

    }
    if ( v == 'u')
    {

      digitalWrite(B_led, LOW);
      Serial.println("Back Light OFF");
    }
    if ( v == 'W')
    {
      digitalWrite(F_led, HIGH);

      Serial.println("Front Light ON");

    }
    if ( v == 'w')
    {
      digitalWrite(F_led, LOW);
      Serial.println("Front Light OFF");

    }
    if ( v == 'F') // Forward
    {
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      Serial.println("FORWARD");

    }
    if ( v == 'B') // backward
    {
      digitalWrite(in1, LOW);
      digitalWrite(in2, HIGH);
      digitalWrite(in3, LOW);
      digitalWrite(in4, HIGH);
      Serial.println("BACKWARD");
    }
    if ( v == 'R') // right
    {
      digitalWrite(in1, LOW);
      digitalWrite(in2, HIGH);
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      Serial.println("LEFT");
    }
    if ( v == 'H') //backward left
    {
      digitalWrite(in1, LOW);
      digitalWrite(in2, HIGH);
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      Serial.println("LEFT");
    }
    if ( v == 'I') //  forward right
    {
      digitalWrite(in1, LOW);
      digitalWrite(in2, HIGH);
      digitalWrite(in3, HIGH);
      digitalWrite(in4, LOW);
      Serial.println("LEFT");
    }
    if ( v == 'L') // left
    {
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      digitalWrite(in3, LOW);
      digitalWrite(in4, HIGH);
      Serial.println("RIGHT");
    }
    if ( v == 'J') // backward right
    {
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      digitalWrite(in3, LOW);
      digitalWrite(in4, HIGH);
      Serial.println("RIGHT");
    }
    if ( v == 'G') // forward left
    {
      digitalWrite(in1, HIGH);
      digitalWrite(in2, LOW);
      digitalWrite(in3, LOW);
      digitalWrite(in4, HIGH);
      Serial.println("RIGHT");
    }
    if ( v == 'S') // Forward
    {
      digitalWrite(in1, LOW);
      digitalWrite(in2, LOW);
      digitalWrite(in3, LOW);
      digitalWrite(in4, LOW);
      Serial.println("STOP");
    }
  }

}


// Recieved data from transmiiter for Speed Control

// int mapToPWM(int inputValue) {
//   // Ensure the inputValue is within the valid range (0 to 10)
//   inputValue = constrain(inputValue, 0, 9);

//   // Map the input range (0 to 10) to the output range (0 to 255)
//   int pwmValue = map(inputValue, 0, 9, 0, 230);

//   return pwmValue;
// }

// int checker(int input)
// {
//   byte putter ;
//   if (input >= 0 && input <= 9) {
//     int temp = input;
//     putter  = mapToPWM(temp);
//     analogWrite(5, putter);
//     analogWrite(6, putter);
//     Serial.print(putter);
//   }
//   else {
//     analogWrite(5, 255);
//     analogWrite(6, 255);
//     Serial.print(input);
//   }
// }