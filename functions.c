//Tilakoneen eri tilat
enum state { IDLE=1, READ_SENSOR, READ_MESSAGE, SEND_MESSAGE };
//tilakoneen nykyinen tila
enum state myState = IDLE;


void addValuesToArray(float ax, float ay, float az, float bx, float by, float bz) {
    if (IndexNumber > 50) {
        IndexNumber = 0;
    }
    MPU_Data[IndexNumber][] = {ax, ay, az, bx, by, bz};
}


void calcVariance() {
    float mean, num;
    int i, j;
    mean = num = 0;

    for (j = 1; j < 3; j++) {
        for (i = 0; i < 50; i++) {
            n += 1;
            mean += MPU_Data[i][j];
        }

        mean = mean / n;

        for (i = 0; i < 50; i++) {

        }
    }

}
