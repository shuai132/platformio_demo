// Petoi Voice Command Module
// Doc: https://docs.petoi.com/extensible-modules/voice-command-module
// use the software serial port on the NyBoard to read the module. connect the module to the grove socket with pin 6 and 7.
// or use the serial 2 port on the BiBoard to read the module. connect the module to the pin Tx2 and Rx2.
// if you wire the module up with the USB programmer directly, connect the module's Tx to the programmer's Rx, and Rx to Tx.
// Rongzhong Li
// Petoi LLC
// Jan 12, 2023
#define SERIAL2_BAUD_RATE 9600
#define MAX_CUSTOMIZED_CMD 10

// Speak "start learning" to record your voice commands in order. You can record up to 10 voice commands
// Speak "stop learning" to stop in the middle
// Speak one of the recorded voice commands to trigger the reaction
// Speak "clear the learning data" to delete all the recordings at once. (you cannot delete a specific recording)
// The reactions below are already defined in the program. You may use SkillComposer to design new skills then import them into InstinctX.h
// Other serial commands are also supported, such as joint movements and melody

// 说”开始学习“开始录音，最多10条
// 说”结束学习“停止录入
// 说出口令触发反应
// 说”清除数据“删除所有自定义口令（无法删除单条口令）
// 下列行为是程序中预设的，您可以用技能创作坊设计新技能并导入到 InstinctX.h
// 支持其他的串口指令，比如活动关节和旋律

// #define VOICE_MODULE_SAMPLE
String customizedCmdList[] = {
  "T",                                                                     //call the last skill data sent by the Skill Composer
  "kpu1",                                                                  //single-handed pushups
  "m0 80 0 -80 0 0",                                                       //wave head
  "kmw",                                                                   //moonwalk
  "b14,8,14,8,21,8,21,8,23,8,23,8,21,4,19,8,19,8,18,8,18,8,16,8,16,8,14,4,\
  21,8,21,8,19,8,19,8,18,8,18,8,16,4,21,8,21,8,19,8,19,8,18,8,18,8,16,4,\
  14,8,14,8,21,8,21,8,23,8,23,8,21,4,19,8,19,8,18,8,18,8,16,8,16,8,14,4",  //twinkle star
  "6th",
  "7th",
  "8th",
  "9th",
  "10th"  //define up to 10 customized commands.
};
int listLength = 0;
bool enableVoiceQ = true;

void voiceSetup() {
  PTLF("Init voice");
  Serial2.begin(SERIAL2_BAUD_RATE);
  Serial2.setTimeout(5);
  listLength = min(int(sizeof(customizedCmdList) / sizeof(customizedCmdList[0])), MAX_CUSTOMIZED_CMD);
  PTLF("Number of customized voice commands on the main board: ");
  PTL(listLength);
}

void read_voice() {
  if (token == 'X' && newCmd[0] == 'A') {  // send some control command directly to the module
                                           // XAa: switch English
                                           // XAb: switch Chinese
                                           // XAc: turn on the sound response
                                           // XAd: turn off the sound response
                                           // XAe: start learning
                                           // XAf: stop learning
                                           // XAg: clear the learning data
    byte c = 0;
    while (newCmd[c++] != '~')
      ;
    newCmd[c - 1] = '\0';
    // Serial.print('X');
    // Serial.println(newCmd);
    Serial2.print('X');
    Serial2.println(newCmd);
    while (Serial2.available())
      PT(Serial2.read());
    PTL();
    if (!strcmp(newCmd, "Ac"))  // enter "XAc" in the serial monitor or add button "X65,99" in the mobile app to enable voice reactions
                                // 在串口监视器输入指令“XAc”或在手机app创建按键"X65,99"来激活语音动作
      enableVoiceQ = true;
    else if (!strcmp(newCmd, "Ad"))  // enter "XAd" in the serial monitor or add button "X65,100" in the mobile app to disable voice reactions
                                     // 在串口监视器输入指令“XAd”或在手机app创建按键"X65,100"来禁用语音动作
      enableVoiceQ = false;
    resetCmd();
  }

  if (Serial2.available()) {
    String raw = Serial2.readStringUntil('\n');
    PTL(raw);
    byte index = (byte)raw[2];  //interpret the 3rd byte as integer
    int shift = -1;
    if (index > 10 && index < 61) {
      if (index < 21) {  //11 ~ 20 are customized commands, and their indexes should be shifted by 11
        index -= 11;
        PT(index);
        PT(' ');
        if (index < listLength) {
          raw = customizedCmdList[index];
          token = raw[0];
          shift = 1;
        } else {
          PTLF("Undefined!");
        }
      } else if (index < 61) {  //21 ~ 60 are preset commands, and their indexes should be shifted by 21.
                                //But we don't need to use their indexes.
#ifdef VOICE_MODULE_SAMPLE
        token = T_SKILL;
        shift = 3;
#else
        token = raw[3];
        shift = 4;
#endif
      }
      if (enableVoiceQ) {
        const char *cmd = raw.c_str() + shift;
        tQueue->addTask(token, shift > 0 ? cmd : "", 2000);
        char end = cmd[strlen(cmd) - 1];
        if (!strcmp(cmd, "bk") || !strcmp(cmd, "x") || end >= 'A' && end <= 'Z') {
          tQueue->addTask('k', "up");
        }
      }
    } else {
      switch (tolower(index)) {
        case 'a':  //say "Bing-bing" to switch English /说“冰冰”切换英文
          {
            PTLF("Switch English");
            break;
          }
        case 'b':  //say "Di-di" to switch Chinese /说“滴滴”切换中文
          {
            PTLF("Switch Chinese");
            break;
          }
        case 'c':  //say "play sound" to enable voice reactions / 说“打开音效”激活语音动作
          {
            enableVoiceQ = true;
            PTLF("Turn on the audio response");
            break;
          }
        case 'd':  //say "be quiet" to disable voice reactions / 说“安静点”禁用语音动作
          {
            enableVoiceQ = false;
            PTLF("Turn off the audio response");
            break;
          }
        case 'e':
          {
            PTLF("Start learning");
            break;
          }
        case 'f':
          {
            PTLF("Stop learning");
            break;
          }
        case 'g':
          {
            PTLF("Delete all learning data!");
            break;
          }
      }
    }
  }
}