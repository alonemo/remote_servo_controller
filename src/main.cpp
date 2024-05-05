#include <RotaryEncoder.h>
#include <WiFi.h>
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
LiquidCrystal_I2C lcd(0x27, 16, 3);

#define up    0
#define right 1
#define down  2
#define left  3
#define enter 4
#define none  5

// CLK - 6
// DT - 7
int STEP = 5;
int position_step = 5;
int lastPos, newPos;
RotaryEncoder encoder(7, 6);
int default_speed = 1;

int Pin_VRX = 17; //пин оси x джойстика
int Pin_VRY = 16; //пин оси y джойстика
int pinSW = 2; //пин кнопки джойстика


const char* main_menu[] = {"Servo-1","Servo-2","Servo-3","Servo-4","Servo-5","Servo-6","Servo-7","Servo-8","Servo-9","Servo-10","Servo-11","Servo-12","Send commands"};

int current_menu_item;
String selected_menu_item;
int servo_move = 0;
int servo_speed = 1;

boolean has_selected_servo = false;
boolean has_selected_servo_speed = false;

const char* controller_menu[] = {"1.Main-menu", "2.POS STEP", "3.Default speed", "4.Reset Servos", "5.Stop script"};
int selected_controller_menu;

class Servo {
public:
    String name;
    int position;
    int speed;

    Servo(String name, int position, int speed)
        : name(name), position(position), speed(speed) {}
};

Servo servos[12] = {
        {"Servo-1", 0, 0},
        {"Servo-2", 0, 0},
        {"Servo-3", 0, 0},
        {"Servo-4", 0, 0},
        {"Servo-5", 0, 0},
        {"Servo-6", 0, 0},
        {"Servo-7", 0, 0},
        {"Servo-8", 0, 0},
        {"Servo-9", 0, 0},
        {"Servo-10", 0, 0},
        {"Servo-11", 0, 0},
        {"Servo-12", 0, 0}
};


const char* additional_menu[] = {"Save", "Exit"};
int current_additional_item = 0;
int last_joy_read;
int read_joystick() {
  int output = none;
  // read all joystick values
  int X_Axis = analogRead(Pin_VRX);     // read the x axis value
  int Y_Axis = analogRead(Pin_VRY);     // read the y axis value
  Y_Axis = map(Y_Axis, 0, 1023, 1023, 0);      // invert the input from the y axis so that pressing the stick forward gives larger values
  X_Axis = map(X_Axis, 0, 1023, 1023, 0); 
  int SwitchValue = digitalRead(pinSW);  // read the state of the switch
  SwitchValue = map(SwitchValue, 0, 1, 1, 0);  // invert the input from the switch to be high when pressed

  // default X -- -4200
  // default Y -- -4200
  // joystick up --> X -> -4200, Y -> 981
  // joystick down --> X -> -4200, Y -> -7168
  // joystick left --> X -> 989, Y -> -4200
  // joystick right --> X -> -7168, Y -> -4200

  if (SwitchValue == 1){
    output = enter;
  } else if (X_Axis <= -7000 && Y_Axis <= -4000) {
    output = right;
  } else if (X_Axis >= 900 && Y_Axis <= -4000) {
    output = left;
  } else if (Y_Axis >= 900 && X_Axis <= -4000) {
    output = up;
  } else if (Y_Axis <= -7000 && X_Axis <= -4000) {
    output = down;
  }
  return output;

}

void print_line(int line, String text) {
    lcd.setCursor(0, line);
    lcd.print("               ");
    lcd.setCursor(0, line);
    lcd.print(text);
}

void move_down(){
  if (current_menu_item <= 0){
    current_menu_item = sizeof(main_menu)/sizeof(main_menu[0])-1;
  } else {
    current_menu_item--;
  }  
}

void move_up(){
  if (current_menu_item == sizeof(main_menu)/sizeof(main_menu[0])-1){
    current_menu_item = 0;
  } else {
    current_menu_item++;
  }
}

void move_up_additional(){
  if (current_additional_item == sizeof(additional_menu)/sizeof(additional_menu[0])-1){
    current_additional_item = 0;
  } else {
    current_additional_item++;
  }
}

void move_down_additional(){
  if (current_additional_item <= 0){
    current_additional_item = sizeof(additional_menu)/sizeof(additional_menu[0])-1;
  } else {
    current_additional_item--;
  }  
}

void switch_servo_menu(){
  if(has_selected_servo_speed == false){
    has_selected_servo_speed = true;

    for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
      if(servos[i].name == main_menu[current_menu_item]){
        encoder.setPosition(servos[i].speed);
        break;
      }
    }
  } else {
    for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
      if(servos[i].name == main_menu[current_menu_item]){
        //  encoder.setPosition(servos[i].position/position_step);
        servos[i].speed = servo_speed;
        encoder.setPosition(servo_move/position_step);
        break;
      }
    }
    has_selected_servo_speed = false;
  }
}

void enter_servo_menu(){
  for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
      if(servos[i].name == main_menu[current_menu_item]){
         encoder.setPosition(servos[i].position/position_step);
        break;
      }
    }
 
  has_selected_servo = true;
}

void switch_menu_left(){
  lcd.clear();
  if(selected_controller_menu == 0){
    selected_controller_menu = sizeof(controller_menu)/sizeof(controller_menu[0])-1;
  } else {
    selected_controller_menu -= 1;
  }
  if (selected_controller_menu == 1){
    encoder.setPosition(position_step/STEP);
  } else if (selected_controller_menu == 2){
    lcd.setCursor(0,1);
    encoder.setPosition(default_speed);
  }
}

void send_stop_script_request(){
  if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      http.begin("https://servo-server.vercel.app/home/stopScript");
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{}");
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
}
}

void send_reset_servos_request(){
  if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      http.begin("https://servo-server.vercel.app/home/resetServos");
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST("{}");
      if (httpResponseCode>0) {
        for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
          servos[i].position = 0;
          servos[i].speed = 1;
        }
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
}
}

void switch_menu_right(){
  lcd.clear();
  if(selected_controller_menu == sizeof(controller_menu)/sizeof(controller_menu[0])-1){
    selected_controller_menu = 0;
  } else {
    selected_controller_menu += 1;
  }
  if (selected_controller_menu == 1){
    encoder.setPosition(position_step/STEP);
  } else if (selected_controller_menu == 2){
    lcd.setCursor(0,1);
    encoder.setPosition(default_speed);
  }
}

void menu_exit(){
  if(additional_menu[current_additional_item] == "Save"){
    for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
      if(servos[i].name == main_menu[current_menu_item]){
        servos[i].position = servo_move;
        servos[i].speed = servo_speed;
      }
    }
  }
  servo_speed = default_speed;
  servo_move = 0; 
  current_additional_item = 0;
  has_selected_servo = false;
  lcd.clear();
  selected_controller_menu = 0;
}

void get_positions_request(){
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      http.useHTTP10(true);
      http.begin("https://servo-server.vercel.app/home");
      int httpResponseCode = http.GET();
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, http.getStream());
      if (httpResponseCode>0) {
        String msg = doc["message"];
        for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
          int serv_pos = doc["servos"][i]["position"];
          int serv_speed = doc["servos"][i]["speed"];
          servos[i].position = serv_pos;
          servos[i].speed = serv_speed;
        }
        Serial.println(msg);
        Serial.println("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
}else {
      Serial.println("WiFi Disconnected");
    }
}

void send_new_positions_request(){
  JsonDocument doc;
  JsonArray array = doc["servos"].to<JsonArray>();
  for (int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
        JsonObject nested = array.createNestedObject();
        nested["name"] = servos[i].name;
        nested["position"] = servos[i].position;
        if(servos[i].speed == 0){
          nested["speed"] = default_speed;
        } else {
          nested["speed"] = servos[i].speed;
        }

  }
  String requestBody;
  serializeJson(doc, requestBody);
  if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      http.begin("https://servo-server.vercel.app/home/moveServo");
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode>0) {
        for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
          int serv_pos = doc["servos"][i]["position"];
          int serv_speed = doc["servos"][i]["speed"];
          servos[i].position = serv_pos;
          servos[i].speed = serv_speed;
        }
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
} else {
      Serial.println("WiFi Disconnected");
    }
}

void setup() {
  encoder.setPosition(10/STEP);
// WiFi.disconnect(true);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);
  WiFi.begin("RT-GPON-E7E7","Wa7yqc3AJi");
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.print(WiFi.status());
  }

  Serial.println("Connected:");
  Serial.println(WiFi.localIP());
  get_positions_request();

  lcd.begin(16, 2);
  lcd.init();
  lcd.setBacklight(HIGH);

  // set up joy pins
  pinMode(pinSW, INPUT_PULLUP);

  selected_controller_menu = 0;

  // Init vars
  current_menu_item = 0;
  last_joy_read = none;
}

void loop(){
  if (has_selected_servo == false && selected_controller_menu == 0){
    lcd.setCursor(0, 0);
    lcd.print(controller_menu[selected_controller_menu]);
    lcd.setCursor(0, 1);
    lcd.print(main_menu[current_menu_item]);
    int current_joy_read = read_joystick();
    if (current_joy_read != last_joy_read) {
      last_joy_read = current_joy_read;
      switch (current_joy_read) {
        case up:
          move_up();
          break;
        case down:
          move_down();
          break;
        case enter:
          if(current_menu_item == 12){
            send_new_positions_request();
            break;
          }
          enter_servo_menu();
          break;
        case left:
          switch_menu_left();
          break;
        case right:
          switch_menu_right();
          break;
        default: break;
      }
      print_line(1, main_menu[current_menu_item]);

      delay(100);   
  } 
  } else if(has_selected_servo == false && selected_controller_menu == 1){
    lcd.setCursor(0,1);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print(controller_menu[selected_controller_menu]);
    encoder.tick();
    position_step = encoder.getPosition()*STEP;
    if(position_step > 100){
      encoder.setPosition(20);
    } else if (position_step <= 0){
      encoder.setPosition(1);
    }
    lcd.print("");
    lcd.print("<");
    lcd.print(position_step);
    lcd.print(">     ");
    int current_joy_read = read_joystick();
    if (current_joy_read != last_joy_read) {
      last_joy_read = current_joy_read;
      switch (current_joy_read) {
        case right:
          switch_menu_right();
          break;
        case left:
          switch_menu_left();
          break;
        default: break;
      }
    }
  } else if (has_selected_servo == false && selected_controller_menu == 2){
    lcd.setCursor(0, 0);
    lcd.print(controller_menu[selected_controller_menu]);
    encoder.tick();
    default_speed = encoder.getPosition();
    if(default_speed > 50){
      encoder.setPosition(50);
    } else if (default_speed <= 0){
      encoder.setPosition(1);
    }
    for(int i = 0; i<sizeof(servos)/sizeof(servos[0]); i++){
      servos[i].speed = default_speed;
    }
    lcd.setCursor(0,1);
    lcd.print("<");
    lcd.print(default_speed);
    lcd.print(">               ");
    int current_joy_read = read_joystick();
    if (current_joy_read != last_joy_read) {
      last_joy_read = current_joy_read;
      switch (current_joy_read) {
        case right:
          switch_menu_right();
          break;
        case left:
          switch_menu_left();
          break;
        default: break;
      }
    }
  } else if (has_selected_servo == false && selected_controller_menu == 3){
    lcd.setCursor(0, 0);
    lcd.print(controller_menu[selected_controller_menu]);
    lcd.print("             ");
    lcd.setCursor(0,1);
    lcd.print("                ");
    int current_joy_read = read_joystick();
    if (current_joy_read != last_joy_read) {
      last_joy_read = current_joy_read;
      switch (current_joy_read) {
        case right:
          switch_menu_right();
          break;
        case left:
          switch_menu_left();
          break;
        case enter:
          send_reset_servos_request();
          break;  
        default: break;
      }
    }
  } else if (has_selected_servo == false && selected_controller_menu == 4){
    lcd.setCursor(0, 0);
    lcd.print(controller_menu[selected_controller_menu]);
    lcd.print("             ");
    lcd.setCursor(0,1);
    lcd.print("                ");
    int current_joy_read = read_joystick();
    if (current_joy_read != last_joy_read) {
      last_joy_read = current_joy_read;
      switch (current_joy_read) {
        case right:
          switch_menu_right();
          break;
        case left:
          switch_menu_left();
          break;
        case enter:
          send_stop_script_request();
          break;  
        default: break;
      }
    }
  }else if(has_selected_servo == true && has_selected_servo_speed == false){
    selected_menu_item = main_menu[current_menu_item];
    lcd.setCursor(0, 0);
    lcd.print(selected_menu_item);
    encoder.tick();
    servo_move = encoder.getPosition()*position_step;
    if(servo_move == 0){
      lcd.print("<  ");
      lcd.print(servo_move);
      lcd.print("  >     ");
    } else if (servo_move <= -10 && servo_move > -100){
      lcd.print("< ");
      lcd.print(servo_move);
      lcd.print(" >    ");
    } else if (servo_move <= -100){
      lcd.print("<");
      lcd.print(servo_move);
      lcd.print(">     ");
    } else if (servo_move >= 10 && servo_move < 100){
      lcd.print("<  ");
      lcd.print(servo_move);
      lcd.print(" >    ");
    } else if (servo_move >= 100){
      lcd.print("<");
      lcd.print(servo_move);
      lcd.print(">    ");
    }

    int current_joy_read = read_joystick();
    if (current_joy_read != last_joy_read) {
      last_joy_read = current_joy_read;
      switch (current_joy_read) {
        case up:
          move_up_additional();
          break;
        case down:
          move_down_additional();
          break;
        case enter:
          menu_exit();
          break;
        case right:
          switch_servo_menu();
          break;
        case left:
          switch_servo_menu();
          break;
        default: break;
      }
      print_line(1, additional_menu[current_additional_item]);
      delay(100);
    }
  } else if (has_selected_servo == true && has_selected_servo_speed == true){
    selected_menu_item = main_menu[current_menu_item];
    lcd.setCursor(0, 0);
    lcd.print(selected_menu_item);
    lcd.print(" Speed    ");
    lcd.setCursor(0,1);
    encoder.tick();
    servo_speed = encoder.getPosition();
    if(servo_speed > 50){
      encoder.setPosition(50);
    } else if(servo_speed < 0){
      encoder.setPosition(1);
    }
    lcd.print("< ");
    lcd.print(servo_speed);
    lcd.print(" >            ");
    int current_joy_read = read_joystick();
    if (current_joy_read != last_joy_read) {
      last_joy_read = current_joy_read;
      switch (current_joy_read) {
        case right:
          switch_servo_menu();
          break;
        case left:
          switch_servo_menu();
          break;
        default: break;
      }
  } 
}
}