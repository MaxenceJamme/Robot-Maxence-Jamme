#include <xc.h>
#include "UART_Protocol.h"
#include "CB_TX1.h"
#include "main.h"
#include "IO.h"
#include "PWM.h"
#include "Toolbox.h"
#include "asservissement.h"
#include "Robot.h"

unsigned char UartCalculateChecksum(int msgFunction, int msgPayloadLength, unsigned char * msgPayload) {
    //Fonction prenant entr�e la trame et sa longueur pour calculer le checksum
    unsigned char Checksum = 0;
    Checksum ^= (0xFE);
    Checksum ^= (msgFunction >> 8);
    Checksum ^= (msgFunction >> 0);
    Checksum ^= (msgPayloadLength >> 8);
    Checksum ^= (msgPayloadLength >> 0);
    int i = 0;
    for (i = 0; i < msgPayloadLength; i++) {
        Checksum ^= msgPayload[i];
    }
    return Checksum;
}

void UartEncodeAndSendMessage(int msgFunction, int msgPayloadLength, unsigned char* msgPayload) {
    //Fonction d?encodage et d?envoi d?un message
    unsigned char Checksum = 0;
    unsigned char trame[msgPayloadLength + 6];
    int pos = 0;
    trame[pos++] = (0xFE);
    trame[pos++] = (msgFunction >> 8);
    trame[pos++] = (msgFunction >> 0);
    trame[pos++] = (msgPayloadLength >> 8);
    trame[pos++] = (msgPayloadLength >> 0);
    int i = 0;
    for (i = 0; i < msgPayloadLength; i++) {
        trame[pos++] = msgPayload[i];
    }
    Checksum = UartCalculateChecksum(msgFunction, msgPayloadLength, msgPayload);

    trame[pos++] = (Checksum);
    SendMessage(trame, msgPayloadLength + 6);
}

int msgDecodedFunction = 0;
int msgDecodedPayloadLength = 0;
unsigned char msgDecodedPayload[128];
int msgDecodedPayloadIndex = 0;
int rcvState = 0;
unsigned char calculatedChecksum = 0;

void UartDecodeMessage(unsigned char c) {
    //Fonction prenant en entr�e un octet et servant � reconstituer les trames
    switch (rcvState) {
        case StateReceptionWaiting:
            if (c == 0xFE) {
                rcvState = StateReceptionFunctionMSB;
                msgDecodedPayloadLength = 0;
                msgDecodedFunction = 0;
                msgDecodedPayloadIndex = 0;
            }
            break;
        case StateReceptionFunctionMSB:
            msgDecodedFunction = ((int)c << 8);
            rcvState = StateReceptionFunctionLSB;
            break;
        case StateReceptionFunctionLSB:
            msgDecodedFunction += (int)c;
            rcvState = StateReceptionPayloadLengthMSB;
            break;
        case StateReceptionPayloadLengthMSB:
            msgDecodedPayloadLength = ((int)c << 8);
            rcvState = StateReceptionPayloadLengthLSB;
            break;
        case StateReceptionPayloadLengthLSB:
            msgDecodedPayloadLength += (int)c;
            if(msgDecodedPayloadLength == 0)
                rcvState = StateReceptionCheckSum;
            else if (msgDecodedPayloadLength > 128)
                rcvState = StateReceptionWaiting;
            else
            {                
                msgDecodedPayloadIndex = 0;
                rcvState = StateReceptionPayload;
            }
            break;
        case StateReceptionPayload:
            msgDecodedPayload[msgDecodedPayloadIndex++] = c;
            if (msgDecodedPayloadIndex >= msgDecodedPayloadLength) {
                rcvState = StateReceptionCheckSum;
            }
            break;
        case StateReceptionCheckSum:
            calculatedChecksum = UartCalculateChecksum(msgDecodedFunction, msgDecodedPayloadLength, msgDecodedPayload);
            if (calculatedChecksum == c) {
                UartProcessDecodedMessage(msgDecodedFunction, msgDecodedPayloadLength, msgDecodedPayload);
            } else {
                int toto=0;
                toto++;
                //SendMessage( (unsigned char *) "7654321" , 7 ) ;
            }
            rcvState = StateReceptionWaiting;
            break;
        default:
            rcvState = StateReceptionWaiting;
            break;
    }
}

double mode, Kp, Ki, Kd, KpMax, KiMax, KdMax, VL, VA;

void UartProcessDecodedMessage(unsigned char msgFunction, unsigned char msgpayloadLength, unsigned char* msgPayload) {
    //Fonction appel�e apr�s le d�codage pour ex�cuter l?action
    //correspondant au message re�u

    switch (msgFunction) {
        case SET_ROBOT_STATE:
            SetRobotState(msgPayload[0]);
            break;

        case SET_ROBOT_MANUAL_CONTROL:
            SetRobotAutoControlState(msgPayload[0]);
            break;

        case Function_Text:
            UartEncodeAndSendMessage(Function_Text, msgpayloadLength, msgPayload);
            break;

        case Function_Led:
            if (msgPayload[0] == 0x49 && msgPayload[1] == 0x31) { // 0x49=I 0x31=1
                LED_ORANGE = 1;
            }
            if (msgPayload[0] == 0x4F && msgPayload[1] == 0x31) {
                LED_ORANGE = 0;
            }
            if (msgPayload[0] == 0x49 && msgPayload[1] == 0x32) {
                LED_BLEUE = 1;
            }
            if (msgPayload[0] == 0x4F && msgPayload[1] == 0x32) {
                LED_BLEUE = 0;
            }
            if (msgPayload[0] == 0x49 && msgPayload[1] == 0x33) {
                LED_BLANCHE = 1;
            }
            if (msgPayload[0] == 0x4F && msgPayload[1] == 0x33) {
                LED_BLANCHE = 0;
            }
            break;

        case Function_Asservissement:
            //UartEncodeAndSendMessage(Function_Text, msgpayloadLength, msgPayload);
            mode = getDouble(msgPayload, 0);
            Kp = getDouble(msgPayload, 4);
            Ki = getDouble(msgPayload, 8);
            Kd = getDouble(msgPayload, 12);
            KpMax = getDouble(msgPayload, 16);
            KiMax = getDouble(msgPayload, 20);
            KdMax = getDouble(msgPayload, 24);
            //int kp = getBytesFromDouble(msgPayload, int index, double d);

            if (mode == 1) {
                SetupPidAsservissement(&robotState.PidX, Kp, Ki, Kd, KpMax, KiMax, KdMax);
            } else if (mode == 0) {
                SetupPidAsservissement(&robotState.PidTheta, Kp, Ki, Kd, KpMax, KiMax, KdMax);
            }
            //LED_BLANCHE = 1;
            break;

        case Function_Consigne:
            //UartEncodeAndSendMessage(Function_Text, msgpayloadLength, msgPayload);
            robotState.PidX.consigne = getDouble(msgPayload, 0);
            robotState.PidTheta.consigne = getDouble(msgPayload, 4);
            break;
        default:
            break;
    }
}

void SetRobotState(unsigned char c) {
    unsigned char msgPayloadLed [] = {0, 0};
    if (c == 0x30) {
        LED_BLEUE = 1;
        msgPayloadLed[0] = 0x49;
        msgPayloadLed[1] = 0x32;
        UartEncodeAndSendMessage(Function_Led, 2, msgPayloadLed);
        autoControlActivated = 0;
    } else {
        LED_BLEUE = 0;
        msgPayloadLed[0] = 0x4F;
        msgPayloadLed[1] = 0x32;
        UartEncodeAndSendMessage(Function_Led, 2, msgPayloadLed);
        autoControlActivated = 1;
        stateRobot = STATE_AVANCE;
    }
}

void SetRobotAutoControlState(unsigned char c) {
    switch (c) {
        case 8:
            PWMSetSpeedConsigne(-15, MOTEUR_DROIT);
            PWMSetSpeedConsigne(15, MOTEUR_GAUCHE);
            break;
        case 10:
            PWMSetSpeedConsigne(15, MOTEUR_DROIT);
            PWMSetSpeedConsigne(-15, MOTEUR_GAUCHE);
            break;
        case 2:
            PWMSetSpeedConsigne(-20, MOTEUR_DROIT);
            PWMSetSpeedConsigne(-20, MOTEUR_GAUCHE);
            break;
        case 12:
            PWMSetSpeedConsigne(0, MOTEUR_DROIT);
            PWMSetSpeedConsigne(0, MOTEUR_GAUCHE);
            break;
        case 14:
            PWMSetSpeedConsigne(20, MOTEUR_DROIT);
            PWMSetSpeedConsigne(20, MOTEUR_GAUCHE);
            break;
    }
}
//*************************************************************************/
//Fonctions correspondant aux messages
//*************************************************************************/

