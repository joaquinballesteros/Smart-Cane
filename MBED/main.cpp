#include "mbed.h"
#include "ble/BLE.h"
#include "ble/services/UARTService.h"
#include <string>
#include <vector>

//------------------------------------------------------------
// Definition
//------------------------------------------------------------
#define TAM                    400 //multiplos de 20
#define TICKER_INSERT          0.02 //50 Hz
#define TICKER_SEND            8*TICKER_INSERT //nunca mayor que 10
#define MIN_CONN_INTERVAL      TICKER_SEND*1000/2 //250 milliseconds
#define MAX_CONN_INTERVAL      TICKER_SEND*1000 //350 milliseconds

//------------------------------------------------------------
// Object generation
//------------------------------------------------------------
BLEDevice       ble;
Ticker          ticker_insert;
Ticker          ticker_send;
Timer           tPasos;
Timer           tActividad;
UARTService     *uartServicePtr;
AnalogIn        forceSensor(P0_4);

//------------------------------------------------------------
// Activity variables
//------------------------------------------------------------
static const uint16_t BEGIN_LRM_AFTER = 4; //Tiempo para pasar a inactividad
static const uint16_t IDLE_FORCE_SENSOR_THRESHOLD = 15; //Threshold lectura minima sensor
static const float MINIMUN_STEP = 0.3; //Tiempo minimo para que un paso sea valido
uint32_t readForceIdleCounter = 0; // init the idle counter to 0
static const int offset = 0; //error en ms entre movil y video

struct Estados { //Maquina de estados
    //I_State->1, S*_State->2, S_State->3, NS*_State->4, NS_State->5
    uint16_t Anterior;
    uint16_t Actual;
    bool PSoporte;
    bool Soporte;
} State;

struct Activity {
    uint32_t actLength;
    uint16_t data_act[7];
    bool fail;
};

static uint16_t fsrReading = 0;
uint32_t actLen;
uint32_t act_cont = 0;
bool requestAct, sync_act, chgCont;
uint8_t rx_buf[20];

vector<uint16_t> S_Matrix;
vector<uint16_t> NS_Matrix;
vector<Activity> act_buffer;

//------------------------------------------------------------
// Raw variables
//------------------------------------------------------------
uint8_t circularBuffer[TAM];
uint16_t i = 0;
uint16_t e = 0;
uint16_t j = 0;
bool sync_raw;

/**
    Raw Data functions: Modular Addition, Modular Subtraction
**/

//======================================================================
// Modular Addition
//======================================================================
uint16_t modularAdd(uint16_t x, uint16_t y, uint16_t tam)
{
    return ((x+y) % tam);
}

//======================================================================
// Modular Subtraction
//======================================================================
uint16_t modularSub(uint16_t x, uint16_t y, uint16_t tam)
{
    return (((x>y?x:x+tam)-y) % tam);
}

/**
    Activity functions: Measurement, StateMachine
**/

//======================================================================
// Measurement
//======================================================================
int MeasurementFunction(const vector<uint16_t> &X, const vector<uint16_t> &Y)
{
    int temp, len;
    float meanS, meanNS, sim, varS, varNS;
    Activity tempAct;

    if ((X.size() != 0) && (Y.size() != 0)) {
        temp = X.size() + Y.size();

        meanS = 0;
        meanNS = 0;
        varS = 0;
        varNS = 0;
        if (X.size() >= Y.size()) {
            len = Y.size();
        } else {
            len = X.size();
        }
        for (int k = 1; k < (len-1); k++) {
            meanS += X[k]; //acumula el tiempo de los pasos de soporte en una variable
            meanNS += Y[k]; //acumula el tiempo de los pasos de no soporte en una variable
        }
        if(len > 2) {

            meanS /= (len-2)*1.0; //media aritmetica (ms)
            meanNS /= (len-2)*1.0;

            for(int n = 1; n < (len-1); n++ ) {
                varS += ((X[n]) - (meanS)) * ((X[n]) - (meanS));
                varNS += ((Y[n]) - (meanNS)) * ((Y[n]) - (meanNS));
            }

            varS /= ((len-2)*1.0); //varianza (ms)
            varNS /=  ((len-2)*1.0);

            sim = meanNS / meanS;

            tempAct.data_act[0] = temp; //pasos
            if (actLen != 0) {
                tempAct.actLength = (actLen/1000.0)*1000; //duracion de la actividad (segundos)*1000
                tempAct.data_act[1] = (temp / (actLen*1.0)) * 60 * 1000 * 100; //cadencia *100
                tempAct.data_act[2] = meanS; //media temporal de los pasos de soporte
                tempAct.data_act[3] = meanNS; //media temporal de los pasos de no soporte
                tempAct.data_act[4] = sim * 1000; //simetria de los pasos ()*1000
                tempAct.data_act[5] = varS * 100; //varianza
                tempAct.data_act[6] = varNS * 100; //varianza
            } else {
                tempAct.fail = true;
                act_buffer.push_back(tempAct);
                return 3;
            }
        } else {
            tempAct.fail = true;
            act_buffer.push_back(tempAct);
            return 2;
        }
    } else {
        tempAct.fail = true;
        act_buffer.push_back(tempAct);
        return 1;
    }
    tempAct.fail = false;
    act_buffer.push_back(tempAct);
    return 0;
}

//======================================================================
// State Machine Process
//======================================================================
void stateMachine()
{
    //Reading data
    if (fsrReading > IDLE_FORCE_SENSOR_THRESHOLD) {
        State.Soporte = true;
    } else {
        State.Soporte = false;
    }

    //Estimating the actual state
    switch (State.Anterior) {
        case 1:
            if (State.PSoporte != State.Soporte) {
                tPasos.reset();
                if (State.Soporte) {
                    State.Actual = 2;
                } else {
                    State.Actual = 4;
                }
            } else {
                State.Actual = 1;
            }
            break;
        case 2:
            if ((readForceIdleCounter * TICKER_INSERT >= BEGIN_LRM_AFTER) || ((readForceIdleCounter * TICKER_INSERT < MINIMUN_STEP) && !(State.Soporte))) {
                State.Actual = 1;
            } else if ((readForceIdleCounter * TICKER_INSERT > MINIMUN_STEP) && !(State.Soporte)) {
                State.Actual = 3;
            } else {
                State.Actual = 2;
            }
            break;
        case 3:
            State.Actual = 4;
            break;
        case 4:
            if ((readForceIdleCounter * TICKER_INSERT >= BEGIN_LRM_AFTER) || ((readForceIdleCounter * TICKER_INSERT < MINIMUN_STEP) && (State.Soporte))) {
                State.Actual = 1;
            } else if ((readForceIdleCounter * TICKER_INSERT > MINIMUN_STEP) && (State.Soporte)) {
                State.Actual = 5;
            } else {
                State.Actual = 4;
            }
            break;
        case 5:
            State.Actual = 2;
            break;
    }

    //State's actions
    switch (State.Actual) {
        case 1:
            readForceIdleCounter = 0;
            if (((State.Anterior == 2) || (State.Anterior == 4)) && ((S_Matrix.size() != 0) && (NS_Matrix.size() != 0))) {
                actLen = tActividad.read_ms() - (BEGIN_LRM_AFTER * 1000); //compensar los 200 datos para volver a estado 1 estando en 2-4
                MeasurementFunction(S_Matrix, NS_Matrix);
                act_cont++;
                chgCont = true;
                S_Matrix.clear();
                NS_Matrix.clear();
                tActividad.reset();
            }
            break;
        case 2:
            readForceIdleCounter++;
            break;
        case 3:
            S_Matrix.push_back(tPasos.read_ms() + offset);
            tPasos.reset();
            readForceIdleCounter = 0;
            break;
        case 4:
            readForceIdleCounter++;
            break;
        case 5:
            NS_Matrix.push_back(tPasos.read_ms() - offset);
            tPasos.reset();
            readForceIdleCounter = 0;
            break;
    }

    State.Anterior = State.Actual;
    State.PSoporte = State.Soporte;

}

/**
    Common functions: insertCallback, disconnectionCallback, sendCallback, syncTime
**/

//======================================================================
// Callback Send
//======================================================================
void sendCallback()
{
    if(sync_raw){
        if (ble.getGapState().connected && modularSub(i,e,TAM) >= 20) {
            ble.updateCharacteristicValue(uartServicePtr->getRXCharacteristicHandle(), &circularBuffer[e], 20);
            e = modularAdd(e,20,TAM);
        }
    }
    
    if (ble.getGapState().connected) {
        if(act_cont != 0 && chgCont) {
            rx_buf[0] = (act_cont >> 24);
            rx_buf[1] = (act_cont >> 16);
            rx_buf[2] = (act_cont >> 8);
            rx_buf[3] = act_cont;
            ble.updateCharacteristicValue(uartServicePtr->getRXCharacteristicHandle(), rx_buf, 4);
            chgCont = false;
        } else if(requestAct && !act_buffer.empty() && !act_buffer.front().fail) {
            rx_buf[0] = (act_buffer.front().actLength >> 24);
            rx_buf[1] = (act_buffer.front().actLength >> 16);
            rx_buf[2] = (act_buffer.front().actLength >> 8);
            rx_buf[3] = act_buffer.front().actLength;
            rx_buf[4] = (act_buffer.front().data_act[0] >> 8);
            rx_buf[5] = act_buffer.front().data_act[0];
            rx_buf[6] = (act_buffer.front().data_act[1] >> 8);
            rx_buf[7] = act_buffer.front().data_act[1];
            rx_buf[8] = (act_buffer.front().data_act[2] >> 8);
            rx_buf[9] = act_buffer.front().data_act[2];
            rx_buf[10] = (act_buffer.front().data_act[3] >> 8);
            rx_buf[11] = act_buffer.front().data_act[3];
            rx_buf[12] = (act_buffer.front().data_act[4] >> 8);
            rx_buf[13] = act_buffer.front().data_act[4];
            rx_buf[14] = (act_buffer.front().data_act[5] >> 8);
            rx_buf[15] = act_buffer.front().data_act[5];
            rx_buf[16] = (act_buffer.front().data_act[6] >> 8);
            rx_buf[17] = act_buffer.front().data_act[6];
            ble.updateCharacteristicValue(uartServicePtr->getRXCharacteristicHandle(), rx_buf, 18);
            act_buffer.erase(act_buffer.begin());
            if(act_buffer.empty()) requestAct = false;
        }
    }
}

//======================================================================
// Read sensor callback
//======================================================================
void insertCallback()
{
    if(sync_act) {
        fsrReading = forceSensor.read_u16();
        stateMachine();
    }
    
    if(sync_raw){
        fsrReading = forceSensor.read_u16();
        if (i != modularSub(e,2,TAM)) {
            circularBuffer[i] = (fsrReading >> 8); //MSB
            circularBuffer[i+1] = fsrReading; //LSB
            i = modularAdd(i,2,TAM);
            j=0;
        } else {
            circularBuffer[i] = ((65536-j) >> 8); //MSB
            circularBuffer[i+1] = (65536-j); //LSB
            j++;
            i = i;
        }
    }
}

//======================================================================
// onDisconnection
//======================================================================
void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    if(sync_act){
        requestAct = true;
        act_cont = 0;
        sync_act = false;
        tPasos.stop();
        tActividad.stop();
    }
    ble.startAdvertising();
}

//======================================================================
// Data Written Callback
//======================================================================
void syncTime(const GattWriteCallbackParams *params)
{
    if ((uartServicePtr != NULL) && (params->handle == uartServicePtr->getTXCharacteristicHandle())) {
        if(params->data[0] == 0x30){
            sync_raw = true;
            insertCallback();
        } else if(params->data[0] == 0x31) {
            tPasos.start();
            tActividad.start();
            sync_act = true;
            requestAct = false;
        } else if(params->data[0] == 0x32) {
            requestAct = true;
            act_cont = 0;
            sync_act = false;
            tPasos.stop();
            tActividad.stop();
        }
        ticker_insert.attach(insertCallback, TICKER_INSERT);
        ticker_send.attach(sendCallback, TICKER_SEND);
    }
}

//======================================================================
// Main
//======================================================================
int main(void)
{

    fsrReading = forceSensor.read_u16();

    if (fsrReading > IDLE_FORCE_SENSOR_THRESHOLD) {
        State.PSoporte = true;
    } else {
        State.PSoporte = false;
    }

    State.Soporte = false;
    State.Anterior = 1;

    ble.init();
    ble.onDisconnection(disconnectionCallback);
    ble.onDataWritten(syncTime);

    /* setup advertising */
    ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED);
    ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                                     (const uint8_t *)"BLE Cane", sizeof("BLE Cane") - 1);
    ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS,
                                     (const uint8_t *)UARTServiceUUID_reversed, sizeof(UARTServiceUUID_reversed));

    ble.setAdvertisingInterval(1000); /* 1000ms; in multiples of 0.625ms. */
    ble.startAdvertising();

    UARTService uartService(ble);
    uartServicePtr = &uartService;
    sync_raw = false;
    sync_act = false;

    do {
        ble.waitForEvent();
    } while(!ble.getGapState().connected);

    while (true) {
        ble.waitForEvent();
    }
}