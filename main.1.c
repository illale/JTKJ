//Tilakoneen eri tilat
enum state { IDLE=1, READ_SENSOR, READ_MESSAGE, SEND_MESSAGE };
//tilakoneen nykyinen tila
enum state myState = IDLE;

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

#include "Board.h"

#include "wireless/comm_lib.h"
#include "sensors/mpu9250.h"

#define STACKSIZE 2048
/* Tähän taskien tarvitsemat pinomuistit
   muodossa Char myTaskStack[STACKSIZE]; */
char TaskStack[STACKSIZE];
char dispStack[STACKSIZE];

float MPU_Data[20][6];
float Variances[3];
int Cursor_Pos = 2;
int IndexNumber = 0;

Display_Handle dispHandle;

static PIN_Handle MPU_PIN_handle;
static PIN_State MPU_PIN_state;

static PIN_Handle ButtonHandle;
static PIN_State ButtonState;

static PIN_Config ButtonT[] = {
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};

//Liikesensorin pinnin määrittely outputiksi
static PIN_Config MPUConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

void addValuesToArray(float ax, float ay, float az, float bx, float by, float bz){
    MPU_Data[IndexNumber][0] = ax;
    MPU_Data[IndexNumber][1] = ay;
    MPU_Data[IndexNumber][2] = az;
    MPU_Data[IndexNumber][3] = bx;
    MPU_Data[IndexNumber][4] = by;
    MPU_Data[IndexNumber][5] = bz;
    
}

void calcVariance() {
    /*
    Tämä funktio laskee kiihyvyyden varianssin kaikilla kolmella akselilla. Sen
    jälkeen laskettu varianssi sijoitetaan globaaliin listaan.
    */
    float num1, num2, mean;
    int i, j, n;
    n = 0;
    mean = num1 = num2 = 0;
    
    for (j = 0; j < 3; j++) {
        for (i = 0; i < 20; i++) {
            n += 1;
            num1 += MPU_Data[i][j];
        } 


        //Lasketaan keskiarvo ja sijoitetaan se num1 muistipaikkaan
        
        mean = num1 / n;
        
        for (i = 0; i < 20; i++) {
            num2 += (MPU_Data[i][j] - mean) * (MPU_Data[i][j] - mean);
        }
        //Lasketaan varianssi, ja sijoitetaan se num1 muistipaikkaan
        num1 = num2 / (n - 1);
        
        Variances[j] = num1;
    }
}

void checkHigh() {
    /*
    Tämä funktio tarkistaa mikä liike on tehty, vai onko tehty ollenkaan.
    */
    if ((Variances[0] > 0.15) && (Variances[1] > 0.2) && (Variances[2] > 0.4)) {
        Display_clear(dispHandle);
        Display_print0(dispHandle, 5, 4, "Ylavitonen");
    }
} 

void MPUTask(UArg arg0, UArg arg1) {
    
    float ax, ay, az, bx, by, bz;
    
    I2C_Handle i2cMPU_handle;
    I2C_Params i2cMPU_params;
    
    I2C_Params_init(&i2cMPU_params);
    i2cMPU_params.bitRate = I2C_400kHz;
    i2cMPU_params.custom = (uintptr_t)&i2cMPUCfg;
    
    MPU_PIN_handle = PIN_open(&MPU_PIN_state, MPUConfig);
    if (MPU_PIN_handle == NULL) {
        System_abort("Pinnin käyttöönotto keskeytyi \n");
    }
    
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
    
    while(1) {
        if (myState == IDLE) {
            i2cMPU_handle = I2C_open(Board_I2C, &i2cMPU_params);
            if (i2cMPU_handle == NULL) {
                System_abort("Virhe käynnistäessä MPU9250 sensoria");
            }
            
            mpu9250_get_data(&i2cMPU_handle, &ax, &ay, &az, &bx, &by, &bz);
       
            addValuesToArray(ax, ay, az, bx, by, bz);
    
            IndexNumber += 1;
            if (IndexNumber == 20) {
                myState = HANDLE_DATA;
            }
            
            I2C_close(i2cMPU_handle);
            Task_sleep(30000 / Clock_tickPeriod);
        } else {
            Task_sleep(100000 / Clock_tickPeriod);
        }
    } 
}

void handleButton(PIN_Handle handle, PIN_Id id) {
    Cursor_Pos += 1;
    if (Cursor_Pos > 3) {
        Cursor_Pos = 1;
    }
}

void DisplayTask(UArg arg0, UArg arg1) {
    Display_Params dispParams;
    dispParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&dispParams);
    
    dispHandle = Display_open(Display_Type_LCD, &dispParams);
    if (dispHandle == NULL) {
        System_abort("Näyttöä ei saatu avattua");
    }
    while (1) {
        Display_print0(dispHandle, 2, 3, "MENU-1");
        Display_print0(dispHandle, 3, 3, "MENU-2");
        Display_print0(dispHandle, 4, 3, "MENU-3");
        Display_print0(dispHandle, Cursor_Pos, 2, ">");
        Display_clear(dispHandle);
    }
    
}

void CommTask(UArg arg0, UArg arg1) {
    
}

int main(void) {
    Task_Handle dispTask;
    Task_Params disptask_Params;
    
    Task_Handle task;
    Task_Params task_Params;
    
    Board_initGeneral();
    Board_initI2C();
    
    ButtonHandle = PIN_open(&ButtonState, ButtonT);
    
    if (!ButtonHandle) {
        System_abort("Napin käyttöönotto epäonnistui");
    }
    
    if (PIN_registerIntCb(ButtonHandle, &handleButton) != 0) {
        System_abort("Virhe napin funktion rekisteröinnissä");
    }
    
    //Display Taskin alustus
    Task_Params.init(&disptask_Params);
    disptask_Params.stackSize = STACKSIZE;
    disptask_Params.stack = &dispStack;
    display_task = Task_create((Task_FuncPtr)DisplayTask, &disptask_Params, NULL);
    if (display_task == NULL) {
        System_abort("Taskin luonti epäonnistui \n");
    }
    
    //MPU_Taskin alustus
    Task_Params_init(&task_Params);
    task_Params.stackSize = STACKSIZE;
    task_Params.stack = &TaskStack;
    task = Task_create((Task_FuncPtr)MPUTask, &task_Params, NULL);
    if (task == NULL) {
        System_abort("Taskin luonti epäonnistui \n");
    }
    
    BIOS_start();
    
    return (0);
} 
