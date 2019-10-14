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


//Lista johon tallenetaan MPU9250 sensorin dataa
float MPU_Data[50][6];
int indexNumber = 0;

Display_Handle dispHandle;

static PIN_Handle MPU_PIN_handle;
static PIN_State MPU_PIN_state;
//Liikesensorin pinnin määrittely outputiksi
static PIN_Config MPUConfig[] = {
    Board_MPU_POWER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};


void MPUTask(UArg arg0, UArg arg1) {
    
    float ax, ay, az, bx, by, bz;
    
    uint8_t txBuffer[1];
    uint16_t rxBuffer[14];
    
    I2C_Handle i2cMPU_handle;
    I2C_Params i2cMPU_params;
    I2C_Transaction i2c_Transaction;
    
    I2C_Params_init(&i2cMPU_params);
    i2cMPU_params.bitRate = I2C_400kHz;
    i2cMPU_params.custom = (uintptr_t)&i2cMPUCfg;

    Display_Params dispParams;
    dispParams.lineClearMode = DISPLAY_CLEAR_BOTH;
    Display_Params_init(&dispParams);

    
    dispHandle = Display_open(Display_Type_LCD, &dispParams);
    if (dispHandle == NULL) {
        System_abort("Näyttöä ei saatu avattua");
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
        i2cMPU_handle = I2C_open(Board_I2C, &i2cMPU_params);
        if (i2cMPU_handle == NULL) {
            System_abort("Virhe käynnistäessä MPU9250 sensoria");
        }
        
        mpu9250_get_data(&i2cMPU_handle, &ax, &ay, &az, &bx, &by, &bz);
        
        Display_clear(dispHandle);
        Display_print1(dispHandle, 5, 4, "%i", ax);
        
        addValuesToArray(ax, ay, az, bx, by, bz);
        
        I2C_close(i2cMPU_handle);
        Task_sleep(1000000 / Clock_tickPeriod);
        
    } 
}

void ButtonTask(UArg arg0, UArg arg1) {
    
}



int main(void) {
    Task_Handle task;
    Task_Params task_Params;
    
    Board_initGeneral();
    Board_initI2C();
    
    MPU_PIN_handle = PIN_open(&MPU_PIN_state, MPUConfig);
    
    if (MPU_PIN_handle == NULL) {
        System_abort("Pinnin käyttöönotto keskeytyi \n");
    }
    
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
