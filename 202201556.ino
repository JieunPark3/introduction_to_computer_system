#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int GLED = A0;
const int YLED = 13;
const int RLED = 12;

const int tmpPin = A1; //온도센서
const int potPin = A2; //가변저항
const int cdsPin = A3; //조도센서

const int lButtonPin = 2; //왼쪽 버튼
const int mButtonPin = 3; //가운데 버튼
const int rButtonPin = 4; //오른쪽 버튼

const int motorInput1 = 5; //L293D 2번
const int motorInput2 = 6; //L293D 7번
const int motorEnablePin = 9; //L293D 1번

const int servoPin = 8; //서보 모터
const int buzzerPin = 11; //부저
const int echoPin = 10; //초음파 센서 echo
const int trigPin = 7; //초음파 센서 trig

Servo myServo;

bool isEmergency = false; //비상 여부
bool motorAutoMode = true; //모터 auto, manu모드

String climateMode = "NONE";
String controlMode = "A";
String stateDC = "OFF";
String dayOrNight = "DAY";

//LCD 업데이트 함수
void LCDupdate(int temperatureC, int setTemp) {
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temperatureC);
    lcd.print(" S:");
    lcd.print(setTemp);
    lcd.print(" ");
    lcd.print(climateMode); //heat or cool	
    lcd.print("  ");

    lcd.setCursor(0, 1);
    if (motorAutoMode) controlMode = "A";
    else controlMode = "M";
    lcd.print(controlMode); // A or M

    lcd.print("/DC:");
    if (stateDC == "ON") lcd.print("ON ");
    else lcd.print("OFF");

    lcd.print("/");
    if (dayOrNight == "DAY") lcd.print("DAY  ");
    else lcd.print("NIGHT");
}

void setup() {
    lcd.begin(16, 2);
    lcd.backlight();
    lcd.print("      START     "); //lcd 시작화면 출력

    pinMode(GLED, OUTPUT);
    pinMode(YLED, OUTPUT);
    pinMode(RLED, OUTPUT);
    pinMode(tmpPin, INPUT);
    pinMode(potPin, INPUT);
    pinMode(motorInput1, OUTPUT);
    pinMode(motorInput2, OUTPUT);
    pinMode(motorEnablePin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);

    pinMode(lButtonPin, INPUT);
    pinMode(mButtonPin, INPUT);
    pinMode(rButtonPin, INPUT);

    pinMode(echoPin, INPUT);
    pinMode(trigPin, OUTPUT);

    myServo.attach(servoPin); //서보모터 핀 연결
    myServo.write(0);

    Serial.begin(9600); //거리 확인용 Serial 시작

    delay(500);
    lcd.clear(); //lcd화면 지우기
}

void loop() {
    //센서값 읽기
    int tmpValue = analogRead(tmpPin); //온도센서 값 읽어오기
    int temperatureC = ((tmpValue * 5.0 / 1024.0) - 0.5) * 100;
    int potTempValue = analogRead(potPin); //가변 저항 값 읽어오기
    int setTemp = map(potTempValue, 0, 1023, 10, 40);

    //------조도센서로 낮과 밤 출력
    int cdsValue = analogRead(cdsPin);
    if (cdsValue < 500) dayOrNight = "NIGHT";
    else dayOrNight = "DAY";

    //---------비상버튼 
    if (digitalRead(rButtonPin) == HIGH) {
        delay(50); // 디바운싱
        isEmergency = !isEmergency;
        while (digitalRead(rButtonPin) == HIGH); // 뗄 때까지 대기
        lcd.clear();
    }

    //비상버튼이 눌렸을 경우
    if (isEmergency) {
        digitalWrite(RLED, HIGH); //RLED켜기
        digitalWrite(GLED, LOW); //에어컨 끄기
        digitalWrite(YLED, LOW); //히터 끄기
        digitalWrite(motorEnablePin, LOW); //DC모터 끄기
        digitalWrite(motorInput1, LOW);
        digitalWrite(motorInput2, LOW);
        myServo.write(90); //문 열리기
        tone(buzzerPin, 1000, 200); //부저로 경고음

        lcd.setCursor(0, 0);
        lcd.print("  EMERGENCY!!!  ");
    }
    else {
        noTone(buzzerPin); //부저 끄기
        digitalWrite(RLED, LOW); //RLED 끄기

        //----------DC모터 제어
        if (digitalRead(lButtonPin) == HIGH) {
            delay(200);
            motorAutoMode = !motorAutoMode; // 자동, 수동 전환
            while (digitalRead(lButtonPin) == HIGH);
            lcd.clear();
        }

        if (motorAutoMode) {
            digitalWrite(motorEnablePin, HIGH);
            digitalWrite(motorInput1, HIGH);
            digitalWrite(motorInput2, LOW);
            stateDC = "ON";
        }
        else {
            if (digitalRead(mButtonPin) == HIGH) {
                digitalWrite(motorEnablePin, HIGH);
                digitalWrite(motorInput1, HIGH);
                digitalWrite(motorInput2, LOW);
                stateDC = "ON";
            }
            else {
                digitalWrite(motorEnablePin, LOW);
                digitalWrite(motorInput1, LOW);
                digitalWrite(motorInput2, LOW);
                stateDC = "OFF";
            }
        }

        //-----------냉난방 조절
        if (temperatureC > setTemp + 5) { //설정 온도보다 높으면 에어컨 틀기
            digitalWrite(YLED, LOW);
            digitalWrite(GLED, HIGH);
            climateMode = "COOL";
        }
        else if (temperatureC < setTemp - 5) { //설정 온도보다 낮으면 히터 틀기
            digitalWrite(YLED, HIGH);
            digitalWrite(GLED, LOW);
            climateMode = "HEAT";
        }
        else {
            digitalWrite(YLED, LOW);
            digitalWrite(GLED, LOW);
            climateMode = "NONE";
        }

        //-----------서보 모터 제어
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);

        long duration = pulseIn(echoPin, HIGH, 30000); //타임아웃 추가
        long distance = duration / 29 / 2;

        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println("cm");

        if (distance > 0 && distance < 30) { //거리가 30cm이내 라면 서보모터로 문 열기
            myServo.write(90);
            digitalWrite(RLED, HIGH);
        }
        else {
            myServo.write(0);
            if (!isEmergency) digitalWrite(RLED, LOW);
        }

        LCDupdate(temperatureC, setTemp);
    }

    delay(200);
}
