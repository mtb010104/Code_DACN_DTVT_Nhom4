#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Adafruit_Fingerprint.h>


LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; 
byte colPins[COLS] = {27, 14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Chân điều khiển
const int relayPin = 2;
const int buzzerPin = 15;
const int buttonPin = 35;

// RFID
#define SS_PIN 5
#define RST_PIN 4
MFRC522 mfrc522(SS_PIN, RST_PIN);
#define MAX_CARDS 5
#define UID_SIZE 4

// UART2 ESP32 cho AS608
#define RXD2 16
#define TXD2 17
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

#define FP_DEFAULT_1_ID 1
#define FP_DEFAULT_2_ID 2
#define FP_MAX_ID 5  // tổng 5 vân tay


// UID đã lưu (thẻ xanh)
byte allowedUID[4] = {0x73, 0xA2, 0x97, 0x04};

// Menu chính
String menuItems[] = {
  "#:Check mode",
  "A:Change pass",
  "B:Add/del RFID",
  "C:Add/del FP",
  "D:Reset data"
};
int menuLength = sizeof(menuItems)/sizeof(menuItems[0]);
int menuIndex = 0; 

// Menu con cho chế độ A
String checkItems[] = {
  "0:Back to menu",
  "1:Check pass",
  "2:Check FP"
};
// Menu con cho chế độ B
String rfidItems[] = {
  "0:Back to menu",
  "1:Add RFID",
  "2:Del RFID"
};
// Menu con cho chế độ C
String fpItems[] = {
  "0:Back to menu",
  "1:Add FP",
  "2:Del FP"
};
// Menu con cho chế độ D (Reset menu)
String resetItems[] = {
  "0:Back to menu",
  "1:Reset Pass",
  "2:Reset RFID",
  "3:Reset FP",
  "4:Reset all"
};

int checkLength = sizeof(checkItems)/sizeof(checkItems[0]);
int checkIndex = 0;

int rfidLength = sizeof(rfidItems)/sizeof(rfidItems[0]);
int rfidIndex = 0;

int fpLength = sizeof(fpItems)/sizeof(fpItems[0]);
int fpIndex = 0;

int resetLength = sizeof(resetItems)/sizeof(resetItems[0]);
int resetIndex = 0;

// State
enum State { MAIN_MENU, CHECK_MENU, CHECK_PASS, CHECK_RFID, LOCKED, MASTER_PASS, CHANGE_OLD, CHANGE_NEW1, CHANGE_NEW2, RFID_MASTER, RFID_MENU, FP_MASTER, FP_MENU, RESET_MASTER, RESET_MENU  };
State currentState = MAIN_MENU;

// Mật khẩu
String masterPass = "1234";   // pass chính mở cửa
String adminPass  = "000A";   // mật khẩu master (đổi pass)
String enteredPass = "";
String newPass1 = "";
int wrongAttempts = 0;
int wrongNewAttempts = 0;   // thêm biến đếm sai khi nhập lại mật khẩu mới lần 2

// ====== Hiển thị menu chính ======
void showMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  if (menuIndex % 2 == 0) {
    lcd.print("->");
    lcd.print(menuItems[menuIndex]);
  } else {
    lcd.print("  ");
    lcd.print(menuItems[menuIndex-1]);
  }

  lcd.setCursor(0,1);
  if (menuIndex % 2 == 1) {
    lcd.print("->");
    lcd.print(menuItems[menuIndex]);
  } else {
    if (menuIndex + 1 < menuLength) {
      lcd.print("  ");
      lcd.print(menuItems[menuIndex+1]);
    }
  }
}

// ====== Hiển thị menu con kiểm tra ======
void showCheckMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("->");
  lcd.print(checkItems[checkIndex]);
  lcd.setCursor(0,1);
  if (checkIndex + 1 < checkLength) {
    lcd.print("  ");
    lcd.print(checkItems[checkIndex + 1]);
  } else {
    lcd.print("                "); // xóa dòng thứ 2
  }
}
// ====== Hiển thị menu con RFID ======
void showRFIDMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  // Dòng 1
  lcd.print("->");
  lcd.print(rfidItems[rfidIndex]);
  // Dòng 2
  lcd.setCursor(0,1);
  if (rfidIndex + 1 < rfidLength) {
    lcd.print("  ");
    lcd.print(rfidItems[rfidIndex + 1]);
  } else {
    lcd.print("                "); // dòng 2 trống
  }
}

// ====== Hiển thị menu con FP ======
void showFPMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  // Dòng 1
  lcd.print("->");
  lcd.print(fpItems[fpIndex]);
  // Dòng 2
  lcd.setCursor(0,1);
  if (fpIndex + 1 < fpLength) {
    lcd.print("  ");
    lcd.print(fpItems[fpIndex + 1]);
  } else {
    lcd.print("                "); // dòng 2 trống
  }
}
// ====== Hiển thị menu con reset ======
void showResetMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  // Dòng 1
  lcd.print("->");
  lcd.print(resetItems[resetIndex]);
  // Dòng 2
  lcd.setCursor(0,1);
  if (resetIndex + 1 < resetLength) {
    lcd.print("  ");
    lcd.print(resetItems[resetIndex + 1]);
  } else {
    lcd.print("                "); // dòng 2 trống
  }
}

// ====== Giao diện nhập ======
void startCheckPass() {
  enteredPass = "";
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Enter Pass:");
  lcd.setCursor(0,1);
}

void startCheckRFID() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scan your card");
  lcd.setCursor(0,1);
}

void startMasterPass() {
  enteredPass = "";
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Enter Master:");
  lcd.setCursor(0,1);
}

void startOldPass() {
  enteredPass = "";
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Old password:");
  lcd.setCursor(0,1);
}

void startNewPass1() {
  enteredPass = "";
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("New password:");
  lcd.setCursor(0,1);
}

void startNewPass2() {
  enteredPass = "";
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Repeat new:");
  lcd.setCursor(0,1);
}

// ====== In dấu * ======
void printHiddenChar(char c) {
  lcd.print(c);
  delay(200);  
  lcd.setCursor(enteredPass.length()-1,1);
  lcd.print("*");
}

// ====== Mở cửa ======
void openDoor() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Thank you");
  lcd.setCursor(0,1);
  lcd.print("Door Opened");
  Serial.println("Open success");

  digitalWrite(relayPin, HIGH);
  digitalWrite(buzzerPin, HIGH);
  delay(200);
  digitalWrite(buzzerPin, LOW);
  delay(5000);
  digitalWrite(relayPin, LOW);

  currentState = MAIN_MENU;
  showMenu();
}
// Hàm xóa ký tự cuối
void deleteLastChar() {
  if (enteredPass.length() > 0) {
    enteredPass.remove(enteredPass.length() - 1); // xóa ký tự cuối
    lcd.setCursor(0,1);
    lcd.print("                "); // xóa cả dòng
    lcd.setCursor(0,1);
    for (int i = 0; i < enteredPass.length(); i++) {
      lcd.print("*"); // in lại dấu *
    }
  }
}
// ====== Khóa hệ thống ======
void lockSystem() {
  currentState = LOCKED;
  for (int i = 10; i > 0; i--) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("System Locked!");
    lcd.setCursor(0,1);
    lcd.print("Wait ");
    lcd.print(i);
    lcd.print("s");
    if (i > 8) {
      digitalWrite(buzzerPin, HIGH);
    } else {
      digitalWrite(buzzerPin, LOW);
    }
    delay(1000);
  }
  digitalWrite(buzzerPin, LOW);
  wrongAttempts = 0;

  if (currentState == LOCKED) {
    currentState = MAIN_MENU;
    showMenu();
  }
}
// ====== Kiểm tra UID RFID ======
bool compareUID(byte *uid, byte *allowed) {
  for (byte i = 0; i < 4; i++) {
    if (uid[i] != allowed[i]) return false;
  }
  return true;
}
// Ghi UID vào EEPROM
void saveUID(int index, byte *uid) {
  for (int i = 0; i < UID_SIZE; i++) {
    EEPROM.write(index * UID_SIZE + i, uid[i]);
  }
  EEPROM.commit();
  delay(10);
}

// Đọc UID từ EEPROM
void readUID(int index, byte *uid) {
  for (int i = 0; i < UID_SIZE; i++) {
    uid[i] = EEPROM.read(index * UID_SIZE + i);
  }
}
// Tìm slot theo UID
int findSlotByUID(byte *uid) {
  byte stored[UID_SIZE];
  for (int i = 0; i < MAX_CARDS; i++) {
    readUID(i, stored);
    bool match = true;
    for (int j = 0; j < UID_SIZE; j++) {
      if (stored[j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return i; // trả về vị trí slot
  }
  return -1; // không tìm thấy
}

// Kiểm tra UID có tồn tại không
bool uidExists(byte *uid) {
  byte stored[UID_SIZE];
  for (int i = 0; i < MAX_CARDS; i++) {
    readUID(i, stored);
    bool match = true;
    for (int j = 0; j < UID_SIZE; j++) {
      if (stored[j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

// Tìm slot trống trong EEPROM
int findEmptySlot() {
  byte stored[UID_SIZE];
  for (int i = 0; i < MAX_CARDS; i++) {
    readUID(i, stored);
    bool empty = true;
    for (int j = 0; j < UID_SIZE; j++) {
      if (stored[j] != 0xFF) { // chưa được xóa
        empty = false;
        break;
      }
    }
    if (empty) return i;
  }
  return -1; // không còn slot
}
// Xóa UID khỏi EEPROM (ghi 0xFF)
void deleteUID(int index) {
  for (int i = 0; i < UID_SIZE; i++) {
    EEPROM.write(index * UID_SIZE + i, 0xFF);
  }
  EEPROM.commit();
  delay(10);
}
// ====== Hiển thị UID lên LCD ======
void displayUID(byte *uid) {
  lcd.setCursor(0,1);
  lcd.print("UID: ");
  for (byte i = 0; i < UID_SIZE; i++) {
    if (uid[i] < 0x10) lcd.print("0"); // in thêm số 0 phía trước nếu < 0x10
    lcd.print(uid[i], HEX);
    if (i < UID_SIZE-1) lcd.print(" ");
  }
  //delay(2000); // hiển thị 2s
}


// Vân tay
void setupFPDefaults() {
  Serial.println("Setting up default fingerprints...");

  for (int id = FP_DEFAULT_1_ID; id <= FP_DEFAULT_2_ID; id++) {
    if (finger.loadModel(id) == FINGERPRINT_OK) {  // kiểm tra ID đã có
      Serial.print("ID "); Serial.print(id); Serial.println(" already exists.");
    } else {
      Serial.print("Place finger for ID "); Serial.println(id);
      enrollFingerprint(id); // enroll mặc định
      Serial.println("Saved!");
    }
  }
}

// Hàm enroll 1 vân tay
int enrollFingerprint(int id) {
  int p = -1;
  lcd.clear();
  lcd.print("Place finger");
  
  // Vòng lặp chờ vân tay
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    else if (p != FINGERPRINT_OK) return p;
  }

  // ---- KIỂM TRA trùng vân tay ----
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) return p;

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("FP Exists!");
    lcd.setCursor(0,1);
    lcd.print("ID:"); lcd.print(finger.fingerID);
    delay(2000);
    return -1;  // Thoát, không enroll lại
  }
  // -------------------------------

  lcd.clear();
  lcd.print("Remove finger");
  delay(2000);

  // Tiếp tục quá trình enroll như code gốc
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

lcd.clear();
lcd.print("Place same FP");
p = -1;
while (true) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    else if (p != FINGERPRINT_OK) return p;

    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) return p;

    // Kiểm tra có cùng vân tay với lần 1 không
    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
        lcd.clear();
        lcd.print("Not same FP!");
        delay(1500);
        return -2; // quay về menu
    } else {
        break; // OK, cùng vân tay lần 1
    }
}


  p = finger.createModel();
  if (p != FINGERPRINT_OK) return p;

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("FP Saved ID:");
    lcd.print(id);
    delay(2000);
    return id;
  } else {
    return p;
  }
}


//Hàm kiểm tra vân tay
void checkFingerprint() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Place your FP");

  unsigned long startTime = millis();
  int p = FINGERPRINT_NOFINGER;
  int timeout = 10; // thời gian chờ 10 giây

  // Chờ tối đa 10 giây
  while (millis() - startTime < timeout * 1000) {
    p = finger.getImage();
    // Tính thời gian còn lại
    int timeLeft = timeout - (millis() - startTime) / 1000;
    lcd.setCursor(0,1);
    lcd.print("timeLeft: ");
    lcd.print(timeLeft);
    lcd.print("s   "); // xóa ký tự thừa

    if (p == FINGERPRINT_NOFINGER) {
      // vẫn chưa có vân tay, tiếp tục đợi
      delay(100);
      continue;
    } else if (p != FINGERPRINT_OK) {
      lcd.setCursor(0,1);
      lcd.print("Error, try again");
      delay(1000);
      lcd.setCursor(0,1);
      lcd.print("                ");
      startTime = millis(); // reset timeout khi lỗi
    } else {
      break; // có vân tay
    }
  }

  if (p == FINGERPRINT_NOFINGER) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("No finger!");
    digitalWrite(buzzerPin, HIGH);
    delay(2000);
    digitalWrite(buzzerPin, LOW);
    return;
  }

  // Chuyển hình ảnh sang template
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) { //có thể lỗi hoặc chưa có vân tay
    lcd.clear();
    lcd.print("Image Err");
    delay(1000);
    return;
  }

  // Tìm ID vân tay
p = finger.fingerFastSearch();
if (p == FINGERPRINT_OK) {
  if (finger.fingerID >= 1 && finger.fingerID <= FP_MAX_ID) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Finger OK!");
    lcd.setCursor(0,1);
    lcd.print("ID:");
    lcd.print(finger.fingerID);
    delay(1500);
    openDoor();   // mở cửa nếu hợp lệ
  } else {
    lcd.clear();
    lcd.print("ID not allowed");
    digitalWrite(buzzerPin, HIGH);
    delay(2000);
    digitalWrite(buzzerPin, LOW);
  }
  } else if (p == FINGERPRINT_NOTFOUND) {
    lcd.clear();
    lcd.print("Finger not found");
    digitalWrite(buzzerPin, HIGH);
    delay(2000);
    digitalWrite(buzzerPin, LOW);
    //currentState = CHECK_MENU; // quay về menu con
} else {
    lcd.clear();
    lcd.print("FP Error");
    digitalWrite(buzzerPin, HIGH);
    delay(2000);
    digitalWrite(buzzerPin, LOW);
    //currentState = CHECK_MENU; // quay về menu con
 }
currentState = CHECK_MENU;
showCheckMenu();  // quay về menu con hiển thị lại
}
//hàm kiểm tra ID đã có trong module chưa
bool fpIDExists(int id) {
  finger.getTemplateCount(); // cập nhật count
  for (int i = 1; i <= FP_MAX_ID; i++) {
    if (finger.loadModel(i) == FINGERPRINT_OK) { 
      if (i == id) return true;
    }
  }
  return false;
}

// ====== Hàm thêm vân tay ======
void addFingerprint() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Add FP ID 1-5:");
  String inputID = "";
  lcd.setCursor(0,1);
  lcd.print("ID: ");

  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key >= '1' && key <= '5') {
        inputID = key;
        lcd.setCursor(4,1);
        lcd.print(inputID);
        delay(1000);
        break;
      } else if (key == '0' || key == '*' || key == '#' || key == 'A' || key == 'B' || key == 'C' || key == 'D' || key > '5') {
        lcd.setCursor(0,1);
        lcd.print("Invalid ID!");
        delay(1000);
        lcd.setCursor(0,1);
        lcd.print("ID:          ");
      }
    }
  }

  int id = inputID.toInt();
  // Kiểm tra ID đã tồn tại chưa
if (fpIDExists(id)) {
  lcd.clear();
  lcd.print("ID Exists!");
  delay(1500);
  return; // thoát khỏi hàm, không enroll lại
}
  // Kiểm tra ID hợp lệ
  if (id < 1 || id > FP_MAX_ID) {
    lcd.clear();
    lcd.print("ID not valid");
    delay(1500);
    return;
  }
  // Bắt đầu quét lần 1
  lcd.clear();
  lcd.print("Place finger 1st");
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    else if (p != FINGERPRINT_OK) {
      lcd.setCursor(0,1);
      lcd.print("Scan Err");
      delay(1000);
      lcd.setCursor(0,1);
      lcd.print("        ");
      p = -1;
    }
  }
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Error 1st scan");
    delay(1500);
    return;
  }
  // **Thêm kiểm tra vân tay đã tồn tại**
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
   lcd.clear();
   lcd.print("Finger Exists!");
   lcd.setCursor(0,1);
   lcd.print("ID:"); lcd.print(finger.fingerID);
   delay(2000);
   return; // vân tay đã có, thoát
 }
  lcd.clear();
  lcd.print("Remove finger");
  delay(1500);

  // Quét lần 2
  lcd.clear();
  lcd.print("Place finger 2nd");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    else if (p != FINGERPRINT_OK) {
      lcd.setCursor(0,1);
      lcd.print("Scan Err");
      delay(1000);
      lcd.setCursor(0,1);
      lcd.print("        ");
      p = -1;
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Error 2nd scan");
    delay(1500);
    return;
  }

  // Tạo mẫu
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Create Failed");
    delay(1500);
    return;
  }

  // Lưu mẫu
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Saved ID:");
    lcd.setCursor(0,1);
    lcd.print(id);
    delay(2000);
  } else {
    lcd.clear();
    lcd.print("Save Failed");
    delay(1500);
  }
}
//Hàm xóa vân tay theo ID
void deleteFingerprint() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Del FP ID 1-5:");
  String inputID = "";
  lcd.setCursor(0,1);
  lcd.print("ID: ");

  // Nhập ID
  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key >= '1' && key <= '5') {
        inputID = key;
        lcd.setCursor(4,1);
        lcd.print(inputID);
        delay(1000);
        break;
      } else if (key == '0' || key == '*' || key == '#' || key == 'A' || key == 'B' || key == 'C' || key == 'D' || key > '5') {
        lcd.setCursor(0,1);
        lcd.print("Invalid ID!");
        delay(1000);
        lcd.setCursor(0,1);
        lcd.print("ID:          ");
      }
    }
  }

  int id = inputID.toInt();

  // Kiểm tra ID tồn tại
  if (!fpIDExists(id)) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ID not exist!");
    delay(1500);
    return;
  }

  // Yêu cầu xác nhận xóa
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Delete ID ");
  lcd.print(id);
  lcd.setCursor(0,1);
  lcd.print("#:Yes  *:No");

  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') { // Xác nhận xóa
        if (finger.deleteModel(id) == FINGERPRINT_OK) {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Deleted OK!");
          lcd.setCursor(0,1);
          lcd.print("ID:");
          lcd.print(id);
          delay(2000);
        } else {
          lcd.clear();
          lcd.print("Delete Failed!");
          delay(2000);
        }
        break; // thoát khỏi vòng while
      } else if (key == '*') { // Hủy xóa
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Delete canceled");
        delay(1500);
        break;
      }
    }
  }

  // Quay lại menu FP
  currentState = FP_MENU;
  showFPMenu();
}
// ===== Hàm khôi phục 2 thẻ RFID mặc định =====
void restoreDefaultRFID() {
  // Xóa toàn bộ EEPROM trước khi ghi lại mặc định
  for (int i = 0; i < MAX_CARDS * UID_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }

  // Ghi lại 2 thẻ mặc định
  byte card1[UID_SIZE] = {0x73, 0xA2, 0x97, 0x04}; 
  byte card2[UID_SIZE] = {0x74, 0xDC, 0x6A, 0x05}; 
  saveUID(0, card1);
  saveUID(1, card2);

  EEPROM.commit(); // lưu thay đổi
}
void resetFingerprint() {
  lcd.clear();
  lcd.print("Resetting FP...");
  delay(1000);

  // Xóa các vân tay từ ID 3 → 5
  for (int id = 3; id <= 5; id++) {
    finger.deleteModel(id);
  }

  // ---- Enroll ID1 ----
  if (!fpIDExists(1)) {
    int result = -1;
    while (result != 1) {  // Lặp cho đến khi enroll thành công
      lcd.clear();
      lcd.print("Enroll ID1");
      delay(2000);
      result = enrollFingerprint(1); // trả về 1 nếu thành công, -1 nếu trùng FP
      if (result == -1) {
        lcd.clear();
        lcd.print("FP Exists! Retry");
        delay(1500);
      }
    }
  }

  // ---- Enroll ID2 ----
  if (!fpIDExists(2)) {
    int result = -1;
    while (result != 2) {
      lcd.clear();
      lcd.print("Enroll ID2");
      delay(2000);
      result = enrollFingerprint(2); // trả về 2 nếu thành công, -1 nếu trùng FP
      if (result == -1) {
        lcd.clear();
        lcd.print("FP Exists! Retry");
        delay(1500);
      }
    }
  }
}




// ====== Setup ======
void setup() {
  Serial.begin(115200);
  EEPROM.begin(UID_SIZE * MAX_CARDS);
  mySerial.begin(57600, SERIAL_8N1, RXD2, TXD2);
  
  // Kiểm tra toàn bộ EEPROM có trống hay không
  bool allEmpty = true;
  for (int i = 0; i < MAX_CARDS * UID_SIZE; i++) {
    if (EEPROM.read(i) != 0xFF) {
      allEmpty = false;
      break;
    }
  }

   // Nếu trống -> khôi phục thẻ mặc định
  if (allEmpty) {
    restoreDefaultRFID();
  }

  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) delay(1);
  }
  // Kiểm tra và setup 2 vân tay mặc định
  setupFPDefaults();

  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("20251FE6008004");
  lcd.setCursor(0,1);
  lcd.print("Nhom_4-DACN DTVT");
  delay(4000);

  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  digitalWrite(relayPin, LOW);
  digitalWrite(buzzerPin, LOW);
  SPI.begin();
  mfrc522.PCD_Init();
  showMenu();
}

// ====== Loop ======
void loop() {
  char key = keypad.getKey();
  if (key) {
    digitalWrite(buzzerPin, HIGH);
    delay(50);
    digitalWrite(buzzerPin, LOW);

    // ---- MAIN MENU ----
    if (currentState == MAIN_MENU) {
      if (key == '*') {
        menuIndex++;
        if (menuIndex >= menuLength) menuIndex = 0;
        showMenu();
      } else if (key == '#') {
        currentState = CHECK_MENU;
        checkIndex = 0;
        showCheckMenu();
      } else if (key == 'A') {
        currentState = MASTER_PASS;
        startMasterPass();
      }
       else if (key == 'B') {
       currentState = RFID_MASTER; // yêu cầu nhập master pass
       startMasterPass();
      }
      else if (key == 'C') {
      currentState = FP_MASTER; // yêu cầu nhập master pass trước
      startMasterPass();
   }
   else if (key == 'D') {
    currentState = RESET_MASTER;  // yêu cầu nhập master pass trước khi reset
    startMasterPass();
  }
 }

    // ---- CHECK MENU ----
    else if (currentState == CHECK_MENU) {
      if (key == '*') {
        checkIndex++;
        if (checkIndex >= checkLength) checkIndex = 0;
        showCheckMenu();
      } else if (key == '0') {
        currentState = MAIN_MENU;
        showMenu();
      } else if (key == '1') {
        currentState = CHECK_PASS;
        startCheckPass();
      } else if (key == '2') {   // Check FP
       checkFingerprint();
       checkIndex = 0;
       showCheckMenu();
      }
    }

    // ---- CHECK PASS ----
    else if (currentState == CHECK_PASS) {
      if (key >= '0' && key <= '9') {
        enteredPass += key;
        printHiddenChar(key);
      } else if (key == '*') {
         deleteLastChar();   // Xóa 1 ký tự
      } else if (key == '#') {
        if (enteredPass == masterPass) {
          wrongAttempts = 0;
          lcd.clear();
          lcd.print("Password OK!");
          delay(1000);
          openDoor();
        } else {
          wrongAttempts++;
          lcd.clear();
          lcd.print("Wrong Pass!");
          delay(1000);

          if (wrongAttempts >= 3) {
            lockSystem();
          } else {
            startCheckPass();
          }
        }
      } else if (key == '0') {
        currentState = CHECK_MENU;
        showCheckMenu();
      }
    }
   
    // ---- MASTER PASS (bảo mật đổi pass) ----
    else if (currentState == MASTER_PASS) {
      if (key >= '0' && key <= '9'|| (key >= 'A' && key <= 'D')) {
        enteredPass += key;
        printHiddenChar(key);
      } else if (key == '*') {
        deleteLastChar();
      } else if (key == '#') {
        if (enteredPass == adminPass) {
          wrongAttempts = 0;
          lcd.clear();
          lcd.print("Master OK!");
          delay(1000);
          currentState = CHANGE_OLD;
          startOldPass();
        } else {
          wrongAttempts++;
          lcd.clear();
          lcd.print("Wrong Master!");
          delay(1000);

          if (wrongAttempts >= 3) {
            lockSystem();
          } else {
            startMasterPass();
          }
        }
      } else if (key == '0') {
        currentState = MAIN_MENU;
        showMenu();
      }
    }

    // ---- Nhập mật khẩu cũ ----
    else if (currentState == CHANGE_OLD) {
      if (key >= '0' && key <= '9') {
        enteredPass += key;
        printHiddenChar(key);
      } else if (key == '*') {
        deleteLastChar();
      } else if (key == '#') {
        if (enteredPass == masterPass) {
          wrongAttempts = 0;
          lcd.clear();
          lcd.print("Old OK!");
          delay(1000);
          currentState = CHANGE_NEW1;
          startNewPass1();
        } else {
          wrongAttempts++;
          lcd.clear();
          lcd.print("Wrong Old!");
          delay(1000);

          if (wrongAttempts >= 3) {
            lockSystem();
          } else {
            startOldPass();
          }
        }
      }
    }

    // ---- Nhập mật khẩu mới lần 1 ----
    else if (currentState == CHANGE_NEW1) {
      if (key >= '0' && key <= '9') {
        enteredPass += key;
        printHiddenChar(key);
      } else if (key == '*') {
         deleteLastChar();
      } else if (key == '#') {
        newPass1 = enteredPass;
         // Kiểm tra không được trùng mật khẩu cũ
    if (newPass1 == masterPass) {
      lcd.clear();
      lcd.print("Same as Old!");
      delay(1500);
      startNewPass1();
    } else {
      lcd.clear();
      lcd.print("Saved new1");
      delay(800);
      currentState = CHANGE_NEW2;
      startNewPass2();
    }
  }
}

    // ---- Nhập mật khẩu mới lần 2 ----
  else if (currentState == CHANGE_NEW2) {
  if (key >= '0' && key <= '9') {
    enteredPass += key;
    printHiddenChar(key);
  } else if (key == '*') {
    deleteLastChar();
  } else if (key == '#') {
    if (enteredPass == newPass1) {
      masterPass = newPass1;
      lcd.clear();
      lcd.print("Change Success!");
      delay(1500);
      wrongNewAttempts = 0;   // reset đếm sai
      currentState = MAIN_MENU;
      showMenu();
    } else {
      wrongNewAttempts++;
      lcd.clear();
      lcd.print("Not Match!");
      delay(1200);

      if (wrongNewAttempts >= 3) {
        // Sai quá 3 lần thì còi kêu cảnh báo 2s
        digitalWrite(buzzerPin, HIGH);
        delay(2000);
        digitalWrite(buzzerPin, LOW);
        wrongNewAttempts = 0;
        currentState = MAIN_MENU;
        showMenu();
      } else {
        // Cho nhập lại lần nữa
        startNewPass2();
      }
    }
  }
}
 // ---- RFID MASTER ---- 
else if (currentState == RFID_MASTER) {
  if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D')) {
    enteredPass += key;
    printHiddenChar(key);
  } else if (key == '*') {
    deleteLastChar();
  } else if (key == '#') {
    if (enteredPass == adminPass) {
      wrongAttempts = 0;
      lcd.clear();
      lcd.print("Master OK!");
      delay(1000);
      currentState = RFID_MENU;   // vào menu thêm/xóa RFID
      rfidIndex = 0;
      showRFIDMenu();
    } else {
      wrongAttempts++;
      lcd.clear();
      lcd.print("Wrong Master!");
      delay(1000);

      if (wrongAttempts >= 3) {
        lockSystem();
      } else {
        startMasterPass();
      }
    }
  } else if (key == '0') {
    currentState = MAIN_MENU;
    showMenu();
  }
}

   // ---- RFID MENU ----  <-- đặt ở đây
else if (currentState == RFID_MENU) {
  if (key == '*') {       // chỉ nhấn * mới dịch menu
    rfidIndex++;
      if (rfidIndex >= rfidLength) rfidIndex = 0;
      showRFIDMenu();
  } else if (key == '1') {
  lcd.clear();
  lcd.print("Scan new card");
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    // đợi quẹt thẻ
  }
  byte *newUID = mfrc522.uid.uidByte;

  if (uidExists(newUID)) {
    lcd.clear();
    lcd.print("Card Exists!");
    delay(2000);
  } else {
    int slot = findEmptySlot();
    if (slot != -1) {
      saveUID(slot, newUID);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Added OK!");
      displayUID(newUID);   // <-- hiển thị UID vừa thêm
      delay(2500);
      showRFIDMenu();
    } else {
      lcd.clear();
      lcd.print("Memory Full!");
      delay(2000);
    }
  }
  showRFIDMenu();
}
  else if (key == '2') {
  lcd.clear();
  lcd.print("Scan card del");
  while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    // đợi quẹt thẻ
  }
  byte *delUID = mfrc522.uid.uidByte;

  int slot = findSlotByUID(delUID);
  if (slot == -1) {
    lcd.clear();
    lcd.print("Not Exists!");
    delay(2000);
  } else {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Deleted OK!");
    deleteUID(slot);
    displayUID(delUID);
    delay(2500);
  }
  showRFIDMenu();
}
  else if (key == '0') {
    currentState = MAIN_MENU;
    showMenu();
  }
    }
  // ---- FP MASTER ----
  else if (currentState == FP_MASTER) {
  if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D')) {
    enteredPass += key;
    printHiddenChar(key);
  } else if (key == '*') {
    deleteLastChar();
  } else if (key == '#') {
    if (enteredPass == adminPass) {
      wrongAttempts = 0;
      lcd.clear();
      lcd.print("Master OK!");
      delay(1000);
      currentState = FP_MENU;   // vào menu thêm/xóa FP
      fpIndex = 0;
      showFPMenu();
    } else {
      wrongAttempts++;
      lcd.clear();
      lcd.print("Wrong Master!");
      delay(1000);

      if (wrongAttempts >= 3) {
        lockSystem();   // đợi 10s
      } else {
        startMasterPass();
      }
    }
  } else if (key == '0') {
    currentState = MAIN_MENU;
    showMenu();
  }
}
// ---- FP MENU----
  else if (currentState == FP_MENU) {
  if (key == '*') {
    fpIndex++;
    if (fpIndex >= fpLength) fpIndex = 0;
    showFPMenu();
  } else if (key == '1') {
    addFingerprint();
    currentState = FP_MENU;
    showFPMenu();
    showFPMenu();
  } else if (key == '2') {
    lcd.clear();
    lcd.print("Del Finger...");
    delay(500); // cho LCD hiển thị trước khi nhập ID
    deleteFingerprint(); // gọi hàm xóa vân tay đã viết
    currentState = FP_MENU;
    showFPMenu();
  } else if (key == '0') {
    currentState = MAIN_MENU;
    showMenu();
  }
}
// ---- RESET MASTER ----
else if (currentState == RESET_MASTER) {
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D')) {
        enteredPass += key;
        printHiddenChar(key);
    } else if (key == '*') {
        deleteLastChar();
    } else if (key == '#') {
        if (enteredPass == adminPass) {
            lcd.clear();
            lcd.print("Master OK!");
            delay(1000);
            // Sau khi nhập master đúng -> vào menu Reset
            currentState = RESET_MENU;
            resetIndex = 0;
            showResetMenu();
        } else {
            wrongAttempts++;
            lcd.clear();
            lcd.print("Wrong Master!");
            delay(1000);

            if (wrongAttempts >= 3) {
                lockSystem();
            } else {
                startMasterPass();
            }
        }
    } else if (key == '0') {
        currentState = MAIN_MENU;
        showMenu();
    }
}
// ---- RESET MENU ----
else if (currentState == RESET_MENU) {
  if (key == '*') {   
    resetIndex++;
    if (resetIndex >= resetLength) resetIndex = 0;
    showResetMenu();
  }
  else if (key == '0') {  // Back về menu chính
    currentState = MAIN_MENU;
    showMenu();
  } 
  else if (key == '1') {
  // Reset mật khẩu về mặc định
  masterPass = "1234";  // mật khẩu mặc định
  lcd.clear();
  lcd.print("Password Reset!");
  delay(1500);
  currentState = RESET_MENU;
  showResetMenu();
  } 
  else if (key == '2') {
     // Reset RFID
    restoreDefaultRFID();          // gọi hàm khôi phục 2 thẻ mặc định
    lcd.clear();
    lcd.print("RFID Reset Done");
    delay(1500);
    // quay lại menu reset
    currentState = RESET_MENU;
    showResetMenu();
  } 
  else if (key == '3') {
    // Reset FP
    resetFingerprint();
    lcd.clear();
   lcd.print("FP Reset Done");
   delay(2000);
   // Quay lại menu Reset
   currentState = RESET_MENU;
   showResetMenu();
  } 
  else if (key == '4') {
    // Reset All
    // 1. Reset mật khẩu về mặc định
    masterPass = "1234"; // mật khẩu mặc định
    // 2. Reset RFID
    restoreDefaultRFID(); // khôi phục 2 thẻ mặc định
     // 3. Reset vân tay
    resetFingerprint(); // xóa tất cả mẫu vân tay
    // 4. Thông báo người dùng
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("All Reset Done!");
    delay(2000);
    // 5. Quay lại menu reset
    currentState = RESET_MENU;
    showResetMenu();
  }
}

  } //dấu kết thúc if(key==)
  // Nút mở cửa bằng button chỉ khi ở menu chính
  if (currentState == MAIN_MENU && digitalRead(buttonPin) == LOW) {
    openDoor();
  }
  // ---- Quét RFID liên tục ngoài key ----
if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
  byte *uid = mfrc522.uid.uidByte;

  // Chỉ kiểm tra thẻ trong EEPROM
  if (uidExists(uid)) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Card OK!");
     lcd.setCursor(0,1);
    lcd.print("UID: ");
    for (byte i = 0; i < UID_SIZE; i++) {
      if (uid[i] < 0x10) lcd.print("0");
      lcd.print(uid[i], HEX);
      if (i < UID_SIZE - 1) lcd.print(" ");
    }
    delay(1500);
    openDoor();
  } else {
    lcd.clear();
    lcd.print("Unknown Card!");
    digitalWrite(buzzerPin, HIGH);
    delay(2000);
    digitalWrite(buzzerPin, LOW);
    currentState = MAIN_MENU;
    showMenu();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(5); 
}
 delay(50);
}

