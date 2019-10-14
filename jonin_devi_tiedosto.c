#include <stdio.h>
#include <high.h>

//Tilakoneen eri tilat
enum state { IDLE=1, READ_SENSOR, READ_MESSAGE, SEND_MESSAGE };
//tilakoneen nykyinen tila
enum state myState = IDLE;

//Luetaan sensoridata 20 kertaa sekuntissa
float sensor_data[20][7];

//Virhe mikä sallitaan mallidatan ja mittausdatan välillä
float error = 0.5;

int compareSensordata(*mallidata, *sensordata, float error);

int main() {

    return 0;
}


int compareSensordata(*mallidata, *sensordata, float error) {
    int i, j;
    //Käydään jokainen mittausläpi
    for(i = 0; i < 20; i++) {
        //Käydään jokaisen mittauksen kaikki eri arvot läpi
        for(j = 0, j < 7; j++) {

        }
    }
}
