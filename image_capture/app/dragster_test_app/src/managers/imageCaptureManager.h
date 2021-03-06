#ifndef SRC_MANAGERS_IMAGECAPTUREMANAGER_H_
#define SRC_MANAGERS_IMAGECAPTUREMANAGER_H_

#include "imageCaptureState.h"
#include "dragsterConfig.h"
#include "xaxivdma.h"
#include "xspi.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_assert.h"

struct ImageCaptureManager
{
public:
    void initialize();
    void startImageCapture();
    void stopImageCapture();
    struct DragsterConfig getDragsterConfig(unsigned char linescannerIndex);
private:
    // vdma
    void initializeVdmaDevices();
    void setupVdmaDevice(XAxiVdma* vdma, u16 deviceId, u32 memoryBaseAddress);
    void configureVdmaInterrupts();
    void startVdmaTransfer();
    void stopVdmaTransfer();
    // dragster
    void initializeSpi();
    void initializeDragsters();
    void initializeDragsterImpl(struct DragsterConfig* config, int dragsterSlaveSelectMask);
    void readDragsterConfigImpl(struct DragsterConfig* config, int dragsterSlaveSelectMask);
    void sendDragsterRegisterValue(unsigned char address, unsigned char value);
    unsigned char readDragsterRegisterValue(unsigned char address);
    void beginDragsterConfigTransaction();
    void endDragsterConfigTransaction();
private:
    // vdma entities
    XAxiVdma _vdma1;
    XAxiVdma _vdma2;
    XScuGic _interruptController;
    // dragster entities
    XSpi _spi;
    struct ImageCaptureState _imageCaptureState;
    struct DragsterConfig _linescanner0Config;
    struct DragsterConfig _linescanner1Config;
};


#endif /* SRC_MANAGERS_IMAGECAPTUREMANAGER_H_ */
