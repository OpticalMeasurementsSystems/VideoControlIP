#include "imageCaptureManager.h"
#include "axiIoHelper.h"
#include "modulesMap.h"
#include "globalDefs.h"
#include "callbacks.h"

#define SPI_DEVICE_ID XPAR_SPI_0_DEVICE_ID

#define START_COMMAND 1
#define STOP_COMMAND 2

#define LINESCANNER0_SLAVE_SELECT 1
#define LINESCANNER1_SLAVE_SELECT 2

#define VDMA_1_DEVICE_ID XPAR_AXI_VDMA_0_DEVICE_ID
#define VDMA_2_DEVICE_ID XPAR_AXI_VDMA_1_DEVICE_ID

#define INTERRUPT_CONTROLLER_DEVICE_ID XPAR_SCUGIC_SINGLE_DEVICE_ID

#define VDMA_1_WRITE_INTERRUPT_ID XPAR_FABRIC_AXI_VDMA_0_S2MM_INTROUT_INTR
#define VDMA_2_WRITE_INTERRUPT_ID XPAR_FABRIC_AXI_VDMA_1_S2MM_INTROUT_INTR

u8 readBuffer[2];
u8 writeBuffer[2];

/* Callbacks */
static void Vdma1WriteCallback(void* callbackRef, u32 mask)
{
    // Do something with a frame produced by first VDMA
    u8* frameBuffer = (u8*)VDMA_1_MEMORY_BASE_ADDRESS;
    copyVdma(0, frameBuffer, mask);
}

static void Vdma2WriteCallback(void* callbackRef, u32 mask)
{
    // Do something with a frame produced by second VDMA
    u8* frameBuffer = (u8*)VDMA_2_MEMORY_BASE_ADDRESS;
    copyVdma(1, frameBuffer, mask);
}

static void Vdma1WriteErrorCallback(void* callbackRef, u32 mask){}
static void Vdma2WriteErrorCallback(void* callbackRef, u32 mask){}

void ImageCaptureManager::initialize()
{
    initializeVdmaDevices();
    initializeSpi();
    initializeDragsters();
}

void ImageCaptureManager::startImageCapture()
{
    startVdmaTransfer();

    //Xil_Out32(IMAGE_CAPTURE_MANAGER_BASE_ADDRESS, 1);
    write(IMAGE_CAPTURE_MANAGER_BASE_ADDRESS, 0, START_COMMAND);
    xil_printf("\n Image Capture Manager has been started\n\r");
}

void ImageCaptureManager::stopImageCapture()
{
    stopVdmaTransfer();

    //Xil_Out32(IMAGE_CAPTURE_MANAGER_BASE_ADDRESS, 2);
    write(IMAGE_CAPTURE_MANAGER_BASE_ADDRESS, 0, STOP_COMMAND);
    xil_printf("\n Image Capture Manager has been stopped\n\r");
}

struct DragsterConfig ImageCaptureManager::getDragsterConfig(unsigned char linescannerIndex)
{
    struct DragsterConfig config;
    readDragsterConfigImpl(&config, linescannerIndex == LINESCANNER0 ? LINESCANNER0_SLAVE_SELECT
                                                                     : LINESCANNER1_SLAVE_SELECT);
    return config;
}

void ImageCaptureManager::initializeVdmaDevices()
{
    setupVdmaDevice(&_vdma1, VDMA_1_DEVICE_ID, VDMA_1_MEMORY_BASE_ADDRESS);
    setupVdmaDevice(&_vdma2, VDMA_2_DEVICE_ID, VDMA_2_MEMORY_BASE_ADDRESS);
    configureVdmaInterrupts();
}

void ImageCaptureManager::setupVdmaDevice(XAxiVdma* vdma, u16 deviceId, u32 memoryBaseAddress)
{
    /* Acquire a device configuration. */
    XAxiVdma_Config* config = XAxiVdma_LookupConfig(deviceId);
    if(!config)
        xil_printf("\n XAxiVdma_LookupConfig failed\n\r");

    /* Initialize a device. */
    int status = XAxiVdma_CfgInitialize(vdma, config, config->BaseAddress);
    if (status != XST_SUCCESS)
        xil_printf("\n XAxiVdma_CfgInitialize failed\r\n");

    /* Create a channel configuration. */
    XAxiVdma_DmaSetup setup;

    /* Width(in bytes). Set this parameter to 1024 as DR-2k-7LCC
     * has 1x2048 pixels, we use half, and the size of each pixel is 1 byte. */
    setup.HoriSizeInput = DRAGSTER_LINE_LENGTH;

    /* Height(in lines). In our case it is always 1. */
    setup.VertSizeInput = 1;

    /* Stride. Specifies the number of bytes between
     * the first pixels of each horizontal line. */
    setup.Stride = DRAGSTER_LINE_LENGTH;

    /* Circular buffer mode. We most definitely want it to be enabled. */
    setup.EnableCircularBuf = 1;

    /* Frame delay. Set it to 0 for now. */
    setup.FrameDelay = 0;

    // Gen-Lock parameters. Set them to 0 for now.
    setup.EnableSync = 0;
    setup.PointNum = 0;

    // Frame counter. Set it to 0 as we dont need it so far.
    setup.EnableFrameCounter = 0;

    // Fixed frame store address. Used for parking.
    setup.FixedFrameStoreAddr = 0;

    /* Configure VDMA write channel. */
    status = XAxiVdma_DmaConfig(vdma, XAXIVDMA_WRITE, &setup);
    if (status != XST_SUCCESS)
        xil_printf("\n XAxiVdma_DmaConfig failed \r\n");

    /* Set start address for 1 buffer. */
    *setup.FrameStoreStartAddr = memoryBaseAddress;

    /* Function which actually configures buffer addresses per channel.
     * I assume it can be done earlier. I mean I believe XAxiVdma_DmaConfig
     * can configure buffer addresses too, but unfortunately I cannot test it so far.*/
    status = XAxiVdma_DmaSetBufferAddr(vdma, XAXIVDMA_WRITE, setup.FrameStoreStartAddr);
    if (status != XST_SUCCESS)
        xil_printf("\n XAxiVdma_DmaSetBufferAddr failed \r\n");
}

void ImageCaptureManager::configureVdmaInterrupts()
{
    XScuGic_Config* config = XScuGic_LookupConfig(INTERRUPT_CONTROLLER_DEVICE_ID);
    if(!config)
        xil_printf("\n XScuGic_LookupConfig failed \r\n");

    int status = XScuGic_CfgInitialize(&_interruptController, config, config->CpuBaseAddress);
    if (status != XST_SUCCESS)
        xil_printf("\n XScuGic_CfgInitialize \r\n");

    /* Set priority and trigger type.
     * Priority range: 0...248.
     * Trigger types:
     *     Software-generated Interrupts(SFI): 2(always);
     *     Private Peripheral Interrupt(PPI): 1(Active HIGH), 3(Rising edge);
     *     Shared Peripheral Interrupts(SPI): 1(Active HIGH), 3(Rising edge);*/

    XScuGic_SetPriorityTriggerType(&_interruptController, VDMA_1_WRITE_INTERRUPT_ID, 0xA0, 0x3);
    XScuGic_SetPriorityTriggerType(&_interruptController, VDMA_2_WRITE_INTERRUPT_ID, 0xA0, 0x3);

    status = XScuGic_Connect(
            &_interruptController,
            VDMA_1_WRITE_INTERRUPT_ID,
            (Xil_InterruptHandler)XAxiVdma_WriteIntrHandler,
            &_vdma1);

    if (status != XST_SUCCESS)
        xil_printf("\n XScuGic_Connect failed (VDMA 1) \r\n");

    status = XScuGic_Connect(
            &_interruptController,
            VDMA_2_WRITE_INTERRUPT_ID,
            (Xil_InterruptHandler)XAxiVdma_WriteIntrHandler,
            &_vdma2);

    if (status != XST_SUCCESS)
        xil_printf("\n XScuGic_Connect failed (VDMA 2) \r\n");

    XScuGic_Enable(&_interruptController, VDMA_1_WRITE_INTERRUPT_ID);
    XScuGic_Enable(&_interruptController, VDMA_2_WRITE_INTERRUPT_ID);

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(
            XIL_EXCEPTION_ID_IRQ_INT,
            (Xil_ExceptionHandler)XScuGic_InterruptHandler,
            &_interruptController);

    Xil_ExceptionEnable();

    XAxiVdma_SetCallBack(&_vdma1, XAXIVDMA_HANDLER_GENERAL, (void*)Vdma1WriteCallback, (void*)&_vdma1, XAXIVDMA_WRITE);
    XAxiVdma_SetCallBack(&_vdma2, XAXIVDMA_HANDLER_GENERAL, (void*)Vdma2WriteCallback, (void*)&_vdma2, XAXIVDMA_WRITE);

    XAxiVdma_SetCallBack(&_vdma1, XAXIVDMA_HANDLER_ERROR, (void*)Vdma1WriteErrorCallback, (void*)&_vdma1, XAXIVDMA_WRITE);
    XAxiVdma_SetCallBack(&_vdma2, XAXIVDMA_HANDLER_ERROR, (void*)Vdma2WriteErrorCallback, (void*)&_vdma2, XAXIVDMA_WRITE);

    XAxiVdma_IntrEnable(&_vdma1, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_WRITE);
    XAxiVdma_IntrEnable(&_vdma2, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_WRITE);
}

void ImageCaptureManager::startVdmaTransfer()
{
    int status = XAxiVdma_DmaStart(&_vdma1, XAXIVDMA_WRITE);
    if (status != XST_SUCCESS)
        xil_printf("\n StartTransfer failed(VDMA 1) \r\n");

    status = XAxiVdma_DmaStart(&_vdma2, XAXIVDMA_WRITE);
        if (status != XST_SUCCESS)
            xil_printf("\n StartTransfer failed(VDMA 2) \r\n");
}

void ImageCaptureManager::stopVdmaTransfer()
{
    XAxiVdma_DmaStop(&_vdma1, XAXIVDMA_WRITE);
    XAxiVdma_DmaStop(&_vdma2, XAXIVDMA_WRITE);
}

/* ������������� SPI � ����������� ������ (polling mode)*/
void ImageCaptureManager::initializeSpi()
{
    int status;

    /* ����������� ������������ ���������� */
    XSpi_Config* spiConfig = XSpi_LookupConfig(SPI_DEVICE_ID);
    if(!spiConfig)
        xil_printf("\n XSpi_LookupConfig Failed\n\r");
    _spi.NumSlaveBits = 2;
    _spi.SpiMode = XSP_STANDARD_MODE;

    /* �������������� ��������� SPI */
    status = XSpi_CfgInitialize(&_spi, spiConfig, spiConfig->BaseAddress);
    if(status != XST_SUCCESS)
        xil_printf("\n XSpi_CfgInitialize Failed\n\r");

    /* �� ��������� SPI �������� Slave, ����� ���� ������������� ��� ��� Master */
     status = XSpi_SetOptions(&_spi, XSP_MASTER_OPTION);
    if(status != XST_SUCCESS)
        xil_printf("\n XSpi_SetOptions Failed\n\r");

    /* ��������� SPI */

    XSpi_Start(&_spi);

    /* ������������ SPI ���������� */
    XSpi_IntrGlobalDisable(&_spi);
}

void ImageCaptureManager::initializeDragsters()
{
    // dragster0 config
    _linescanner0Config.setControlRegister1(0xA9);
    _linescanner0Config.setControlRegister2(0x32);
    _linescanner0Config.setControlRegister3(0x13);
    _linescanner0Config.setEndOfRangeRegister(0x08);
    // dragster1 config
    _linescanner1Config.setControlRegister1(0xA9);
    _linescanner1Config.setControlRegister2(0x32);
    _linescanner1Config.setControlRegister3(0x13);
    _linescanner1Config.setEndOfRangeRegister(0x08);
    initializeDragsterImpl(&_linescanner0Config, LINESCANNER0_SLAVE_SELECT);
    initializeDragsterImpl(&_linescanner1Config, LINESCANNER1_SLAVE_SELECT);
    // deselecting any slave
    XSpi_SetSlaveSelect(&_spi, 0);
}

void ImageCaptureManager::readDragsterConfigImpl(struct DragsterConfig* config, int dragsterSlaveSelectMask)
{
    int status = XSpi_SetSlaveSelect(&_spi, dragsterSlaveSelectMask);
    if(status == XST_SUCCESS)
    {
        config->setControlRegister1(readDragsterRegisterValue(1));
        config->setControlRegister2(readDragsterRegisterValue(2));
        config->setControlRegister3(readDragsterRegisterValue(3));
    }
}

void ImageCaptureManager::initializeDragsterImpl(struct DragsterConfig* config, int dragsterSlaveSelectMask)
{
    int status = XSpi_SetSlaveSelect(&_spi, dragsterSlaveSelectMask);
    if(status == XST_SUCCESS)
    {
        // todo: umv: read from fields
        beginDragsterConfigTransaction();
        // ������ ������������ ��������� Dragster
        // CONTROL Register 2
        sendDragsterRegisterValue(CONTROL_REGISTER_2_ADDRESS, config->getControlRegister2()._registerValue);
        // CONTROL Register 3
        sendDragsterRegisterValue(CONTROL_REGISTER_3_ADDRESS, config->getControlRegister3()._registerValue);
        // End of Data Register
        sendDragsterRegisterValue(END_OF_RANGE_REGISTER_ADDRESS, config->getEndOfRangeRegister());  // 8 bit pixels value
        // CONTROL Register 1
        sendDragsterRegisterValue(CONTROL_REGISTER_1_ADDRESS, config->getControlRegister1()._registerValue);
        // 0 byte (must generate at least 3 clk before SS is disabled)
        endDragsterConfigTransaction();
    }
}

void ImageCaptureManager::sendDragsterRegisterValue(unsigned char address, unsigned char value)
{
    unsigned char convertedAddress = convertFromMsbToLsbFirst(address);
    writeBuffer[1] = convertedAddress;
    unsigned char convertedValue = convertFromMsbToLsbFirst(value);
    writeBuffer[0] = convertedValue;
    XSpi_Transfer(&_spi, writeBuffer, NULL, 2);
}

unsigned char ImageCaptureManager::readDragsterRegisterValue(unsigned char address)
{
    #define READ_REGISTER_ADDRESS 0xF
    writeBuffer[0] = convertFromMsbToLsbFirst(address);
    writeBuffer[0] = convertFromMsbToLsbFirst(READ_REGISTER_ADDRESS);
    XSpi_Transfer(&_spi, writeBuffer, readBuffer, 2);
    return convertFromLsbToMsbFirst(readBuffer[0]);
}

void ImageCaptureManager::beginDragsterConfigTransaction()
{

}

void ImageCaptureManager::endDragsterConfigTransaction()
{
    writeBuffer[0] = 0;
    XSpi_Transfer(&_spi, writeBuffer, NULL, 1);
}
