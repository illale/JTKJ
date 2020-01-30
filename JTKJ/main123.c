//Aleksi Illikainen
//Joni Skofelt
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/mw/display/Display.h>
#include <ti/mw/display/DisplayExt.h>
#include <ti/mw/remotecontrol/buzzer.h>

#include "Board.h"
#include "high.h"

#include "wireless/comm_lib.h"
#include "sensors/mpu9250.h"

#define STACKSIZE 2048
/* Tähän taskien tarvitsemat pinomuistit
   muodossa Char mytaskStack[STACKSIZE]; */
char taskStack[STACKSIZE];
char dispStack[STACKSIZE];
char tempStack[STACKSIZE];
char commtaskStack[STACKSIZE];

//Tilakoneen eri tilat
enum state { IDLE_STATE=1, READ_SENSOR, READ_MESSAGE, SEND_MESSAGE };
//tilakoneen nykyinen tila
enum state myState;

//Tila minkä käyttäjä on valinnut kursorilla eli cursorPos
enum MainMenuNavigation {messageMenuNav=1, gestureMenuNav, musicMenuNav, powerMenuNav};
enum MusicMenuNavigation {playMusic1=1, playMusic2, stopPlayingMusic, back};
enum MessageMenuNavigation {sendMessage1=1, sendMessage2, backMessage};
enum GestureMenuNavigation {turnOff=1, turnOn, backGesture};
enum PowerMenuNavigation {yes=1, no};

//Nykyinen valikko missä ollaan
enum CurrentMenu {mainMenu=1, messageMenu, gestureMenu, musicMenu, powerMenu};
enum CurrentMenu currentMenu;

enum messages { message1 = 1, message2 };

int notes[58] = {440, 440, 587, 587, 587, 523, 587, 587, 659, 659, 698, 659, 587, 523, 587, 587, 4, 4, 4, 4, 4, 4, 
4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 587, 587, 523, 587, 659, 659, 587, 659, 698, 698, 659, 659, 587, 587, 4, 4, 4,
 523, 523, 523};

float MPU_Data[50][6];
float variances[6];
float averages[6];
int cursorPos = 1;
char message[16];
int whatMessage = 0;
int temperature;
int numberOfPositives = 0;
int numberOfNegatives = 0;
float ax, ay, az, bx, by, bz;

char payload[16] = "default";

bool clearDisplay;
bool gestureDetection;
bool togglePlayMusic1;
bool startMusic;
bool stopMusic;
bool toggleDraw;

//const uint32_t battery = HWREG(AON_BATMON_BASE + AON_BATMON_O_BAT);

Display_Handle dispHandle;

static PIN_Handle MPU_PIN_handle;
static PIN_State MPU_PIN_state;

static PIN_Handle ButtonHandle;
static PIN_State ButtonState;

static PIN_Handle MenuHandle;
static PIN_State MenuState;

static PIN_Handle BuzzerHandle;
static PIN_State BuzzerState;

static PIN_Config ButtonPower[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PINCC26XX_WAKEUP_NEGEDGE,
    PIN_TERMINATE
};

static PIN_Config Buzzer[] = {
    Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static PIN_Config ButtonSend[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

//Liikesensorin pinnin määrittely outputiksi
static PIN_Config MPUConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static PIN_Config ButtonMenu[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

//Prototyyppejä

char* getMainNames(int index);
char* getMusicNames(int index);
char* getMessageNames(int index);
char* getGestureNames(int index);
char* getPowerNames(int index);
void addValuesToArray(float ax, float ay, float az, float bx, float by, float bz, int index);
void calcVariance();
void calcaverages();
void checkHigh();
void commTaskFxn(UArg arg0, UArg arg1);
void sendCustomMessage(enum messages mess);
void handleButton(PIN_Handle handle, PIN_Id id);
void shutdown();
void MPUTask(UArg arg0, UArg arg1);
void handleButton(PIN_Handle handle, PIN_Id id);
void draw();
void DisplayTask(UArg arg0, UArg arg1);
void buttonFxn(PIN_Handle handle, PIN_Id pinId);
int getMenuLength(enum CurrentMenu menu);
int main(void);

void addValuesToArray(float ax, float ay, float az, float bx, float by, float bz, int index){
    MPU_Data[index][0] = ax;
    MPU_Data[index][1] = ay;
    MPU_Data[index][2] = az;
    MPU_Data[index][3] = bx;
    MPU_Data[index][4] = by;
    MPU_Data[index][5] = bz;
    //System_printf("[%d ,%f, %f, %f, %f, %f, %f], \n", index, ax, ay, az, bx, by, bz);
    //System_flush();
}

void calcVariance() {
    /*
    Tämä funktio laskee kiihyvyyden varianssin kaikilla kolmella akselilla. Sen
    jälkeen laskettu varianssi sijoitetaan globaaliin listaan.
    */
    float variance;
    int i, j;
    
    //tarvitaan varianssin laskemiseen
    calcaverages();
    
    for (j = 0; j < 6; j++) {
        variance = 0;
        for (i = 0; i < 50; i++) {
            variance += (MPU_Data[i][j] - averages[j]) * (MPU_Data[i][j] - averages[j]);
        }
        //Lasketaan varianssi, ja sijoitetaan se num1 muistipaikkaan
        variances[j] = variance / 50;
    }
}


void calcaverages() {
    int j, i;
    float avg;
    numberOfPositives = 0;
    numberOfNegatives = 0;
    for (j = 0; j < 6; j++) {
        avg = 0;
        for (i = 0; i < 50; i++) {
            avg += MPU_Data[i][j];
            if(j == 0 && MPU_Data[i][j] > 5 && i < 25) {
                numberOfPositives++;
            } else if (j == 0 && MPU_Data[i][j] < -5 && i < 25) {
                numberOfNegatives++;
            }
        }
        averages[j] = avg/50;
    }
}


void checkHigh() {
    /*Tämä funktio tarkistaa mikä liike on tehty, vai onko tehty ollenkaan.
    */
    
    //System_printf("A1: %f, A2: %f, A3: %f, V1: %f,  V2: %f,  V3: %f \n", averages[0], averages[1], averages[2], variances[0], variances[1], variances[2]);
    //System_flush();
   
    if (averages[3] > 0 && averages[3] < 35) {
        if ((averages[1] > 0.5) && (averages[0] < - 0.05)) {
            Display_clear(dispHandle);
            Display_print0(dispHandle, 5, 4, "Ylavitonen");
            System_printf("Ylavitonen\n");
            System_flush();
            strcpy(payload, "Ylavitonen \n");
            myState = SEND_MESSAGE;
        }
    } else if (averages[3] < -20){
        if (variances[1] > 0.25) {
            Display_clear(dispHandle);
            Display_print0(dispHandle, 5, 4, "Alavitonen");
            System_printf("Alavitonen\n");
            System_flush();
            strcpy(payload, "Alavitonen \n");
            myState = SEND_MESSAGE;
        }
    } else if(variances[3] > 1000 && variances[4] > 1000 && variances[5] < 500) {
        Display_clear(dispHandle);
            Display_print0(dispHandle, 5, 4, "Ympyrä");
            System_printf("ympyrä\n");
            System_flush();
    } 
}

/*float getBatteryLevel() {
    
    float batteryLevel;
    int batteryLevelMask = 0b00000000000000000000011111111111;
    batteryLevel = (battery & batteryLevelMask);
    return batteryLevel / 256;
}*/

//Hoitaa viestien vastaanottamisen ja lähettämisen
Void commTaskFxn(UArg arg0, UArg arg1) {
    uint8_t senderAddr;
   
    // Radio to receive mode
    int32_t result = StartReceive6LoWPAN();
    if(result != true) {
        System_abort("Wireless receive mode failed");
    }
    
 
    while (1) {
        //Tarkastetaan onko viesti odottamassa käsittelyä
        if (GetRXFlag()) {
            memset(payload,0,8);
            Receive6LoWPAN(&senderAddr, payload, 8);
            strcpy(message, payload);
        }
        //tilan mukaan lähetetään viesti
        if (myState == SEND_MESSAGE) {
            Send6LoWPAN(IEEE80154_SERVER_ADDR, payload, strlen(payload));
            StartReceive6LoWPAN();
            myState = IDLE_STATE;
        }
    }
}
 

//Tällä voidaan lähettää kaksi ennaltamääritettyä viestiä radiolla
void sendCustomMessage(enum messages mess) {
    myState = SEND_MESSAGE;
    switch (mess) {
        case message1:
            strcpy(payload, "yolo");
            break;
        case message2:
            strcpy(payload, "swag");
            break;
    }
}

//Sulkee sensortagin ja asettaa laitteen toisen napin päällekytkimeksi
void shutdown() {
    Display_clear(dispHandle);
    Display_close(dispHandle);
    Task_sleep(100000/Clock_tickPeriod);
    PIN_close(ButtonHandle);
    PINCC26XX_setWakeup(ButtonPower);
    Power_shutdown(NULL,0);
}

//Hoitaa sensoridatojen hakemisen I2C väylän kautta
void MPUTask(UArg arg0, UArg arg1) {
    
    int IndexNumber = 0;
    
    I2C_Handle i2cMPU_handle;
    I2C_Params i2cMPU_params;
    
    I2C_Handle i2cTMP_handle;
    I2C_Params i2cTMP_params;
    
    I2C_Params_init(&i2cTMP_params);
    i2cTMP_params.bitRate = I2C_400kHz;
    
    I2C_Params_init(&i2cMPU_params);
    i2cMPU_params.bitRate = I2C_400kHz;
    i2cMPU_params.custom = (uintptr_t)&i2cMPUCfg;
    
    i2cMPU_handle = I2C_open(Board_I2C, &i2cMPU_params);
    if (i2cMPU_handle == NULL) {
        System_abort("Virhe käynnistäessä MPU9250 sensoria \n");
    }
    
    PIN_setOutputValue(MPU_PIN_handle, Board_MPU_POWER, Board_MPU_POWER_ON);

    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250 on käynnissä \n");
    System_flush();
    
    System_printf("Kalibroidaan MPU9250 \n");
    System_flush();
    
    mpu9250_setup(&i2cMPU_handle);
    
    System_printf("MPU9250 kalibroitu \n");
    System_flush();

    I2C_close(i2cMPU_handle);
    
    i2cTMP_handle = I2C_open(Board_I2C, &i2cTMP_params);
    if (i2cTMP_handle == NULL) {
        System_abort("Virhe käynnistäessä lämpötila-sensoria \n");
    }
    
    I2C_close(i2cTMP_handle);
    
    
    
    while(1) {
        if (myState == IDLE_STATE && gestureDetection) {
            i2cMPU_handle = I2C_open(Board_I2C, &i2cMPU_params);
            if (i2cMPU_handle == NULL) {
                System_abort("Virhe käynnistäessä MPU9250 sensoria");
            }
            
            //Haetaan kiihtyvyysanturin data
            mpu9250_get_data(&i2cMPU_handle, &ax, &ay, &az, &bx, &by, &bz);
            //Asetetaan kiihtyvyysanturin data omaan taulukkoon
            addValuesToArray(ax, ay, az, bx, by, bz, IndexNumber);
            //pidetään yllä mihin taulukon indeksiin on viimeksi sijoitettu
            if(IndexNumber == 50){
                IndexNumber = -1;
                calcVariance();
                checkHigh();
            }
            IndexNumber++;
            
            I2C_close(i2cMPU_handle);
         
            i2cTMP_handle = I2C_open(Board_I2C_TMP, &i2cTMP_params);
            temperature = tmp007_get_data(&i2cTMP_handle);
            I2C_close(i2cTMP_handle);
            
            
        }
        Task_sleep(50000 / Clock_tickPeriod);
    } 
}

//Cursorin liikuttaja funktio
void handleButton(PIN_Handle handle, PIN_Id id) {
    if (cursorPos == getMenuLength(currentMenu)) {
        cursorPos = 1;
    } else {
        cursorPos++;
    }
}
//Piirtää laitteen näytölle hauskan kuvan
void draw() {
    tContext *Context = DisplayExt_getGrlibContext(dispHandle);
    if (Context) {
        //GrImageDraw(Context, &image, 1, 1);
        GrCircleDraw(Context, 66, 54 , 8);
        GrLineDraw(Context, 61, 50, 63, 50);
        GrLineDraw(Context, 67, 50, 70, 50);
        GrLineDraw(Context, 61, 56, 70, 56);
        GrLineDraw(Context, 78, 56, 80, 48);
        GrFlush(Context);
    } else {
        System_printf("Context failed");
        System_flush();
    }
}
//hoitaa näytön tekstien ja muiden kaikkien hallinnan
void DisplayTask(UArg arg0, UArg arg1) {
    Display_Params dispParams;
    dispParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&dispParams);

    dispHandle = Display_open(Display_Type_LCD, &dispParams);
    if (dispHandle == NULL) {
        System_abort("Näyttöä ei saatu avattua");
    }
    clearDisplay = false;
    
    int indexer = 0;
    
    Display_clear(dispHandle);

    while (1) {
        //voidaan mielivaltaisesti puhdistaa näyttö
        if(clearDisplay) {
            Display_clear(dispHandle);
            clearDisplay = false;
        }
        
        //Musiikin toiston käynnistäminen
        if(startMusic || indexer==4 || indexer == 7){
            buzzerOpen(BuzzerHandle);
            startMusic = false;
        } 
        
        if (stopMusic) {
            buzzerClose();
            stopMusic = false;
            togglePlayMusic1 = false;
        }
        
        if(togglePlayMusic1){
            if(notes[indexer] != notes[indexer+1]){
                buzzerSetFrequency(notes[indexer]);
            }
            if(indexer==58) {
                indexer = 0;
            } else {
                indexer++;
            }
        }
        
        if(toggleDraw) {
            draw();
        }
        
        int i;
        //tästä eteenpäin on valikon piirtoa
        if(currentMenu == mainMenu) {
            for(i = 1; i < 5; i++) {
                if (i == cursorPos) {
                    Display_print1(dispHandle, i, 1, "> %s", getMainNames(i));
                } else {
                    Display_print1(dispHandle, i, 1, "%s", getMainNames(i));
                }
            }
        } 
        else if (currentMenu == messageMenu) {
            for(i = 1; i < 4; i++) {
                if (i == cursorPos) {
                    Display_print1(dispHandle, i, 1, "> %s", getMessageNames(i));
                } else {
                    Display_print1(dispHandle, i, 1, "%s", getMessageNames(i));
                }
            }
        }
        else if(currentMenu == musicMenu) {
            for(i = 1; i < 5; i++) {
                if (i == cursorPos) {
                    Display_print1(dispHandle, i, 1, "> %s", getMusicNames(i));
                } else {
                    Display_print1(dispHandle, i, 1, "%s", getMusicNames(i));
                }
            }
        }
        else if(currentMenu == gestureMenu) {
            for(i = 1; i < 4; i++) {
                if (i == cursorPos) {
                    Display_print1(dispHandle, i, 1, "> %s", getGestureNames(i));
                } else {
                    Display_print1(dispHandle, i, 1, "%s", getGestureNames(i));
                }
            }
        }
        else if (currentMenu == powerMenu) {
            for(i = 1; i < 3; i++) {
                if (i == cursorPos) {
                    Display_print1(dispHandle, i, 1, "> %s", getPowerNames(i));
                } else {
                    Display_print1(dispHandle, i, 1, "%s", getPowerNames(i));
                }
            }
        }
        //Ilmoitetaan käyttäjälle että viesti on lähetetty
        if (myState == SEND_MESSAGE) {
            Display_clear(dispHandle);
            Display_print0(dispHandle, 2, 4, "Lahetetaan");
            myState = IDLE_STATE;
            Task_sleep(10000 / Clock_tickPeriod);
        }
        
        //Ilmoittaa edellisen vastaanotetun viestin
        Display_print1(dispHandle, 9, 1, "msg: %s", message);
        //Näyttää lämpötilan
        Display_print1(dispHandle, 10, 1, "%d C", temperature);
        
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

int getMenuLength(enum CurrentMenu menu){
    switch (menu) {
        case mainMenu: return 4;
        case messageMenu: return 3;
        case musicMenu: return 4;
        case powerMenu: return 2;
        case gestureMenu: return 3;
        default: return 0;
    }
}

//Menun tilojen nimien getterit
char* getMainNames(int index) {
    switch(index) {
        case 1: return "Send messages";
        case 2: return "Toggle gesture detection";
        case 3: return "Music menu";
        case 4: return "Power off";
        default: return "";
    }
}

char* getMusicNames(int index) {
    switch(index) {
        case 1: return "Play music 1";
        case 2: return "Toggle picture";
        case 3: return "Stop playing";
        case 4: return "Back";
        default: return "";
    }
}

char* getGestureNames(int index) {
    switch(index) {
        case 1: return "On";
        case 2: return "Off";
        case 3: return "Back";
        default: return "";
    }
}

char* getMessageNames(int index) {
    switch(index) {
        case 1: return "Send message 1";
        case 2: return "Send message 2";
        case 3: return "Back";
        default: return "";
    }
}

char* getPowerNames(int index) {
    switch(index) {
        case 1: return "Yes";
        case 2: return "No";
        default: return "";
    }
}

//Tämä funktio hoitaa menussa navigoinnin ja valittaessa suoritettavan toiminnon
Void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    //tarkistetaan valitseeko käyttäjä back napin
    if(cursorPos == getMenuLength(currentMenu) && currentMenu != 1) {
        cursorPos = 1;
        currentMenu = 1;
    } else if (currentMenu == mainMenu){
        currentMenu = cursorPos+1;
        cursorPos = 1;
        
    } else if(currentMenu == powerMenu) {
        
        if(cursorPos == yes) {
            shutdown();
        //Tämä käsitellään jo ensimmäisessä ehtolauseessa älä laita tähän takaisinmenologiikkaa
        } else if (cursorPos == no) { 
            
        }
        
    } else if(currentMenu == messageMenu) {
        
        if(cursorPos == sendMessage1) {
            sendCustomMessage(message1);
        } else if (cursorPos == sendMessage2) {
            sendCustomMessage(message2);
        }
        
    } else if (currentMenu == gestureMenu) {
        if(cursorPos == turnOff){
            gestureDetection = false;
        } else if (cursorPos == turnOn) {
            gestureDetection = true;
        }
        currentMenu = 1;
    } else if (currentMenu == musicMenu) {
        if (cursorPos == playMusic1) {
            startMusic = true;
            togglePlayMusic1 = true;
        } else if (cursorPos == playMusic2) {
            toggleDraw = !toggleDraw;
            clearDisplay = true;
        } else if (cursorPos == stopPlayingMusic) {
            stopMusic = true;
        }
    }
    clearDisplay = true;
}

int main(void) {
    
    Task_Handle dispTask;
    Task_Params disptask_Params;
    
    Task_Handle task;
    Task_Params task_Params;
    
    Board_initGeneral();
    Board_initI2C();
    
    MPU_PIN_handle = PIN_open(&MPU_PIN_state, MPUConfig);
    if (MPU_PIN_handle == NULL) {
        System_abort("Pinnin käyttöönotto keskeytyi \n");
    }
    
    ButtonHandle = PIN_open(&ButtonState, ButtonSend);
    
    if (!ButtonHandle) {
        System_abort("Napin käyttöönotto epäonnistui");
    }
    
    if (PIN_registerIntCb(ButtonHandle, &buttonFxn) != 0) {
        System_abort("Virhe napin funktion rekisteröinnissä");
    }
    
    MenuHandle = PIN_open(&MenuState, ButtonMenu);
    
    if (!MenuHandle) {
        System_abort("Napin käyttöönotto epäonnistui");
    }
    
    if (PIN_registerIntCb(MenuHandle, &handleButton) != 0) {
        System_abort("Virhe napin funktion rekisteröinnissä");
    }
    
    BuzzerHandle = PIN_open(&BuzzerState, Buzzer);
    
    if (!BuzzerHandle) {
        System_abort("Äänen käyttöönotto epäonnistui");
    }
    
    Task_Params_init(&disptask_Params);
    disptask_Params.stackSize = STACKSIZE;
    disptask_Params.stack = &dispStack;
    disptask_Params.priority = 2;
    
    dispTask = Task_create((Task_FuncPtr)DisplayTask, &disptask_Params, NULL);
    if (dispTask == NULL) {
        System_abort("Taskin luonti epäonnistui \n");
    }
    
    Task_Params_init(&task_Params);
    task_Params.stackSize = STACKSIZE;
    task_Params.stack = &taskStack;
    task_Params.priority = 2;
    
    task = Task_create((Task_FuncPtr)MPUTask, &task_Params, NULL);
    if (task == NULL) {
        System_abort("Taskin luonti epäonnistui \n");
    }
    
    Task_Handle commTask;
	Task_Params commTaskParams;
	
    Init6LoWPAN();
    
    Task_Params_init(&commTaskParams);
    commTaskParams.stackSize = STACKSIZE;
    commTaskParams.stack = &commtaskStack;
    commTaskParams.priority=1;

    commTask = Task_create(commTaskFxn, &commTaskParams, NULL);
    if (commTask == NULL) {
    	System_abort("Task create failed!");
    }
    
    myState = IDLE_STATE;
    currentMenu = mainMenu;
    togglePlayMusic1 = false;
    gestureDetection = true;
    startMusic = false;
    stopMusic = false;
    toggleDraw = false;
    strcpy(message, " ");
    
    BIOS_start();
    
    return (0);
}