/*asynFloat64SyncIO.c*/
/***********************************************************************
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory, and the Regents of the University of
* California, as Operator of Los Alamos National Laboratory, and
* Berliner Elektronenspeicherring-Gesellschaft m.b.H. (BESSY).
* asynDriver is distributed subject to a Software License Agreement
* found in file LICENSE that is included with this distribution.
***********************************************************************/
/*
 * This package provide a simple, synchronous interface to asynFloat64
 * Author:  Marty Kraimer
 * Created: 12OCT2004
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cantProceed.h>

#include <asynDriver.h>
#include <asynFloat64.h>
#include <asynDrvUser.h>
#include <drvAsynIPPort.h>
#define epicsExportSharedSymbols
#include <asynFloat64SyncIO.h>

typedef struct ioPvt{
   asynCommon   *pasynCommon;
   void         *pcommonPvt;
   asynFloat64  *pasynFloat64;
   void         *float64Pvt;
   asynDrvUser  *pasynDrvUser;
   void         *drvUserPvt;
}ioPvt;

/*asynFloat64SyncIO methods*/
static asynStatus connect(const char *port, int addr,
                          asynUser **ppasynUser, const char *drvInfo);
static asynStatus disconnect(asynUser *pasynUser);
static asynStatus writeOp(asynUser *pasynUser,epicsFloat64 value,double timeout);
static asynStatus readOp(asynUser *pasynUser,epicsFloat64 *pvalue,double timeout);
static asynStatus writeOpOnce(const char *port, int addr,
                     epicsFloat64 value, double timeout, const char *drvInfo);
static asynStatus readOpOnce(const char *port, int addr,
                     epicsFloat64 *pvalue,double timeout, const char *drvInfo);
static asynFloat64SyncIO interface = {
    connect,
    disconnect,
    writeOp,
    readOp,
    writeOpOnce,
    readOpOnce
};
epicsShareDef asynFloat64SyncIO *pasynFloat64SyncIO = &interface;

static asynStatus connect(const char *port, int addr,
   asynUser **ppasynUser, const char *drvInfo)
{
    ioPvt         *pioPvt;
    asynUser      *pasynUser;
    asynStatus    status;
    asynInterface *pasynInterface;

    pioPvt = (ioPvt *)callocMustSucceed(1, sizeof(ioPvt),"asynFloat64SyncIO");
    pasynUser = pasynManager->createAsynUser(0,0);
    pasynUser->userPvt = pioPvt;
    *ppasynUser = pasynUser;
    status = pasynManager->connectDevice(pasynUser, port, addr);    
    if (status != asynSuccess) {
      printf("Can't connect to port %s address %d %s\n",
          port, addr,pasynUser->errorMessage);
      pasynManager->freeAsynUser(pasynUser);
      free(pioPvt);
      return status ;
    }
    pasynInterface = pasynManager->findInterface(pasynUser, asynCommonType, 1);
    if (!pasynInterface) {
       printf("%s interface not supported\n", asynCommonType);
       goto cleanup;
    }
    pioPvt->pasynCommon = (asynCommon *)pasynInterface->pinterface;
    pioPvt->pcommonPvt = pasynInterface->drvPvt;
    pasynInterface = pasynManager->findInterface(pasynUser, asynFloat64Type, 1);
    if (!pasynInterface) {
       printf("%s interface not supported\n", asynFloat64Type);
       goto cleanup;
    }
    pioPvt->pasynFloat64 = (asynFloat64 *)pasynInterface->pinterface;
    pioPvt->float64Pvt = pasynInterface->drvPvt;
    if(drvInfo) {
        /* Check for asynDrvUser interface */
        pasynInterface = pasynManager->findInterface(pasynUser,asynDrvUserType,1);
        if(pasynInterface) {
            asynDrvUser *pasynDrvUser;
            void       *drvPvt;
            pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
            drvPvt = pasynInterface->drvPvt;
            status = pasynDrvUser->create(drvPvt,pasynUser,drvInfo,0,0);
            if(status==asynSuccess) {
                pioPvt->pasynDrvUser = pasynDrvUser;
                pioPvt->drvUserPvt = drvPvt;
            } else {
                printf("asynFloat64SyncIO::connect drvUserCreate drvInfo=%s %s\n",
                         drvInfo, pasynUser->errorMessage);
            }
        }
    }
    return asynSuccess ;
cleanup:
    disconnect(pasynUser);
    return asynError;
}

static asynStatus disconnect(asynUser *pasynUser)
{
    ioPvt      *pioPvt = (ioPvt *)pasynUser->userPvt;
    asynStatus status;

    if(pioPvt->pasynDrvUser) {
        status = pioPvt->pasynDrvUser->destroy(pioPvt->drvUserPvt,pasynUser);
        if(status!=asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "asynFloat64SyncIO pasynDrvUser->destroy failed %s\n",
                pasynUser->errorMessage);
            return status;
        }
    }
    status = pasynManager->disconnect(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynFloat64SyncIO disconnect failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pasynManager->freeAsynUser(pasynUser);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynFloat64SyncIO freeAsynUser failed %s\n",
            pasynUser->errorMessage);
        return status;
    }
    free(pioPvt);
    return asynSuccess;
}

static asynStatus writeOp(asynUser *pasynUser,epicsFloat64 value,double timeout)
{
    asynStatus status;
    ioPvt      *pPvt = (ioPvt *)pasynUser->userPvt;

    pasynUser->timeout = timeout;
    status = pasynManager->lockPort(pasynUser,1);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynFloat64SyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pPvt->pasynFloat64->write(pPvt->float64Pvt, pasynUser,value);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
              "asynFloat64SyncIO status=%d, wrote: %e",
              status,value);
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus readOp(asynUser *pasynUser,epicsFloat64 *pvalue,double timeout)
{
    ioPvt      *pPvt = (ioPvt *)pasynUser->userPvt;
    asynStatus status;

    pasynUser->timeout = timeout;
    status = pasynManager->lockPort(pasynUser,1);
    if(status!=asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynFloat64SyncIO lockPort failed %s\n",pasynUser->errorMessage);
        return status;
    }
    status = pPvt->pasynFloat64->read(pPvt->float64Pvt, pasynUser, pvalue);
    asynPrint(pasynUser, ASYN_TRACEIO_DEVICE, 
                 "asynFloat64SyncIO status=%d read: %e",status,*pvalue);
    if((pasynManager->unlockPort(pasynUser)) ) {
        asynPrint(pasynUser,ASYN_TRACE_ERROR,
            "unlockPort error %s\n", pasynUser->errorMessage);
    }
    return status;
}

static asynStatus writeOpOnce(const char *port, int addr,
    epicsFloat64 value,double timeout,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
       printf("asynFloat64SyncIO connect failed %s\n",
           pasynUser->errorMessage);
       return status;
    }
    status = writeOp(pasynUser,value,timeout);
    if(status!=asynSuccess) {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynFloat64SyncIO writeOp failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}

static asynStatus readOpOnce(const char *port, int addr,
                   epicsFloat64 *pvalue,double timeout,const char *drvInfo)
{
    asynStatus status;
    asynUser   *pasynUser;

    status = connect(port,addr,&pasynUser,drvInfo);
    if(status!=asynSuccess) {
       printf("asynFloat64SyncIO connect failed %s\n",
           pasynUser->errorMessage);
       return status;
    }
    status = readOp(pasynUser,pvalue,timeout);
    if(status!=asynSuccess) {
       asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "asynFloat64SyncIO readOp failed %s\n",pasynUser->errorMessage);
    }
    disconnect(pasynUser);
    return status;
}
